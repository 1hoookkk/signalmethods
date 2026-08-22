from __future__ import annotations

import ctypes
import json
import math
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))

from arma_measure_lib import lib, pack_and_certify, RT_DOUBLES   # noqa: E402

if len(sys.argv) < 4:
    print("usage: translate_law.py <src.body240> <donor-name> <out.body240> [rate_hz]")
    raise SystemExit(2)
SRC = Path(sys.argv[1])
DONOR = sys.argv[2]
OUT = Path(sys.argv[3])
RATE = float(sys.argv[4]) if len(sys.argv) > 4 else 48_000.0

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

GRID = np.geomspace(40.0, 16_000.0, 2048)
CORNER_MQ = {"A": (0.0, 0.0), "B": (1.0, 0.0), "C": (0.0, 1.0), "D": (1.0, 1.0)}

def refuse(msg: str):
    print(f"REFUSED: {msg}")
    raise SystemExit(2)

def find_donor(name: str) -> dict:
    hits = []
    for p in sorted((ROOT / "dossiers" / "characters").glob("P2k_*.json")):
        d = json.loads(p.read_text())
        if name.lower().replace(" ", "") in d["name"].lower().replace(" ", ""):
            hits.append(d)
    if not hits:
        refuse(f"no dossier matches donor '{name}'")
    return hits[0]

def probe_response_db(body: bytes, m: float, q: float) -> np.ndarray:
    coeffs = (ctypes.c_double * RT_DOUBLES)()
    max_r = ctypes.c_double()
    unstable = ctypes.c_uint32()
    nonfinite = ctypes.c_uint32()
    buf = ctypes.create_string_buffer(body, 240)
    if lib.trench_packed_probe_at(buf, 240, m, q, RATE, coeffs,
                                  ctypes.byref(max_r), ctypes.byref(unstable),
                                  ctypes.byref(nonfinite)) != 0:
        refuse("probe refused the source body")
    w = 2.0 * np.pi * GRID / RATE
    z1 = np.exp(-1j * w)
    z2 = z1 * z1
    total = np.zeros_like(GRID)
    for s in range(6):
        b0, b1, b2, a1, a2 = coeffs[s * 5: s * 5 + 5]
        h = (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)
        total += 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))
    return total

def read_words(path: Path):
    data = path.read_bytes()
    if len(data) != 240:
        refuse(f"{path.name} is not a 240-byte body")
    words, i = {}, 0
    for key in "ABCD":
        rows = []
        for _ in range(6):
            rows.append(tuple(data[i + 2 * k] | (data[i + 2 * k + 1] << 8) for k in range(5)))
            i += 10
        words[key] = rows
    return words, data

def decode_row(row):
    w = (ctypes.c_uint16 * 5)(*row)
    out = (ctypes.c_double * 5)()
    if lib.trench_stage_roots_from_words_at(w, RATE, out) != 0:
        return None
    return list(out)

def rp_db(r):
    return -20.0 * math.log10(max(1e-12, 1.0 - min(r, 1.0 - 1e-12)))

def r_from_rp(db):
    return 1.0 - 10.0 ** (-db / 20.0)

def encode_roots(r5):
    roots = (ctypes.c_double * 5)(*r5)
    out = (ctypes.c_uint16 * 5)()
    if lib.trench_stage_words_from_roots_at(roots, RATE, out) != 0:
        refuse(f"encoder refused {r5}")
    return tuple(out)

def pack_flat(corner_rows) -> bytes:
    flat = [w for key in "ABCD" for row in corner_rows[key] for w in row]
    arr = (ctypes.c_uint16 * 120)(*flat)
    out = ctypes.create_string_buffer(240)
    if lib.trench_pack_body_from_corner_words(arr, 120, out) != 0:
        refuse("packer refused")
    return out.raw

def donor_grammar(dossier: dict, corner_name: str) -> list[tuple[float, float]]:
    rows = []
    for st in dossier["corners"][corner_name]:
        ph, zh = st["pole"]["hz"], st["zero"]["hz"]
        zr, pr = st["zero"]["radius"], st["pole"]["radius"]
        if not (150.0 <= ph <= 6000.0) or zr <= 0.0 or zh <= 0.0:
            continue
        depth = rp_db(zr) - rp_db(pr)
        if depth >= 0.0:
            continue
        rows.append((ph, math.log2(zh / ph), depth))
    rows.sort()
    return [(o, d) for _, o, d in rows]

