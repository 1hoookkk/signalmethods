"""Rebuild ref/stage_vocabulary.json so the CASCADE menu lists what each stage
DOES to the running cascade, not what its isolated silhouette looks like.

A stage's class is decided by the landmark change it causes in C_{k-1} -> C_k:
peaks/valleys added or removed, broadband tilt change, level change. Geometry
is still carried (the seat), so seating a menu entry works exactly as before.

Schema is unchanged: shapes / positions / coverage / cluster_shapes /
letter_shapes, so workstation/js/blocks.js consumes it untouched.
"""
import os
import json
import glob
import math
import shutil
import numpy as np
from collections import Counter, defaultdict
from scipy.signal import find_peaks

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.path.join(ROOT, "ref", "stage_vocabulary.json")
BACKUP = os.path.join(ROOT, "ref", "stage_vocabulary.silhouette.json")
FS = 39062.5
NYQ = FS / 2.0
GRID = np.geomspace(40.0, 16000.0, 2048)
LOG2F = np.log2(GRID)
W = 2.0 * math.pi * GRID / FS
E1, E2 = np.exp(-1j * W), np.exp(-2j * W)
PROM = 3.0
MATCH_ST = 3.0


def sec_db(fp, rp, fz, rz):
    num = 1 - 2 * rz * math.cos(2 * math.pi * fz / FS) * E1 + rz ** 2 * E2
    den = 1 - 2 * rp * math.cos(2 * math.pi * fp / FS) * E1 + rp ** 2 * E2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


LOW = GRID <= 80.0
HIGH = GRID >= 12000.0


def landmarks(c):
    """Interior extrema PLUS the two band edges. A zero low enough to act as a
    roll-off makes no local minimum, so edge level is the only channel that
    can see it."""
    pk, _ = find_peaks(c, prominence=PROM)
    vl, _ = find_peaks(-c, prominence=PROM)
    return (GRID[pk], GRID[vl], float(np.polyfit(LOG2F, c, 1)[0]),
            float(c.mean()), float(c[LOW].mean()), float(c[HIGH].mean()))


def count_match(a, b):
    used = set()
    for fa in a:
        best, bd = None, MATCH_ST
        for j, fb in enumerate(b):
            if j in used:
                continue
            d = abs(12 * math.log2(max(fb, 1) / max(fa, 1)))
            if d < bd:
                bd, best = d, j
        if best is not None:
            used.add(best)
    return len(b) - len(used), len(a) - len(used)   # added, removed


OPERATIONS = [
    ("transparent", "leaves the running cascade where it was: no resonance, "
     "no null, no shelf movement at either band edge"),
    ("resonance", "adds a resonant peak and little else"),
    ("resonance + guard zero", "adds a resonant peak with a companion null "
     "beside it — the chained pole/zero rung"),
    ("resonance over low cut", "adds a resonant peak while removing level "
     "below 80 Hz: presence plus headroom, in one section"),
    ("resonance over high cut", "adds a resonant peak while removing level "
     "above 12 kHz"),
    ("null", "carves a null into the running cascade without adding a "
     "resonance"),
    ("null relocation", "removes one null and opens another elsewhere"),
    ("resonance removal", "cancels a resonance the earlier stages had built"),
    ("low cut", "removes level below 80 Hz, no new resonance or null"),
    ("low boost", "adds level below 80 Hz, no new resonance or null"),
    ("high cut", "removes level above 12 kHz, no new resonance or null"),
    ("high boost", "adds level above 12 kHz, no new resonance or null"),
    ("broadband tilt", "moves both band edges in opposite directions without "
     "adding a resonance or a null"),
    ("broadband level", "moves the whole response up or down together"),
]
OP_NAMES = [n for n, _ in OPERATIONS]


EDGE = 4.0          # dB at a band edge before it counts as a shelf move


