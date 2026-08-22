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

K = V.KLATT

def lane_rows(lanes, c):
    m100, q100 = c in (1, 3), c >= 2
    rows = []
    for ln in lanes:
        if ln is None:
            rows.append(None); continue
        pHz = ln["f1"] if m100 else ln["f0"]
        pR = ln["pQ1"] if q100 else ln["pQ0"]
        zHz = ln.get("zf1" if m100 else "zf0", 0.0) or pHz
        zR = ln["zQ1"] if q100 else ln["zQ0"]
        rows.append((pHz, pR, zHz, zR))
    return rows

BODIES = {
    "TALKING_808": [
        dict(f0=80.0, f1=80.0, pQ0=0.95, pQ1=0.95, zQ0=0.85, zQ1=0.85),
        dict(f0=K["UW"][0], f1=K["IY"][0], pQ0=V.r_of_bw(150), pQ1=V.r_of_bw(15), zQ0=V.r_of_bw(600), zQ1=V.r_of_bw(600)),
        dict(f0=K["UW"][1], f1=K["IY"][1], pQ0=V.r_of_bw(150), pQ1=V.r_of_bw(15), zQ0=V.r_of_bw(600), zQ1=V.r_of_bw(600)),
        dict(f0=K["UW"][2], f1=K["IY"][2], pQ0=V.r_of_bw(150), pQ1=V.r_of_bw(15), zQ0=V.r_of_bw(600), zQ1=V.r_of_bw(600)),
        None, None],
    "OVO_WASH": [
        dict(f0=12000.0, f1=150.0, pQ0=0.90, pQ1=0.90, zf0=19000.0, zf1=600.0, zQ0=0.93, zQ1=0.93),
        dict(f0=12000.0, f1=150.0, pQ0=0.88, pQ1=0.975, zf0=19000.0, zf1=600.0, zQ0=0.91, zQ1=0.91),
        dict(f0=12000.0, f1=150.0, pQ0=0.86, pQ1=0.86, zf0=19000.0, zf1=600.0, zQ0=0.89, zQ1=0.89),
        None, None, None],
    "ACID_808": [
        dict(f0=60.0, f1=60.0, pQ0=0.96, pQ1=0.96, zQ0=0.88, zQ1=0.88),
        dict(f0=250.0, f1=3000.0, pQ0=0.90, pQ1=0.90, zf0=1000.0, zf1=12000.0, zQ0=0.92, zQ1=0.92),
        dict(f0=250.0, f1=3000.0, pQ0=0.93, pQ1=0.995, zQ0=0.85, zQ1=0.85),
        None, None, None],
    "SIZZLE_AIR": [
        dict(f0=5000.0, f1=12000.0, pQ0=0.90, pQ1=0.975, zQ0=0.82, zQ1=0.82),
        dict(f0=12000.0, f1=5000.0, pQ0=0.90, pQ1=0.975, zQ0=0.82, zQ1=0.82),
        None, None, None, None],
}

TRAVELLERS = {"SIZZLE_AIR": ((0, 1),)}

def build(name, lanes):
    V.zone_check([lane_rows(lanes, c) for c in range(4)],
                 allow_cross=TRAVELLERS.get(name, ()))
    words = []
    for c in range(4):
        rows = lane_rows(lanes, c)
        act = [r for r in rows if r]
        peak = V.response_db(act).max()
        sc = 10 ** (-peak / 20 / len(act))
        for r in rows:
            words += V.IDENT if r is None else V.stage_words(*r, sc)
    wbuf = (ctypes.c_uint16 * 120)(*words)
    body = (ctypes.c_uint8 * 240)()
    if V.lib.trench_pack_body_from_corner_words(wbuf, 120, body) != 0:
        print(f"{name}: packer refused"); return
    p = ctypes.c_int(); mr = ctypes.c_double(); fm = ctypes.c_double(); fq = ctypes.c_double()
    V.lib.trench_certify_body(body, 240, 17, 1.0, ctypes.byref(p), ctypes.byref(mr),
                              ctypes.byref(fm), ctypes.byref(fq))
    if p.value != 1:
        print(f"{name}: CERT FAIL at morph {fm.value:.2f} q {fq.value:.2f} (pole {mr.value:.4f})")
        return
    V.OUT.mkdir(parents=True, exist_ok=True)
    (V.OUT / f"{name}.body240").write_bytes(bytes(body))
    V.default_plot(body, name)
    print(f"{name}: CERT PASS  hottest pole {mr.value:.4f}")

if __name__ == "__main__":
    for name, lanes in BODIES.items():
        build(name, lanes)
