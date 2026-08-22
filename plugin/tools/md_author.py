from __future__ import annotations

import ctypes
import json
import sys
from pathlib import Path

import numpy as np
from scipy.optimize import linear_sum_assignment

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
sys.path.insert(0, str(Path(r"C:\Users\hooki\df2-workstation")))
from arma_measure_lib import lib  # noqa: E402
from joint_fit import cascade_db, probe, sec_coord, sec_dist  # noqa: E402
from pyruntime.packed_interp import kernel_to_biquad, words_to_coeffs  # noqa: E402

RATE = 44_100.0
FAMILY = 0
BASE = [18, 18, 4, 1][FAMILY]
SCALE = [220, 220, 200, 177][FAMILY]
FREQS = np.geomspace(40.0, 16_000.0, 384)
Z = np.exp(-1j * 2 * np.pi * FREQS / RATE)

def md_words(type_id, freq_byte, gain_byte, shift=0):
    freq = ((SCALE * freq_byte) >> 7) + BASE
    gain = max(-32, min(31, ((gain_byte - 0x40) >> 1) + shift))
    rad = ((freq * 0x7C) >> 8) + 0x76
    cl = lambda v: max(0, min(255, v))
    if type_id == 1:
        return [freq << 8, cl(rad + gain) << 8, freq << 8,
                cl(rad - gain) << 8, 0xE000]
    if type_id == 2:
        return [0xEC00, 0xFF00, freq << 8, cl(rad - gain) << 8,
                (freq + 0xF5) << 8]
    if type_id == 3:
        return [BASE << 8, (((BASE * 0x7C) >> 8) + 0x96) << 8,
                freq << 8, cl(rad - gain) << 8, 0xE000]
    raise ValueError(type_id)

def words_db(words5):
    b0, b1, b2, a1, a2 = kernel_to_biquad(words_to_coeffs(tuple(words5)))
    return 20 * np.log10(np.abs(b0 + b1 * Z + b2 * Z * Z)
                         / np.abs(1 + a1 * Z + a2 * Z * Z) + 1e-12)

def rom_stage_db(words5):
    return words_db(words5)

def stage_words(body, corner, slot):
    import struct
    w = struct.unpack("<120H", body)
    return list(w[corner * 30 + slot * 5: corner * 30 + slot * 5 + 5])

def stage_roots(words5):
    row = (ctypes.c_uint16 * 5)(*words5)
    out = (ctypes.c_double * 5)()
    if lib.trench_stage_roots_from_words_at(row, RATE, out) != 0:
        return None
    return list(out)

def pick_type(roots):
    if roots is None:
        return 1
    phz, pr, zhz, zr, _ = roots
    if pr <= 0.05:
        return 3 if zhz < phz else 2
    if zr > 0.05 and abs(np.log2(max(zhz, 20) / max(phz, 20))) < 1.0:
        return 1
    return 1

def fit_stage(target_db, type_id):
    best = (1e18, 0, 0)
    for fb in range(0, 128, 2):
        for gb in range(0, 128, 2):
            e = words_db(md_words(type_id, fb, gb)) - target_db
            r = float(((e - e.mean()) ** 2).mean())
            if r < best[0]:
                best = (r, fb, gb)
    _, fb0, gb0 = best
    for fb in range(max(0, fb0 - 2), min(128, fb0 + 3)):
        for gb in range(max(0, gb0 - 2), min(128, gb0 + 3)):
            e = words_db(md_words(type_id, fb, gb)) - target_db
            r = float(((e - e.mean()) ** 2).mean())
            if r < best[0]:
                best = (r, fb, gb)
    return best

def main():
    body_path = Path(sys.argv[1] if len(sys.argv) > 1
                     else ROOT / "ref/presets/P2k_013_talking_hedz.bin")
    out_path = Path(sys.argv[2] if len(sys.argv) > 2
                    else ROOT / "evidence" / "md_author" /
                    (body_path.stem + ".md.json"))
    body = body_path.read_bytes()
    assert len(body) == 240, f"{body_path}: {len(body)} bytes"

    r0 = [stage_roots(stage_words(body, 0, s)) for s in range(6)]
    r1 = [stage_roots(stage_words(body, 1, s)) for s in range(6)]
    cost = np.array([[sec_dist(sec_coord(a), sec_coord(b)) for b in r1]
                     for a in r0])
    _, col = linear_sum_assignment(cost)
    print(f"hungarian lane check (M0 -> M100 slots): {list(col)}"
          f"{'  = identity, lanes confirmed' if list(col) == list(range(6)) else '  NON-IDENTITY'}")

    records = []
    for s in range(6):
        best = None
        for t in (1, 2, 3):
            rA, fbA, gbA = fit_stage(rom_stage_db(stage_words(body, 0, s)), t)
            rB, fbB, gbB = fit_stage(rom_stage_db(stage_words(body, 1, s)), t)
            if best is None or rA + rB < best[0]:
                best = (rA + rB, t, rA, fbA, gbA, rB, fbB, gbB)
        _, t, rA, fbA, gbA, rB, fbB, gbB = best
        records.append(dict(stage=s + 1, type=t,
                            A=dict(freq=fbA, gain=gbA),
                            B=dict(freq=fbB, gain=gbB)))
        shape = {1: "EQ", 2: "LP", 3: "HP"}[t]
        print(f"  STAGE {s+1}  {shape}  A(freq={fbA:3d} gain={gbA:3d}) "
              f"rms {np.sqrt(rA):.2f} dB   B(freq={fbB:3d} gain={gbB:3d}) "
              f"rms {np.sqrt(rB):.2f} dB")

    targets = {}
    for corner in (0, 1):
        rows, _ = probe(body, float(corner), 0.0, RATE)
        targets[corner] = cascade_db(rows, FREQS, RATE)

    def stage_db(rec, key):
        return words_db(md_words(rec["type"], rec[key]["freq"],
                                 rec[key]["gain"]))

    for sweep in range(4):
        improved = False
        for corner, key in [(0, "A"), (1, "B")]:
            for s in range(6):
                others = sum(stage_db(r, key) for i, r in enumerate(records)
                             if i != s)
                resid = targets[corner] - others
                t = records[s]["type"]
                r_best, fb, gb = fit_stage(resid, t)
                cur = records[s][key]
                e_cur = stage_db(records[s], key) - resid
                r_cur = float(((e_cur - e_cur.mean()) ** 2).mean())
                if r_best + 1e-9 < r_cur:
                    records[s][key] = dict(freq=fb, gain=gb)
                    improved = True
        if not improved:
            break

    print("after whole-cascade descent:")
    for rec in records:
        shape = {1: "EQ", 2: "LP", 3: "HP"}[rec["type"]]
        print(f"  STAGE {rec['stage']}  {shape}  "
              f"A(freq={rec['A']['freq']:3d} gain={rec['A']['gain']:3d})  "
              f"B(freq={rec['B']['freq']:3d} gain={rec['B']['gain']:3d})")

    for corner, key in [(0, "A"), (1, "B")]:
        md = np.zeros_like(FREQS)
        for rec in records:
            md += words_db(md_words(rec["type"], rec[key]["freq"],
                                    rec[key]["gain"]))
        rows, _ = probe(body, float(corner), 0.0, RATE)
        rom = cascade_db(rows, FREQS, RATE)
        e = md - rom
        rms = float(np.sqrt(((e - e.mean()) ** 2).mean()))
        print(f"corner null M{corner*100} Q0: {rms:.2f} dB rms")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(
        dict(source=str(body_path), family=FAMILY, records=records), indent=1))
    print(f"wrote {out_path}")

if __name__ == "__main__":
    main()
