from __future__ import annotations
import json, math, sys
from pathlib import Path
import numpy as np

FREQS = np.geomspace(30.0, 19200.0, 512)

ZERO_R_MAX = 0.999

def ir_to_tf(wav_path, smooth_oct=1 / 6, detilt=False):
    from scipy.io import wavfile
    sr, x = wavfile.read(wav_path)
    x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    x /= (np.max(np.abs(x)) + 1e-12)
    n = int(2 ** np.ceil(np.log2(len(x))))
    H = np.abs(np.fft.rfft(x, n))
    f = np.fft.rfftfreq(n, 1 / sr)
    mag = np.interp(FREQS, f, H)
    lo = np.log2(np.maximum(FREQS, 1.0))
    sm = np.empty_like(mag)
    for i, c in enumerate(lo):
        w = np.abs(lo - c) <= smooth_oct / 2
        sm[i] = np.sqrt(np.mean(mag[w] ** 2))
    db = 20 * np.log10(np.maximum(sm, 1e-9))
    if detilt:
        trend = np.empty_like(db)
        for i, c in enumerate(lo):
            w = np.abs(lo - c) <= 1.0
            trend[i] = np.mean(db[w])
        db -= trend
    db -= np.median(db)
    return {"freqs_hz": FREQS.tolist(), "mag_db": db.tolist(),
            "source": str(wav_path), "kind": "ir"}

def modes_to_tf(modes, source="modal-table"):
    mag = np.zeros_like(FREQS)
    for m in modes:
        f0 = float(m["freq_hz"])
        bw = float(m["bw_hz"]) if "bw_hz" in m else 2.2 / float(m["t60_s"])
        amp = 10 ** (float(m.get("level_db", 0.0)) / 20)
        mag += amp / np.sqrt(1.0 + ((FREQS ** 2 - f0 ** 2) / np.maximum(FREQS * bw, 1e-9)) ** 2)
    db = 20 * np.log10(np.maximum(mag, 1e-9))
    db -= np.median(db)
    return {"freqs_hz": FREQS.tolist(), "mag_db": db.tolist(),
            "source": source, "kind": "modes"}

SR_RUNTIME = 39062.5

def _load_ir(wav_path, sr_target):
    from scipy.io import wavfile
    from scipy.signal import resample_poly
    from fractions import Fraction
    sr, x = wavfile.read(wav_path)
    x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    x /= (np.max(np.abs(x)) + 1e-12)
    fr = Fraction(sr_target / sr).limit_denominator(10000)
    return resample_poly(x, fr.numerator, fr.denominator)

def prony_poles(x, sr, n_modes, lp_order=48, n_fit=8192, f_lo=60.0, f_hi=12000.0):
    from scipy.linalg import lstsq as _lstsq
    y = x[:n_fit]
    p = lp_order
    A = np.column_stack([y[p - 1 - k: len(y) - 1 - k] for k in range(p)])
    a = _lstsq(A, y[p:])[0]
    roots = np.roots(np.concatenate([[1.0], -a]))
    cand = [z for z in roots if z.imag > 1e-9 and abs(z) < 1.0
            and f_lo <= abs(np.angle(z)) / (2 * math.pi) * sr <= f_hi]
    if not cand:
        return []
    n = np.arange(len(y))
    E = np.column_stack([z ** n for z in cand])
    B = np.column_stack([E.real, E.imag])
    c = _lstsq(B, y)[0]
    res = np.abs(c[:len(cand)] + 1j * c[len(cand):])
    energy = res / np.maximum(1.0 - np.abs(cand), 1e-6)
    order = np.argsort(energy)[::-1][:n_modes]
    return sorted((abs(np.angle(cand[i])) / (2 * math.pi) * sr, abs(cand[i]))
                  for i in order)

def _select_modes(x, sr, n_rows, f_lo, f_hi):
    from scipy.signal import find_peaks
    cand = prony_poles(x, sr, 3 * n_rows, f_lo=f_lo, f_hi=f_hi)
    if len(cand) <= n_rows:
        return cand
    nfft = int(2 ** np.ceil(np.log2(len(x))))
    db = 20 * np.log10(np.abs(np.fft.rfft(x, nfft)) + 1e-12)
    f = np.fft.rfftfreq(nfft, 1.0 / sr)
    grid = np.geomspace(f_lo, f_hi, 400)
    gdb = np.interp(grid, f, db)
    k = 9
    gdb = np.convolve(gdb, np.ones(k) / k, mode="same")
    pk, props = find_peaks(gdb, prominence=3.0)
    order = pk[np.argsort(gdb[pk])[::-1]]
    chosen, used = [], set()
    for i in order:
        if len(chosen) >= n_rows:
            break
        dists = [abs(math.log2(h / grid[i])) for h, _ in cand]
        j = int(np.argmin(dists))
        if dists[j] <= 1 / 3 and j not in used:
            used.add(j); chosen.append(cand[j])
    for j, c in enumerate(cand):
        if len(chosen) >= n_rows:
            break
        if j not in used:
            used.add(j); chosen.append(c)
    return sorted(chosen[:n_rows])