def main():
    words, body = read_words(SRC)
    dossier = find_donor(DONOR)
    grammar = {"A": donor_grammar(dossier, "M0_Q0"),
               "B": donor_grammar(dossier, "M100_Q0")}
    for key in "AB":
        if len(grammar[key]) < 2:
            refuse(f"donor {dossier['name']} has no transplantable mouth grammar at "
                   + ("M0" if key == "A" else "M100"))

    mouth = []
    for s in range(6):
        r = decode_row(words["A"][s])
        if r is None:
            continue
        ph, pr, zh, zr, sc = r
        if not (ph > 0 and pr > 0 and 100.0 <= ph <= 7000.0):
            continue
        naked = zr < 0.01 or zh <= 0.0
        local = zh > 0.0 and abs(math.log2(zh / ph)) <= 1.5
        if naked or local:
            mouth.append(s)
    if not mouth:
        refuse("no voice-band stages to teach - every pole here is frame or far-carved")
    print(f"{SRC.stem}: stages {[s + 1 for s in mouth]} learn from {dossier['name']}; the rest hold")

    hottest = max(mouth, key=lambda s: decode_row(words["A"][s])[1])

    def grammar_for(key: str):
        ours = sorted(mouth, key=lambda s: decode_row(words[key][s])[0])
        g = grammar[key]
        ranks = np.linspace(0.0, len(g) - 1.0, num=len(ours))
        offs = np.interp(ranks, np.arange(len(g)), [o for o, _ in g])
        deps = np.interp(ranks, np.arange(len(g)), [d for _, d in g])
        return {s: (float(o), float(d)) for s, o, d in zip(ours, offs, deps)}

    depths = {key: grammar_for(key) for key in "AB"}

    def build():
        corners = {}
        for q0_key, q100_key in (("A", "C"), ("B", "D")):
            rows_q0, rows_q100 = [], []
            for s in range(6):
                r = decode_row(words[q0_key][s])
                if r is None or s not in mouth or r[0] <= 0.0 or r[1] <= 0.0:
                    rows_q0.append(words[q0_key][s])
                    rows_q100.append(words[q100_key][s])
                    continue
                ph, pr, zh, zr, sc = r
                p_db = rp_db(pr)
                off_oct, d_rel = depths[q0_key][s]
                zero_hz = min(ph * 2.0 ** off_oct, RATE * 0.45)
                carved = [ph, pr, zero_hz, r_from_rp(max(p_db + d_rel, 0.0)), sc]
                rows_q0.append(encode_roots(carved))
                factor = 0.2 if s == hottest else 0.4
                pushed = carved.copy()
                pushed[1] = r_from_rp(p_db + factor * (66.0 - p_db))
                rows_q100.append(encode_roots(pushed))
            corners[q0_key] = rows_q0
            corners[q100_key] = rows_q100
        return corners

    trial_corners = build()
    trial = pack_flat(trial_corners)

    final_corners = {}
    for key, (m, q) in CORNER_MQ.items():
        want = float(probe_response_db(body, m, q).max())
        have = float(probe_response_db(trial, m, q).max())
        mult = 10.0 ** ((want - have) / 20.0 / len(mouth))
        rows = []
        for s in range(6):
            row = trial_corners[key][s]
            r = decode_row(row)
            if r is not None and s in mouth:
                r[4] *= mult
                row = encode_roots(r)
            rows.append(row)
        final_corners[key] = rows

    law_body, max_r = pack_and_certify(
        [w for key in "ABCD" for row in final_corners[key] for w in row])
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(law_body)
    print(f"certified: hottest pole {max_r:.6f}  ->  {OUT.name}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(2, 2, figsize=(14, 9), sharex=True, sharey=True)
    for ax, (label, (m, q)) in zip(axes.flat,
                                   [("M0 Q0", CORNER_MQ["A"]), ("M100 Q0", CORNER_MQ["B"]),
                                    ("M0 Q100", CORNER_MQ["C"]), ("M100 Q100", CORNER_MQ["D"])]):
        ax.semilogx(GRID, probe_response_db(body, m, q), "--", color="0.4",
                    lw=1.2, label="measured (as fitted)")
        ax.semilogx(GRID, probe_response_db(law_body, m, q), color="#c96a54",
                    lw=1.6, label=f"learned from {dossier['name']}")
        ax.set_ylim(-60, 30)
        ax.set_title(label, fontsize=10)
        ax.grid(alpha=0.3)
    axes.flat[0].legend(fontsize=8)
    fig.suptitle(f"{SRC.stem} - {dossier['name']}'s stage law, our measurement", fontsize=12)
    fig.tight_layout()
    fig.savefig(OUT.with_name(OUT.stem + "_vs.png"), dpi=110)
    print(f"plate: {OUT.stem}_vs.png")

main()
