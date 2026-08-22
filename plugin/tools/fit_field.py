from __future__ import annotations

import ctypes
import math
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))

from arma_measure_lib import RT_DOUBLES  # noqa: E402
from arma_measure_lib import (lib, load_wav, harmonic_envelope,          # noqa: E402
                              averaged_spectrum_db, fit_arma, pack_and_certify)

NAME = sys.argv[1]
WAV = Path(sys.argv[2])
RATE = float(sys.argv[3]) if len(sys.argv) > 3 else 48_000.0
ITERS = int(sys.argv[4]) if len(sys.argv) > 4 else 4000
N_FRAMES = 17
GRID = np.geomspace(120.0, 8000.0, 96)

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int
lib.trench_stage_roots_from_words_at.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_double,
    ctypes.POINTER(ctypes.c_double)]
lib.trench_stage_roots_from_words_at.restype = ctypes.c_int

_Z1 = np.exp(-1j * 2.0 * np.pi * GRID / RATE)
_Z2 = _Z1 * _Z1

def frame_targets(x: np.ndarray, sr: float) -> list[np.ndarray]:
    hop = len(x) // N_FRAMES
    win = min(len(x), hop * 2)
    targets = []
    for i in range(N_FRAMES):
        start = min(i * hop, len(x) - win)
        seg = x[start:start + win]
        try:
            f0, hf, hdb = harmonic_envelope(seg, sr, fmax=10_000.0)
            if len(hf) < 8:
                raise ValueError
            src_f, src_db = hf, hdb
        except Exception:
            f, db = averaged_spectrum_db(seg, sr)
            keep = (f > 60.0) & (f < 12_000.0)
            src_f, src_db = f[keep], db[keep]
        dbs = np.interp(np.log(GRID), np.log(src_f), src_db)
        k = np.hanning(9); k /= k.sum()
        dbs = np.convolve(dbs, k, mode="same")
        targets.append(dbs - dbs.mean())
    return targets

def probe_shape(body: bytes, m: float) -> tuple[np.ndarray, float]:
    coeffs = (ctypes.c_double * RT_DOUBLES)()
    max_r = ctypes.c_double()
    unstable = ctypes.c_uint32()
    nonfinite = ctypes.c_uint32()
    buf = ctypes.create_string_buffer(body, 240)
    if lib.trench_packed_probe_at(buf, 240, m, 0.0, RATE, coeffs,
                                  ctypes.byref(max_r), ctypes.byref(unstable),
                                  ctypes.byref(nonfinite)) != 0 \
       or unstable.value or nonfinite.value:
        return np.zeros_like(GRID), 2.0
    c = np.ctypeslib.as_array(coeffs).reshape(6, 5)
    h = np.ones_like(_Z1)
    for b0, b1, b2, a1, a2 in c:
        h = h * (b0 + b1 * _Z1 + b2 * _Z2) / (1.0 + a1 * _Z1 + a2 * _Z2)
    db = 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))
    return db - db.mean(), max_r.value

def pack(words120: list[int]) -> bytes | None:
    out = ctypes.create_string_buffer(240)
    if lib.trench_pack_body_from_corner_words(
            (ctypes.c_uint16 * 120)(*words120), 120, out) != 0:
        return None
    return out.raw

def rp_db(r):
    return -20.0 * math.log10(max(1e-12, 1.0 - min(r, 1.0 - 1e-12)))

def r_from_rp(db):
    return 1.0 - 10.0 ** (-db / 20.0)

def push_law(corner30: list[int]) -> list[int]:
    rows = []
    radii = []
    for s in range(6):
        w = (ctypes.c_uint16 * 5)(*corner30[s * 5:s * 5 + 5])
        out = (ctypes.c_double * 5)()
        if lib.trench_stage_roots_from_words_at(w, RATE, out) != 0:
            rows.append(None); radii.append(-1.0)
        else:
            rows.append(list(out)); radii.append(out[1])
    hottest = int(np.argmax(radii))
    pushed = []
    for s in range(6):
        if rows[s] is None or rows[s][1] <= 0.0:
            pushed.extend(corner30[s * 5:s * 5 + 5])
            continue
        r5 = rows[s].copy()
        p_db = rp_db(r5[1])
        factor = 0.2 if s == hottest else 0.4
        r5[1] = r_from_rp(p_db + factor * (66.0 - p_db))
        enc = (ctypes.c_uint16 * 5)()
        if lib.trench_stage_words_from_roots_at((ctypes.c_double * 5)(*r5), RATE, enc) != 0:
            pushed.extend(corner30[s * 5:s * 5 + 5])
        else:
            pushed.extend(enc)
    return pushed

