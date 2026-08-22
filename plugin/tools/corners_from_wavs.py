from __future__ import annotations

import ctypes
import sys
from pathlib import Path

SOURCE_WAV = None
if "--source" in sys.argv:
    i = sys.argv.index("--source")
    SOURCE_WAV = Path(sys.argv[i + 1])
    del sys.argv[i:i + 2]

import numpy as np
from scipy.signal import find_peaks

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))

from arma_measure_lib import (lib, load_wav, harmonic_envelope,      # noqa: E402
                              averaged_spectrum_db, fit_arma,
                              roots_response_db, pack_and_certify, trim_to_p95)
from joint_fit import certify as certify_bytes                       # noqa: E402
from joint_fit import joint_refine, surface_rms                      # noqa: E402

NAME = sys.argv[1]
WAVS = [Path(p) for p in sys.argv[2:6]]
RATE = float(sys.argv[6]) if len(sys.argv) > 6 else 48_000.0
LABELS = ["M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100"]

def envelope(path: Path):
    x, sr = load_wav(path)
    peak = float(np.abs(x).max())
    if peak > 1.0:
        print(f"  NOTE: {path.name} clipped (peak {peak:.2f}) - normalised, but a"
              " clean retake would measure truer")
        x = x / peak
    hop = int(sr * 0.01)
    fr = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2) + 1e-12)
                   for i in range(0, len(x) - hop, hop)])
    db = 20.0 * np.log10(fr / max(fr.max(), 1e-12))
    on = np.where(db > -40.0)[0]
    if len(on):
        x = x[on[0] * hop: (on[-1] + 1) * hop]
    try:
        f0, hf, hdb = harmonic_envelope(x, sr, fmax=12_000.0)
        keep = hf > 40.0
        hf, hdb = hf[keep], hdb[keep]
        if len(hf) < 32 or hf[-1] / hf[0] < 24.0:
            raise ValueError
        return np.asarray(hf, float), np.asarray(hdb, float)
    except Exception:
        f, dbs = averaged_spectrum_db(x, sr)
        keep = (f > 40.0) & (f < 12_000.0)
        f, dbs = f[keep], dbs[keep]
        pk, props = find_peaks(dbs, prominence=6.0, distance=6)
        if len(pk) < 32:
            k = np.hanning(31); k /= k.sum()
            sm = np.convolve(dbs, k, mode="same")
            grid = np.geomspace(f[0], f[-1], 200)
            return grid, np.interp(np.log(grid), np.log(f), sm)
        sel = np.sort(pk[np.argsort(props["prominences"])[::-1][:48]])
        return f[sel], dbs[sel]

def source_spectrum():
    x, sr = load_wav(SOURCE_WAV)
    f, db = averaged_spectrum_db(x, sr)
    keep = (f > 40.0) & (f < 14_000.0)
    grid = np.geomspace(60.0, 12_000.0, 256)
    d = np.interp(np.log(grid), np.log(f[keep]), db[keep])
    k = np.hanning(15); k /= k.sum()
    return grid, np.convolve(d, k, mode="same")

def main():
    src = source_spectrum() if SOURCE_WAV else None
    if src is not None:
        print(f"source reference: {SOURCE_WAV.name} (subtracted per corner)")
    fitted = []
    for path, lab in zip(WAVS, LABELS):
        f, db = envelope(path)
        if src is not None:
            db = db - np.interp(np.log(np.clip(f, src[0][0], src[0][-1])),
                                np.log(src[0]), src[1])
        roots6, _, metrics = fit_arma(f, db - float(np.mean(db)), RATE)
        words, trimmed = trim_to_p95(roots6, 0.0, RATE)
        fitted.append((words, trimmed, (f, db)))
        print(f"  {lab}: {path.name}  fit residual {metrics[0]:.2f} dB")

    flat = [w for words, _, _ in fitted for w in words]
    body, max_r = pack_and_certify(flat)

    corner_roots = [trimmed for _, trimmed, _ in fitted]
    corner_curves = [(f, db - float(np.mean(db))) for _, _, (f, db) in fitted]
    print("joint refine (multistart over correspondences, a few minutes)...")
    body_j, info = joint_refine(corner_roots, corner_curves, rate=RATE)
    rms_a = surface_rms(body, info["targets"], info["freqs"], RATE)
    print(f"interior vs blended corners (7x7): independent {rms_a:.2f} dB rms")
    if body_j is not None:
        ok, mr_j = certify_bytes(body_j)
        rms_j = surface_rms(body_j, info["targets"], info["freqs"], RATE)
        print(f"joint: {rms_j:.2f} dB rms (seed {info['winning_seed']}, "
              f"{info['seconds']:.0f}s, certify {'PASS' if ok else 'FAIL'})")
        if ok and rms_j < rms_a:
            body, max_r = body_j, mr_j
            print("joint body adopted")
        else:
            print("joint body rejected; keeping independent-corner body")
    else:
        print("joint stage: encoder refused refined roots; keeping "
              "independent-corner body")

    out = ROOT / "bodies" / "candidates" / f"{NAME}.body240"
    out.write_bytes(body)
    print(f"certified: hottest pole {max_r:.6f}  ->  {out.name}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    grid = np.geomspace(40.0, 12_000.0, 1024)
    fig, axes = plt.subplots(2, 2, figsize=(14, 9), sharex=True, sharey=True)
    for ax, (_, trimmed, (mf, mdb)), lab in zip(axes.flat, fitted, LABELS):
        rdb = roots_response_db(trimmed, grid, RATE)
        ax.semilogx(mf, mdb - np.mean(mdb), "x", color="0.45", ms=3.5,
                    label="capture envelope")
        ax.semilogx(grid, rdb - np.mean(rdb), color="#c96a54", lw=1.5,
                    label="the corner")
        ax.set_ylim(-60, 30)
        ax.set_title(lab, fontsize=10)
        ax.grid(alpha=0.3)
    axes.flat[0].legend(fontsize=8)
    fig.suptitle(f"{NAME} - four captures, four corners (as assigned)", fontsize=12)
    fig.tight_layout()
    fig.savefig(out.with_name(f"{NAME}_corners.png"), dpi=110)
    print(f"plate: {NAME}_corners.png")

main()
