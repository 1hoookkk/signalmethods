from __future__ import annotations

import ctypes
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib  # noqa: E402
from joint_fit import cascade_db, certify, pack, probe  # noqa: E402

BLOCKS = Path(r"C:\Users\hooki\df2-workstation\evidence"
              r"\emulatorx_binary_filter_rip_20260729\x3_menu\runtime_blocks")
sys.path.insert(0, str(Path(r"C:\Users\hooki\df2-workstation")))
from pyruntime.packed_interp import kernel_to_biquad, words_to_coeffs  # noqa: E402

RATE = 44_100.0
FUND = [
    ("2_pole_lowpass", 1), ("4_pole_lowpass", 2), ("6_pole_lowpass", 3),
    ("2_pole_highpass", 1), ("4_pole_highpass", 2),
    ("2_pole_bandpass", 1), ("4_pole_bandpass", 2), ("contrary_bandpass", 1),
    ("swept_eq_1_octave", 1), ("swept_eq_2_1_octave", 1),
    ("swept_eq_3_1_octave", 1),
    ("phaser_1", 2), ("phaser_2", 2), ("bat_phaser", 2),
    ("flanger_lite", 3), ("vocal_ah_ay_ee", 3), ("vocal_oo_ah", 3),
]

lib.trench_stage_coeffs_from_words_at = getattr(
    lib, "trench_stage_coeffs_from_words_at", None)

def identity_row():
    row = (ctypes.c_uint16 * 5)()
    rc = lib.trench_stage_words_from_roots_at(
        (ctypes.c_double * 5)(1000.0, 0.0, 1000.0, 0.0, 1.0), RATE, row)
    assert rc == 0
    return list(row)

def read_corners(stem, ns):
    raw = (BLOCKS / f"{stem}_44100.raw").read_bytes()
    w = list(struct.unpack(f"<{len(raw)//2}H", raw))
    per = ns * 5
    assert len(w) == 4 * per, f"{stem}: {len(w)} words, expected {4*per}"
    return [[w[c * per + s * 5: c * per + s * 5 + 5] for s in range(ns)]
            for c in range(4)]

def df2_corner_db(stages, freqs):
    z = np.exp(-1j * 2 * np.pi * freqs / RATE)
    t = np.zeros_like(freqs)
    for s in stages:
        b0, b1, b2, a1, a2 = kernel_to_biquad(words_to_coeffs(tuple(s)))
        t += 20 * np.log10(np.abs(b0 + b1 * z + b2 * z * z)
                           / np.abs(1 + a1 * z + a2 * z * z) + 1e-12)
    return t

def main():
    ident = identity_row()
    freqs = np.geomspace(40.0, 16_000.0, 384)
    out_dir = ROOT / "bodies" / "candidates"
    print(f"{'name':24s} {'codec-null':>10s} {'certify':>8s} {'max_r':>9s}")
    for stem, ns in FUND:
        corners = read_corners(stem, ns)
        words = []
        for c in range(4):
            for s in range(6):
                words += corners[c][s] if s < ns else ident
        body = pack(words)
        worst = 0.0
        for (m, q), c in zip([(0, 0), (1, 0), (0, 1), (1, 1)], range(4)):
            rows, _ = probe(body, float(m), float(q), RATE)
            ours = cascade_db(rows, freqs, RATE)
            theirs = df2_corner_db(corners[c], freqs)
            worst = max(worst, float(np.abs(ours - theirs).max()))
        ok, mr = certify(body)
        name = f"X3F_{stem}"
        status = "PASS" if ok else "FAIL"
        print(f"{name:24s} {worst:9.4f}dB {status:>8s} {mr:9.6f}")
        if ok and worst < 0.01:
            (out_dir / f"{name}.body240").write_bytes(body)
        else:
            print(f"  -> NOT WRITTEN (codec null {worst:.4f} dB, "
                  f"certify {status})")

if __name__ == "__main__":
    main()