def main():
    x, sr = load_wav(WAV)
    targets = frame_targets(x, sr)
    ms = np.linspace(0.0, 1.0, N_FRAMES)

    hop = len(x) // N_FRAMES
    win = min(len(x), hop * 2)
    def endpoint_words(seg):
        try:
            f0, hf, hdb = harmonic_envelope(seg, sr, fmax=10_000.0)
            assert len(hf) >= 32
            f, db = hf, hdb
        except Exception:
            fa, dba = averaged_spectrum_db(seg, sr)
            keep = (fa > 60.0) & (fa < 12_000.0)
            f, db = fa[keep], dba[keep]
        _, words6, _ = fit_arma(np.asarray(f, float), np.asarray(db, float) - float(np.mean(db)), RATE)
        return [w for row in words6 for w in row]
    def endpoint_words_robust(segments):
        last_err = None
        for seg in segments:
            try:
                return endpoint_words(seg)
            except Exception as e:
                last_err = e
        raise last_err
    A = endpoint_words_robust([x[i * hop: i * hop + win] for i in range(8)])
    B = endpoint_words_robust([x[len(x) - win - i * hop: len(x) - i * hop] for i in range(8)])

    def full(a, b):
        return a + b + push_law(a) + push_law(b)

    def loss(a, b) -> float:
        body = pack(full(a, b))
        if body is None:
            return 1e9
        total = 0.0
        for m, t in zip(ms, targets):
            shape, max_r = probe_shape(body, float(m))
            if max_r >= 1.0:
                return 1e9
            total += float(np.mean((shape - t) ** 2))
            if max_r > 0.9975:
                total += (max_r - 0.9975) * 4e4
        return total / len(ms)

    rng = np.random.default_rng(240)
    cur = A + B
    cur_loss = loss(cur[:30], cur[30:])
    seed_loss = cur_loss
    print(f"{NAME}: seed field error {math.sqrt(seed_loss):.2f} dB rms over {N_FRAMES} frames")

    steps = np.array([8192, 2048, 512, 128])
    since_best = 0
    for it in range(ITERS):
        idx = int(rng.integers(0, 60))
        delta = int(rng.choice(steps)) * (1 if rng.random() < 0.5 else -1)
        trial = cur.copy()
        trial[idx] = int(np.clip(trial[idx] + delta, 0, 65535))
        if trial[idx] == cur[idx]:
            continue
        t_loss = loss(trial[:30], trial[30:])
        if t_loss < cur_loss:
            cur, cur_loss = trial, t_loss
            since_best = 0
        else:
            since_best += 1
        if it % 500 == 499:
            print(f"  iter {it + 1}: field error {math.sqrt(cur_loss):.2f} dB rms")

    body, max_r = pack_and_certify(full(cur[:30], cur[30:]))
    out = ROOT / "bodies" / "candidates" / f"{NAME}.body240"
    out.write_bytes(body)
    print(f"fitted: {math.sqrt(seed_loss):.2f} -> {math.sqrt(cur_loss):.2f} dB rms"
          f"   certified: hottest pole {max_r:.6f}  ->  {out.name}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fitted = np.array([probe_shape(body, float(m))[0] for m in ms])
    tgt = np.array(targets)
    fig = plt.figure(figsize=(14, 9))
    for i, (data, title) in enumerate ([(tgt, "the gesture (measured frames)"),
                                        (fitted, "the fitted ride (packed runtime)")]):
        ax = fig.add_subplot(2, 2, i + 1)
        ax.imshow(data.T, origin="lower", aspect="auto", cmap="magma",
                  vmin=-30, vmax=30,
                  extent=[0, 1, math.log10(GRID[0]), math.log10(GRID[-1])])
        ax.set_title(title, fontsize=10)
        ax.set_xlabel("MORPH")
        ax.set_ylabel("log10 Hz")
    ax = fig.add_subplot(2, 1, 2)
    for m, t in zip(ms[::4], targets[::4]):
        ax.semilogx(GRID, t, "--", color="0.5", lw=1.0)
        shape, _ = probe_shape(body, float(m))
        ax.semilogx(GRID, shape, color="#c96a54", lw=1.4)
    ax.set_ylim(-40, 40)
    ax.set_title("five wheel positions: dashed = the recording, coral = the body", fontsize=10)
    ax.grid(alpha=0.3)
    fig.suptitle(f"{NAME} - the ride is the fitted object", fontsize=12)
    fig.tight_layout()
    fig.savefig(out.with_name(f"{NAME}_field.png"), dpi=110)
    print(f"plate: {NAME}_field.png")

main()
