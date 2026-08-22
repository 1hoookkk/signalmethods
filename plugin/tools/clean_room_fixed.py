from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
GRID = np.geomspace(60.0, 15_000.0, 512)

def db(mag2):
    return 10.0 * np.log10(np.maximum(mag2, 1e-12))

def sec_lp(f, fc, q):
    x = f / fc
    return 1.0 / ((1 - x * x) ** 2 + (x / q) ** 2)

def sec_hp(f, fc, q):
    x = f / fc
    return x ** 4 / ((1 - x * x) ** 2 + (x / q) ** 2)

def sec_bp(f, fc, q):
    x = f / fc
    return (x / q) ** 2 / ((1 - x * x) ** 2 + (x / q) ** 2)

def npole(kind, n, fc, res):
    q_res = 0.707 + res * 5.3
    stages = n // 2
    m = sec_lp(GRID, fc, q_res) if kind == "lp" else sec_hp(GRID, fc, q_res)
    for _ in range(stages - 1):
        m = m * (sec_lp(GRID, fc, 0.707) if kind == "lp"
                 else sec_hp(GRID, fc, 0.707))
    return db(m)

def bandpass(n, fc, res):
    q = 1.2 + res * 6.8
    m = sec_bp(GRID, fc, q)
    m = m / m.max()
    if n == 4:
        m = m * m
    return db(m)

def contrary(fc_up, fc_dn, res):
    q = 1.5 + res * 5.5
    a = sec_bp(GRID, fc_up, q); a /= a.max()
    b = sec_bp(GRID, fc_dn, q); b /= b.max()
    return db(a + b)

def bell(fc, bw_oct, gain_db):
    lg = np.log2(GRID / fc)
    return gain_db * np.exp(-0.5 * (lg / (bw_oct / 2.355)) ** 2)

def phaser(centers, depth_db, width_oct, peak_db):
    d = np.zeros_like(GRID)
    for fc in centers:
        lg = np.log2(GRID / fc)
        d -= depth_db * np.exp(-0.5 * (lg / (width_oct / 2.355)) ** 2)
        for side in (-0.45, 0.45):
            lgp = np.log2(GRID / fc) - side
            d += peak_db * np.exp(-0.5 * (lgp / 0.18) ** 2)
    return d

def sweep(lo, hi, m):
    return lo * (hi / lo) ** m

PRESETS = {
    "CR_2pole_lowpass":  lambda m, q: npole("lp", 2, sweep(120, 9000, m), q),
    "CR_4pole_lowpass":  lambda m, q: npole("lp", 4, sweep(120, 9000, m), q),
    "CR_6pole_lowpass":  lambda m, q: npole("lp", 6, sweep(120, 9000, m), q),
    "CR_2pole_highpass": lambda m, q: npole("hp", 2, sweep(150, 4000, m), q),
    "CR_4pole_highpass": lambda m, q: npole("hp", 4, sweep(150, 4000, m), q),
    "CR_2pole_bandpass": lambda m, q: bandpass(2, sweep(150, 5000, m), q),
    "CR_4pole_bandpass": lambda m, q: bandpass(4, sweep(150, 5000, m), q),
    "CR_contrary_bandpass": lambda m, q: contrary(
        sweep(250, 3000, m), sweep(3000, 250, m), q),
    "CR_swept_eq_1oct": lambda m, q: bell(sweep(80, 8000, m), 1.0,
                                          2.0 + 22.0 * q),
    "CR_swept_eq_2_1oct": lambda m, q: bell(sweep(80, 8000, m), 2.0,
                                            2.0 + 22.0 * q),
    "CR_swept_eq_3_1oct": lambda m, q: bell(sweep(80, 8000, m), 3.0,
                                            2.0 + 22.0 * q),
    "CR_phaser1": lambda m, q: phaser(
        [sweep(300, 3200, m), sweep(600, 6400, m)],
        8.0 + 16.0 * q, 0.5 - 0.2 * q, 8.0 * q),
    "CR_phaser2": lambda m, q: phaser(
        [sweep(220, 2400, m), sweep(440, 4800, m), sweep(880, 9600, m)],
        8.0 + 16.0 * q, 0.5 - 0.2 * q, 8.0 * q),
}

CORNERS = [("c00", 0.0, 0.0), ("c10", 1.0, 0.0),
           ("c01", 0.0, 1.0), ("c11", 1.0, 1.0)]

def main():
    outdir = Path(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1] != "--run" \
        else ROOT / "evidence" / "clean_room_targets"
    run = "--run" in sys.argv
    cmds = []
    for name, fn in PRESETS.items():
        d = outdir / name
        d.mkdir(parents=True, exist_ok=True)
        for tag, m, q in CORNERS:
            dbs = fn(m, q)
            dbs = np.maximum(dbs, -60.0)
            dbs = dbs - dbs.mean()
            (d / f"{tag}.txt").write_text(
                "\n".join(f"{f:.3f} {v:.4f}" for f, v in zip(GRID, dbs)))
        cmd = [sys.executable, str(ROOT / "tools" / "make_body.py"), name,
               str(d / "c00.txt"), str(d / "c10.txt"),
               str(d / "c01.txt"), str(d / "c11.txt")]
        cmds.append(cmd)
        print(" ".join(cmd[1:]))
    if run:
        for cmd in cmds:
            print(f"\n=== {cmd[2]}")
            subprocess.run(cmd, check=False)

if __name__ == "__main__":
    main()