def classify(pa, pr, va, vr, dslope, dlevel, dlow, dhigh, stage, nstages):
    lo_cut, lo_up = dlow <= -EDGE, dlow >= EDGE
    hi_cut, hi_up = dhigh <= -EDGE, dhigh >= EDGE
    if pr >= 1 and pa == 0:
        return "resonance removal"
    if va >= 1 and vr >= 1:
        return "null relocation"
    if pa >= 1:
        if lo_cut and not hi_cut:
            return "resonance over low cut"
        if hi_cut and not lo_cut:
            return "resonance over high cut"
        if va >= 1:
            return "resonance + guard zero"
        return "resonance"
    if va >= 1:
        return "null"
    if lo_cut and hi_up:
        return "broadband tilt"
    if lo_up and hi_cut:
        return "broadband tilt"
    if lo_cut:
        return "low cut"
    if lo_up:
        return "low boost"
    if hi_cut:
        return "high cut"
    if hi_up:
        return "high boost"
    if abs(dlevel) > 1.5:
        return "broadband level"
    return "transparent"


def load():
    out = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            secs = []
            for s in cor["sections"]:
                w = s["raw"]
                dead = (w[1] == 2047 and w[3] == 2047)
                secs.append(None if dead else (s["pole"]["hz"], s["pole"]["r"],
                                               s["zero"]["hz"], s["zero"]["r"]))
            if any(s for s in secs):
                out.append(("morpheus", c["name"], ci, secs))
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            secs = []
            for sec in p["sections"]:
                g = sec["corners"][cn]

                def rd(x):
                    if "pair" in x:
                        a, b = x["pair"]
                        return (0.3 if (a + b) >= 0 else NYQ,
                                min(math.sqrt(abs(a * b)), 0.99999))
                    return (x["hz"], min(x["r"], 0.99999))
                fp, rp = rd(g["pole"])
                fz, rz = rd(g["zero"])
                secs.append(None if (rp < 1e-6 and rz < 1e-6) else
                            (fp, rp, fz, rz))
            if any(s for s in secs):
                out.append(("p2k", p["name"], ci, secs))
    return out