def ir_to_rows(wav_path, n_rows=6, f_lo=100.0, f_hi=10000.0, sr=SR_RUNTIME,
               n_grid=600):
    import sys as _s
    _root = Path(__file__).resolve().parent.parent
    _s.path.insert(0, str(_root / "filters" / "rails"))
    from dvtd_rails import eval_tf, roots_features, floored_db, band_weight
    x = _load_ir(wav_path, sr)
    onset = int(np.argmax(np.abs(x)))
    x = x[onset:]
    poles = _select_modes(x, sr, n_rows, f_lo, f_hi)
    pz = []
    for hz, r in poles:
        z = r * np.exp(1j * 2 * math.pi * hz / sr)
        pz += [z, np.conj(z)]
    a_full = np.real(np.poly(pz))
    nfft = int(2 ** np.ceil(np.log2(len(x)) + 1))
    Hfull = np.fft.rfft(x, nfft)
    ffull = np.fft.rfftfreq(nfft, 1.0 / sr)
    grid = np.geomspace(f_lo, f_hi, n_grid)
    Hg = np.interp(grid, ffull, Hfull.real) + 1j * np.interp(grid, ffull, Hfull.imag)
    omega = 2 * math.pi * grid / sr
    zinv = np.exp(-1j * omega)
    Aw = np.polyval(a_full[::-1], zinv)
    M = 2 * n_rows
    Zb = np.vander(zinv, M + 1, increasing=True)
    w = (band_weight(grid) / np.maximum(np.abs(Aw), 1e-12))[:, None]
    Phi = Zb * w
    y = (Hg * Aw) * w[:, 0]
    Ar = np.vstack([Phi.real, Phi.imag])
    yr = np.concatenate([y.real, y.imag])
    b = np.linalg.lstsq(Ar, yr, rcond=None)[0]
    Hfit = eval_tf(b, a_full, omega)
    err = floored_db(Hfit) - floored_db(Hg)
    rms = float(np.sqrt(np.mean(err ** 2)))

    def _pairs(feats):
        cx = [f for f in feats if 1e-6 < f["angle_rad"] < math.pi - 1e-6]
        re = [f for f in feats if f not in cx]
        pairs = [(f["hz"], f["r"]) for f in cx]
        for i in range(0, len(re) - 1, 2):
            pairs.append(((re[i]["hz"] + re[i + 1]["hz"]) / 2,
                          math.sqrt(abs(re[i]["r"] * re[i + 1]["r"]))))
        return sorted(pairs)

    zcand = _pairs(roots_features(b[::-1], sr))

    def _cascade_db(zsub, psub=None):
        Hc = np.ones_like(grid, dtype=complex)
        for hz, r in (psub if psub is not None else poles):
            p = r * np.exp(1j * 2 * math.pi * hz / sr)
            Hc /= (1 - p * zinv) * (1 - np.conj(p) * zinv)
        for hz, r in zsub:
            z = r * np.exp(1j * 2 * math.pi * hz / sr)
            Hc *= (1 - z * zinv) * (1 - np.conj(z) * zinv)
        return Hc

    from itertools import combinations
    from scipy.optimize import least_squares
    tdb = floored_db(Hg)
    tdb -= np.median(tdb)
    best_sub, best_rms = zcand[:n_rows], np.inf
    for sub in combinations(zcand, min(n_rows, len(zcand))):
        cdb = floored_db(_cascade_db(sub))
        e = (cdb - np.median(cdb)) - tdb
        srms = float(np.sqrt(np.mean(e ** 2)))
        if srms < best_rms:
            best_rms, best_sub = srms, list(sub)
    while len(best_sub) < n_rows:
        best_sub.append((f_hi, 0.0))
    from scipy.signal import find_peaks
    nt, _ = find_peaks(-tdb, prominence=6.0)
    nt = nt[np.argsort(tdb[nt])][:n_rows]
    seed_b = [(float(grid[i]), 0.98) for i in nt]
    while len(seed_b) < n_rows:
        seed_b.append(best_sub[len(seed_b)])
    seeds = [best_sub, seed_b]
    wn = np.where(tdb < -6.0, 3.0, 1.0)

    def _resid(v):
        zs = [(math.exp(v[i]), v[n_rows + i]) for i in range(n_rows)]
        ps = [(poles[i][0], v[2 * n_rows + i]) for i in range(len(poles))]
        cdb = floored_db(_cascade_db(zs, ps))
        return ((cdb - np.median(cdb)) - tdb) * wn

    npole = len(poles)
    bounds = (np.concatenate([np.full(n_rows, math.log(f_lo * 0.8)), np.zeros(n_rows),
                              np.full(npole, 0.5)]),
              np.concatenate([np.full(n_rows, math.log(f_hi)), np.full(n_rows, ZERO_R_MAX),
                              np.full(npole, 0.999)]))
    pr0 = [min(r, 0.999) for _, r in poles]
    lo_hz, hi_hz = f_lo * 0.8, f_hi
    sol, sol_cost = None, np.inf
    for seed in seeds:
        x0 = np.concatenate([[math.log(min(max(h, lo_hz), hi_hz)) for h, _ in seed],
                             [min(r, ZERO_R_MAX) for _, r in seed], pr0])
        s = least_squares(_resid, x0, method="trf", bounds=bounds)
        if s.cost < sol_cost:
            sol, sol_cost = s, s.cost
    zeros = sorted((math.exp(sol.x[i]), float(sol.x[n_rows + i])) for i in range(n_rows))
    poles = [(poles[i][0], float(sol.x[2 * n_rows + i])) for i in range(npole)]
    best_rms = float(np.sqrt(np.mean(_resid(sol.x) ** 2)))
    Hc = _cascade_db(zeros)
    scale_row_db = float(np.median(20 * np.log10(np.abs(Hg) + 1e-12))
                         - np.median(20 * np.log10(np.abs(Hc) + 1e-12))) / n_rows
    rows = []
    for i in range(n_rows):
        ph, pr = poles[i] if i < len(poles) else (0.0, 0.0)
        zh, zr = zeros[i] if i < len(zeros) else (0.0, 0.0)
        rows.append({"pole": {"hz": ph, "r": min(pr, 0.999)},
                     "zero": {"hz": zh, "r": zr},
                     "scale_db": scale_row_db})
    report = {"source": str(wav_path), "sr": sr, "fit_band_hz": [f_lo, f_hi],
              "mag_rms_floored_db": rms, "rows_rms_floored_db": best_rms,
              "max_pole_r": max((r for _, r in poles), default=0.0),
              "poles": poles, "zeros": zeros}
    return rows, report

