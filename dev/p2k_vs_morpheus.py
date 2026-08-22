"""P2K (6 sections, 44,100 Hz) against Morpheus (7 sections, 39,062.5 Hz).

Why does the later machine have fewer sections, and what does the reduction
look like?  Each lineage is decoded at its OWN datum -- that is the whole point,
since geometry only means anything against the right rate -- and the resulting
responses are compared on a shared Hz axis, which is legitimate because both
sides are then in real Hz.
"""
import collections
import json
import pathlib
import struct
import sys

import numpy as np

sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
import trench_native_research as core

P2K_SR = core.kP2kDatumHz
MOR_SR = core.kMorpheusDatumHz
IDENT = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)

# shared Hz axis, inside both Nyquists
GRID = [40.0 * (17000.0 / 40.0) ** (i / 255.0) for i in range(256)]

PRESETS = pathlib.Path(r"C:\Users\hooki\trench-native\ref\presets")
DECODED = pathlib.Path(r"C:\Users\hooki\trench-native\ref\morpheus_decoded")


def p2k_corner(raw, corner):
    w = struct.unpack("<120H", raw)
    return [list(w[corner * 30 + s * 5: corner * 30 + s * 5 + 5]) for s in range(6)]


def spans(rows, sr):
    out = []
    for row in rows:
        db = np.asarray(core.section_db(row, GRID, sr))
        out.append(float(db.max() - db.min()) if np.all(np.isfinite(db)) else 0.0)
    return out


def response(rows, sr):
    flat = [v for r in rows for v in r]
    return np.asarray(core.cascade_db(flat, GRID, sr))


def norm(v):
    v = v - v.mean()
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v


print("== 1. how many sections are actually live? (span > 0.5 dB, own datum) ==")
p2k_live = collections.Counter()
p2k_spans = []
p2k_bodies = {}
for p in sorted(PRESETS.glob("P2k_*.bin")):
    raw = p.read_bytes()
    best = [0.0] * 6
    for c in range(4):
        for i, s in enumerate(spans(p2k_corner(raw, c), P2K_SR)):
            best[i] = max(best[i], s)
    p2k_live[sum(1 for s in best if s > 0.5)] += 1
    p2k_spans.append(best)
    p2k_bodies[p.stem] = raw
print(f"   P2K  n={len(p2k_spans):3d}  live-section counts: {dict(sorted(p2k_live.items()))}")

index = json.load(open(DECODED / "index.json"))
mor_live = collections.Counter()
mor_spans = {"square": [], "cube": [], "unknown": []}
for row in index["filters"]:
    mor_live[row["active_sections"]] += 1
print(f"   Morph n={len(index['filters']):3d}  live-section counts: {dict(sorted(mor_live.items()))}")

print("\n== 2. per-section contribution profile, section 1..N ==")
a = np.asarray(p2k_spans)
print("   P2K   median span by section index:",
      "  ".join(f"{np.median(a[:, i]):5.1f}" for i in range(6)))
prof = {"square": [], "cube": []}
for row in index["filters"]:
    if row["geometry"] not in prof:
        continue
    doc = json.load(open(DECODED / row["path"]))
    best = [0.0] * 7
    for c in doc["corner_data"]:
        for sec in c["sections"]:
            best[sec["section"] - 1] = max(best[sec["section"] - 1], sec["span_db"])
    prof[row["geometry"]].append(best)
for k, v in prof.items():
    m = np.asarray(v)
    print(f"   {k:<6}median span by section index:",
          "  ".join(f"{np.median(m[:, i]):5.1f}" for i in range(7)))

print("\n== 3. is any P2K body a close response match to a Morpheus body? ==")
mor_curves = []
for row in index["filters"]:
    doc = json.load(open(DECODED / row["path"]))
    rows = [s["words"] for s in doc["corner_data"][0]["sections"]]
    v = response(rows, MOR_SR)
    if np.all(np.isfinite(v)):
        mor_curves.append((row.get("manual_name") or doc["source_file"],
                           row["geometry"], norm(v)))
print(f"   comparing {len(p2k_bodies)} P2K corner-0 responses against "
      f"{len(mor_curves)} Morpheus corner-0 responses")
print(f'   {"P2K body":<28}{"best Morpheus match":<24}{"geom":<9}{"cos sim":>8}')
sims = []
for name, raw in sorted(p2k_bodies.items()):
    v = response(p2k_corner(raw, 0), P2K_SR)
    if not np.all(np.isfinite(v)):
        continue
    u = norm(v)
    best = max(mor_curves, key=lambda t: float(u @ t[2]))
    s = float(u @ best[2])
    sims.append(s)
    if s > 0.90 or len(sims) <= 8:
        print(f"   {name:<28}{best[0]:<24}{best[1]:<9}{s:8.3f}")
sims = np.asarray(sims)
print(f"\n   cosine similarity: median {np.median(sims):.3f}  max {sims.max():.3f}  "
      f"above 0.95: {(sims > 0.95).sum()}/{len(sims)}")
print("   (1.000 would mean the same response shape; near 1.0 would suggest one")
print("    lineage's bodies are re-authored versions of the other's)")