def main():
    corners = load()
    inst = defaultdict(list)          # (stage, op) -> list of instances
    per_stage_total = Counter()
    for corpus, name, ci, secs in corners:
        cum = np.zeros(len(GRID))
        prev = landmarks(cum)
        for k, s in enumerate(secs):
            per_stage_total[k + 1] += 1
            if s is None:
                continue
            cum = cum + sec_db(*s)
            cur = landmarks(cum)
            pa, pr = count_match(prev[0], cur[0])
            va, vr = count_match(prev[1], cur[1])
            op = classify(pa, pr, va, vr, cur[2] - prev[2], cur[3] - prev[3],
                          cur[4] - prev[4], cur[5] - prev[5],
                          k + 1, len(secs))
            inst[(k + 1, op)].append(dict(corpus=corpus, name=name, corner=ci,
                                          seat=s, dslope=cur[2] - prev[2],
                                          dlevel=cur[3] - prev[3],
                                          dlow=cur[4] - prev[4],
                                          dhigh=cur[5] - prev[5]))
            prev = cur

    old = json.load(open(TARGET, encoding="utf-8"))
    if not os.path.exists(BACKUP):
        shutil.copy2(TARGET, BACKUP)

    shapes = {}
    for nm, desc in OPERATIONS:
        shapes[nm] = {"structure": desc, "free": [], "freedom": [],
                      "slaving": None}

    positions, coverage = {}, {}
    for st in sorted(per_stage_total):
        entries = []
        classified = 0
        for nm, _ in OPERATIONS:
            rows = inst.get((st, nm), [])
            if not rows:
                continue
            classified += len(rows)
            seats = np.array([r["seat"] for r in rows])
            med = np.median(seats, axis=0)
            j = int(np.argmin(np.abs(np.log2(np.maximum(seats[:, 0], 1e-9) /
                                             max(med[0], 1e-9))) +
                              np.abs(seats[:, 1] - med[1])))
            seat = rows[j]["seat"]
            names = sorted({r["name"] for r in rows})
            entries.append({
                "name": nm,
                "seat": {"pole_hz": round(seat[0], 2),
                         "pole_r": round(seat[1], 6),
                         "zero_hz": round(seat[2], 2),
                         "zero_r": round(seat[3], 6)},
                "ranges": {
                    "d_slope_db_per_oct": {
                        "min": round(float(np.min([r["dslope"] for r in rows])), 2),
                        "median": round(float(np.median([r["dslope"] for r in rows])), 2),
                        "max": round(float(np.max([r["dslope"] for r in rows])), 2)},
                    "d_low_edge_db": {
                        "min": round(float(np.min([r["dlow"] for r in rows])), 2),
                        "median": round(float(np.median([r["dlow"] for r in rows])), 2),
                        "max": round(float(np.max([r["dlow"] for r in rows])), 2)},
                    "d_high_edge_db": {
                        "min": round(float(np.min([r["dhigh"] for r in rows])), 2),
                        "median": round(float(np.median([r["dhigh"] for r in rows])), 2),
                        "max": round(float(np.max([r["dhigh"] for r in rows])), 2)},
                    "d_level_db": {
                        "min": round(float(np.min([r["dlevel"] for r in rows])), 2),
                        "median": round(float(np.median([r["dlevel"] for r in rows])), 2),
                        "max": round(float(np.max([r["dlevel"] for r in rows])), 2)},
                },
                "evidence": {
                    "occurrences": len(rows),
                    "distinct_filters": len(names),
                    "by_corpus": dict(Counter(r["corpus"] for r in rows)),
                    "corpora_spanned": len({r["corpus"] for r in rows}),
                    "exemplar_filters": names[:8],
                    "clusters": [], "letters": [],
                },
            })
        entries.sort(key=lambda e: -e["evidence"]["occurrences"])
        positions[f"S{st}"] = {"stage_instances": per_stage_total[st],
                               "shapes": entries}
        coverage[f"S{st}"] = {"instances": per_stage_total[st],
                              "classified": classified,
                              "share": round(classified / per_stage_total[st], 4)}

    out = {
        "contract": "stage vocabulary keyed by the OPERATION each stage "
                    "performs on the running cascade C_{k-1} -> C_k "
                    "(resonances and nulls added or removed, and the dB change at "
                    "each band edge), measured on "
                    "a 2048-point log grid 40 Hz-16 kHz at the 39,062.5 Hz "
                    "datum. Seats are the medoid geometry of each class.",
        "generated_by": "dev/derive_operation_vocabulary.py",
        "supersedes": "ref/stage_vocabulary.silhouette.json (shape-name taxonomy)",
        "sources": old.get("sources", []),
        "shapes": shapes,
        "positions": positions,
        "coverage": coverage,
        "cluster_shapes": old.get("cluster_shapes", []),
        "letter_shapes": old.get("letter_shapes", {}),
    }
    json.dump(out, open(TARGET, "w", encoding="utf-8"), indent=1)

    print(f"backup of the old silhouette taxonomy -> {BACKUP}")
    print(f"wrote {TARGET}")
    print(f"operations: {len(OPERATIONS)}   (was {len(old['shapes'])} silhouette shapes)")
    print()
    tot = sum(len(v) for v in inst.values())
    print(f"{'operation':<28}{'total':>7}{'%':>7}   per-stage")
    agg = Counter()
    for (st, nm), rows in inst.items():
        agg[nm] += len(rows)
    for nm, _ in OPERATIONS:
        if not agg[nm]:
            continue
        per = " ".join(f"S{st}:{len(inst.get((st,nm),[]))}"
                       for st in sorted(per_stage_total)
                       if inst.get((st, nm)))
        print(f"{nm:<28}{agg[nm]:7d}{100*agg[nm]/tot:6.1f}%   {per}")
    print()
    for st in sorted(coverage):
        c = coverage[st]
        print(f"  {st} classified {c['classified']}/{c['instances']} "
              f"({100*c['share']:.1f}%)")


if __name__ == "__main__":
    main()