def main():
    if len(sys.argv) < 2:
        print("usage: tf_ingest.py <ir.wav | modes.json> [out.json]\n"
              "       tf_ingest.py --rows <ir.wav> [out.rows.json]"); sys.exit(1)
    if sys.argv[1] == "--rows":
        p = Path(sys.argv[2])
        rows, rep = ir_to_rows(p)
        out = Path(sys.argv[3]) if len(sys.argv) > 3 else p.with_suffix(".rows.json")
        out.write_text(json.dumps({"rows": rows, "report": rep}, indent=1))
        print(f"{p.name} -> {out.name}  rows_rms {rep['rows_rms_floored_db']:.2f} dB "
              f"(notch-weighted), maxPoleR {rep['max_pole_r']:.4f}")
        for r in rows:
            print(f"  pole {r['pole']['hz']:7.1f} Hz r {r['pole']['r']:.4f}   "
                  f"zero {r['zero']['hz']:7.1f} Hz r {r['zero']['r']:.4f}   "
                  f"scale {r['scale_db']:+.2f} dB")
        return
    p = Path(sys.argv[1])
    if p.suffix.lower() == ".json":
        tf = modes_to_tf(json.loads(p.read_text())["modes"], source=str(p))
    else:
        tf = ir_to_tf(p)
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else p.with_suffix(".tf.json")
    out.write_text(json.dumps(tf))
    db = np.array(tf["mag_db"])
    print(f"{p.name} -> {out.name}  ({tf['kind']}, crown {db.max():+.1f} dB @ "
          f"{FREQS[int(np.argmax(db))]:.0f} Hz, floor 0 dB median)")

if __name__ == "__main__":
    main()
