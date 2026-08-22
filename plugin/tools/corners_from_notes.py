from __future__ import annotations

import ctypes
import math
import sys
from pathlib import Path

import numpy as np
from scipy.signal import find_peaks

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))

from arma_measure_lib import (lib, load_wav, averaged_spectrum_db,   # noqa: E402
                              fit_arma, roots_response_db, pack_and_certify,
                              trim_to_p95)

NAME = sys.argv[1]
WAV = Path(sys.argv[2])
RATE = float(sys.argv[3]) if len(sys.argv) > 3 else 48_000.0

NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

def note_name(hz: float) -> str:
    midi = 12.0 * math.log2(hz / 440.0) + 69.0
    n = round(midi)
    return f"{NOTE[n % 12]}{n // 12 - 1}"

def four_rings(x: np.ndarray, sr: float) -> list[np.ndarray]:
    hop = int(sr * 0.01)
    frames = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2) + 1e-12)
                       for i in range(0, len(x) - hop, hop)])
    db = 20.0 * np.log10(frames / frames.max())
    rise = np.diff(db, prepend=db[0])
    peaks, props = find_peaks(rise, height=6.0, distance=int(0.4 / 0.01))
    head = [0] if db[:5].max() > -20.0 else []
    later = [int(p) for p in peaks[np.argsort(props["peak_heights"])[::-1]]
             if p * 0.01 > 0.3]
    strikes = sorted(head + later[: 4 - len(head)])
    if len(strikes) < 4:
        raise SystemExit(f"REFUSED: found only {len(strikes)} strikes - need 4")
    strongest = np.array(strikes)
    bounds = list(strongest) + [len(frames)]
    rings = []
    for a, b in zip(bounds[:-1], bounds[1:]):
        s = (a + 3) * hop
        e = min(b * hop, len(x))
        rings.append(x[s:e])
    return rings

def modal_envelope(seg: np.ndarray, sr: float):
    f, db = averaged_spectrum_db(seg, sr)
    keep = (f > 60.0) & (f < 12_000.0)
    f, db = f[keep], db[keep]
    pk, props = find_peaks(db, prominence=8.0, distance=8)
    if len(pk) < 6:
        pk, props = find_peaks(db, prominence=4.0, distance=8)
    order = np.argsort(props["prominences"])[::-1][:40]
    sel = np.sort(pk[order])
    pitch_hz = float(f[pk[np.argmax(db[pk])]] if len(pk) else f[np.argmax(db)])
    strong = [i for i in sel if db[i] > db[sel].max() - 30.0]
    if strong:
        pitch_hz = float(f[strong[0]])
    return f[sel], db[sel], pitch_hz

def main():
    x, sr = load_wav(WAV)
    rings = four_rings(x, sr)

    fitted = []
    for ring in rings:
        f, db, pitch = modal_envelope(ring, sr)
        roots6, words6, _ = fit_arma(np.asarray(f, float),
                                     np.asarray(db, float) - float(np.mean(db)), RATE)
        words, trimmed = trim_to_p95(roots6, 0.0, RATE)
        fitted.append((pitch, words, trimmed, (f, db)))

    fitted.sort(key=lambda t: t[0])
    names = [note_name(t[0]) for t in fitted]
    print(f"{NAME}: corners by pitch  "
          f"M0Q0={names[0]}  M100Q0={names[1]}  M0Q100={names[2]}  M100Q100={names[3]}")

    flat = [w for _, words, _, _ in fitted for w in words]
    body, max_r = pack_and_certify(flat)
    out = ROOT / "bodies" / "candidates" / f"{NAME}.body240"
    out.write_bytes(body)
    print(f"certified: hottest pole {max_r:.6f}  ->  {out.name}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    grid = np.geomspace(60.0, 12_000.0, 1024)
    fig, axes = plt.subplots(2, 2, figsize=(14, 9), sharex=True, sharey=True)
    labels = ["M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100"]
    for ax, (pitch, _, trimmed, (mf, mdb)), lab, nm in zip(
            axes.flat, fitted, labels, names):
        rdb = roots_response_db(trimmed, grid, RATE)
        ax.semilogx(mf, mdb - np.mean(mdb), "x", color="0.45", ms=4,
                    label="measured modes")
        ax.semilogx(grid, rdb - np.mean(rdb), color="#c96a54", lw=1.5,
                    label="the corner")
        ax.set_ylim(-60, 30)
        ax.set_title(f"{lab}  -  {nm}", fontsize=10)
        ax.grid(alpha=0.3)
    axes.flat[0].legend(fontsize=8)
    fig.suptitle(f"{NAME} - four notes, four corners", fontsize=12)
    fig.tight_layout()
    fig.savefig(out.with_name(f"{NAME}_corners.png"), dpi=110)
    print(f"plate: {NAME}_corners.png")

main()
