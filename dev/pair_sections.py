"""Section-by-section comparison of a close P2K / Morpheus response pair.

Each side is decoded at its own datum.  Only the resulting Hz values are
compared.  For every Morpheus corner the nearest P2K corner is found by
response similarity, then both cascades are listed root by root.
"""
import json
import pathlib
import struct
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "out/build/windows-msvc-release/native/research"))
import trench_native_research as core  # noqa: E402

P2K_SR = core.kP2kDatumHz
MOR_SR = core.kMorpheusDatumHz
GRID = [40.0 * (17000.0 / 40.0) ** (i / 255.0) for i in range(256)]

PAIRS = [
    ("P2k_003_millennium.bin", "standard/052_4_PoleMidQ.4.json"),
    ("P2k_024_cruz_pusher.bin", "standard/053_2pole_4pole.json"),
]


def p2k_rows(raw, corner):
    w = struct.unpack("<120H", raw)
    return [list(w[corner * 30 + s * 5: corner * 30 + s * 5 + 5]) for s in range(6)]


def resp(rows, sr):
    return np.asarray(core.cascade_db([v for r in rows for v in r], GRID, sr))


def norm(v):
    v = v - v.mean()
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v


def describe(root):
    k = root["kind"]
    if k == "conjugate":
        return f"{root['hz']:8.1f} Hz r={root['radius']:.4f}"
    if k == "real":
        return f"real {root}"
    return "      --            "


def classify(g, sr):
    p, z = g["pole"], g["zero"]
    if p["kind"] != "conjugate" and z["kind"] != "conjugate":
        return "identity"
    if p["kind"] == "conjugate" and z["kind"] != "conjugate":
        return "pole-only"
    if z["kind"] == "conjugate" and p["kind"] != "conjugate":
        return "zero-only"
    ratio = z["hz"] / max(p["hz"], 1.0)
    if z["hz"] > 0.47 * sr:
        return "lowpass"
    if z["hz"] < 30.0:
        return "highpass"
    if 1 / 1.35 < ratio < 1.35:
        return "bell-cut" if z["radius"] > p["radius"] else "bell-boost"
    return "shelf/split"


def table(rows, sr, label):
    print(f"    {label}")
    for i, words in enumerate(rows):
        g = core.geometry(words, sr)
        db = np.asarray(core.section_db(words, GRID, sr))
        span = float(db.max() - db.min()) if np.all(np.isfinite(db)) else float("nan")
        sc = 20 * np.log10(max(g["scale"], 1e-9))
        print(f"      s{i + 1}  {classify(g, sr):<11} pole {describe(g['pole'])}   zero {describe(g['zero'])}"
              f"   scale {sc:6.1f} dB   span {span:5.1f} dB")


for p2k_file, mor_path in PAIRS:
    raw = (ROOT / "ref/presets" / p2k_file).read_bytes()
    doc = json.load(open(ROOT / "ref/morpheus_decoded" / mor_path))
    print(f"\n==== {p2k_file}  (44,100 Hz, 6 sections)   vs   {doc['manual_name']}  (39,062.5 Hz, 7 sections)")
    p2k = [(c, norm(resp(p2k_rows(raw, c), P2K_SR))) for c in range(4)]
    shown = set()
    for corner in doc["corner_data"]:
        mrows = [s["words"] for s in corner["sections"]]
        mv = resp(mrows, MOR_SR)
        if not np.all(np.isfinite(mv)):
            continue
        mu = norm(mv)
        c, s = max(((c, float(mu @ u)) for c, u in p2k), key=lambda t: t[1])
        print(f"\n  Morpheus corner {corner['corner']}  <->  P2K corner {c}   cos {s:.3f}")
        if (corner["corner"], c) in shown:
            continue
        shown.add((corner["corner"], c))
        table(mrows, MOR_SR, "Morpheus")
        table(p2k_rows(raw, c), P2K_SR, "P2K")
