from __future__ import annotations

import sys
import ctypes
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vowel_morph as V

NOTE = {n: i for i, n in enumerate(
    ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"])}
NOTE.update({"DB": 1, "EB": 3, "GB": 6, "AB": 8, "BB": 10})
CHORD_IV = {"": (0, 4, 7), "M": (0, 4, 7), "MAJ": (0, 4, 7),
            "M7": (0, 4, 7, 11), "MAJ7": (0, 4, 7, 11),
            "m": (0, 3, 7), "MIN": (0, 3, 7),
            "m7": (0, 3, 7, 10), "MIN7": (0, 3, 7, 10),
            "7": (0, 4, 7, 10), "DIM": (0, 3, 6), "SUS4": (0, 5, 7)}

BW_Q0, BW_Q100 = 120.0, 12.0

def hz(midi):
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)

def parse_chord(sym):
    s = sym.strip()
    root = s[:2].upper() if len(s) > 1 and s[1] in "#b" else s[:1].upper()
    root = root.replace("B", "b") if root.endswith("b") and len(root) == 2 else root
    quality = s[len(root):]
    quality = quality if quality in CHORD_IV else quality.upper()
    if quality not in CHORD_IV:
        raise SystemExit(f"unknown chord quality '{quality}' in {sym}")
    return NOTE[root.upper()], CHORD_IV[quality]

def voices_for(sym):
    root, iv = parse_chord(sym)
    bass = 45 + root if 45 + root < 51 else 33 + root
    upper_pool = sorted({60 + root + i + o for i in iv for o in (0, 12, 24)})
    upper = [m for m in upper_pool if 62 <= m <= 96][:8]
    return bass, upper

MIN_TRAVEL_ST = 5.0

def lead(sym_a, sym_b):
    bass_a, pool_a = voices_for(sym_a)
    bass_b, pool_b = voices_for(sym_b)
    va = pool_a[:4] if len(pool_a) >= 4 else (pool_a * 2)[:4]

    def led_to(pool):
        vb, used = [], []
        for m in va:
            best = min((x for x in pool if x not in used), key=lambda x: abs(x - m))
            used.append(best)
            vb.append(best)
        return vb

    vb = led_to(pool_b)
    for lift in (12, 19, 24):
        travel = np.mean([abs(b - a) for a, b in zip(va, vb)])
        if travel >= MIN_TRAVEL_ST:
            break
        vb = [b + lift if b + lift <= 103 else b for b in led_to(pool_b)]
    return [bass_a] + va, [bass_b] + vb

def corner_rows(sym_a, sym_b, corner, sym_c=None, sym_d=None):
    m100, q100 = corner in (1, 3), corner >= 2
    if q100 and sym_c:
        base = lead(sym_a, sym_b)[1 if m100 else 0]
        _, pool = voices_for(sym_d if m100 else sym_c)
        bass = voices_for(sym_d if m100 else sym_c)[0]
        used, notes = [], [bass]
        for m in base[1:]:
            best = min((x for x in pool if x not in used), key=lambda x: abs(x - m))
            used.append(best); notes.append(best)
    else:
        notes = lead(sym_a, sym_b)[1 if m100 else 0]
    pR = V.r_of_bw(BW_Q100 if q100 else BW_Q0)
    zR = V.r_of_bw(600.0)
    rows = [(hz(m), pR, hz(m), zR) for m in notes]
    return rows + [None] * (6 - len(rows))

def make_progression(sym_a, sym_b, sym_c=None, sym_d=None):
    name = (f"CALL_{sym_a}{sym_b}_RESP_{sym_c}{sym_d}" if sym_c
            else f"CHORD_{sym_a}_to_{sym_b}").replace("#", "s")
    va, vb = lead(sym_a, sym_b)
    moves = ["held" if a == b else f"{'+' if b > a else ''}{b - a} st"
             for a, b in zip(va, vb)]
    travel = float(np.mean([abs(b - a) for a, b in zip(va, vb)]))
    print(f"{name}: voices {va} -> {vb}  ({', '.join(moves)})  mean travel {travel:.1f} st")
    if travel < MIN_TRAVEL_ST:
        raise RuntimeError(f"{name}: mean travel {travel:.1f} st - a sway, not a morph")
    V.zone_check([corner_rows(sym_a, sym_b, c, sym_c, sym_d) for c in range(4)])
    words = []
    for c in range(4):
        rows = corner_rows(sym_a, sym_b, c, sym_c, sym_d)
        active = sum(1 for r in rows if r)
        peak = V.response_db(rows).max()
        sc = 10 ** (-peak / 20 / active)
        for r in rows:
            words += V.IDENT if r is None else V.stage_words(*r, sc)
    wbuf = (ctypes.c_uint16 * 120)(*words)
    body = (ctypes.c_uint8 * 240)()
    if V.lib.trench_pack_body_from_corner_words(wbuf, 120, body) != 0:
        raise RuntimeError("packer refused")
    p = ctypes.c_int(); mr = ctypes.c_double(); fm = ctypes.c_double(); fq = ctypes.c_double()
    V.lib.trench_certify_body(body, 240, 17, 1.0, ctypes.byref(p), ctypes.byref(mr),
                              ctypes.byref(fm), ctypes.byref(fq))
    if p.value != 1:
        raise RuntimeError(f"{name}: CERT FAIL at {fm.value:.2f}/{fq.value:.2f}")
    V.OUT.mkdir(parents=True, exist_ok=True)
    (V.OUT / f"{name}.body240").write_bytes(bytes(body))
    V.default_plot(body, name)
    print(f"{name}: CERT PASS  hottest pole {mr.value:.4f}")

if __name__ == "__main__":
    args = sys.argv[1:]
    if len(args) == 4 and "--cr" not in args:
        make_progression(*args)
    elif args:
        for a, b in zip(args[::2], args[1::2]):
            make_progression(a, b)
    else:
        for a, b in [("Am", "F"), ("Cmaj7", "Fmaj7")]:
            make_progression(a, b)
        make_progression("Am", "F", "C", "G")
