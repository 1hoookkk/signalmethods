"""Stage bank keyed by transfer function, with the operation as an attribute.

The bank is mined from the corpus, so a corpus object must resolve against it
totally and exactly. That requires a context-free key: the stage's own biquad
coefficients. What a stage DOES depends on the stages before it, so the
operation cannot be the key -- it is recorded per entry as a distribution over
the contexts the corpus actually puts that stage in.

    key      = rounded (b0,b1,b2,a1,a2) of H_k(z)
    entry    = seat geometry + evidence + operation distribution

Schema keeps shapes / positions / coverage so workstation/js/blocks.js still
consumes it; `by_tf` is added for exact geometry->entry resolution on load.
"""
import os
import json
import glob
import math
import shutil
import struct
import numpy as np
from collections import Counter, defaultdict
from scipy.signal import find_peaks

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET = os.path.join(ROOT, "ref", "stage_vocabulary.json")
BACKUP_OP = os.path.join(ROOT, "ref", "stage_vocabulary.operations.json")
FS = 39062.5
NYQ = FS / 2.0
GRID = np.geomspace(40.0, 16000.0, 2048)
LOG2F = np.log2(GRID)
W = 2.0 * math.pi * GRID / FS
E1, E2 = np.exp(-1j * W), np.exp(-2j * W)
LOW = GRID <= 80.0
HIGH = GRID >= 12000.0
PROM = 3.0
MATCH_ST = 3.0
EDGE = 4.0
IDENTITY = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)


# ------------------------------------------------------------ TF translation
def conj_poly(hz, r):
    w = 2.0 * math.pi * hz / FS
    return (1.0, -2.0 * r * math.cos(w), r * r)


def real_poly(a, b):
    return (1.0, -(a + b), a * b)


def decode_u16(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))


def kernel_biquad(words):
    d = [decode_u16(x) for x in words]
    k = (4.0 * d[0] + d[1], d[1], 4.0 * d[2] + d[3], d[3], 4.0 * d[4])
    return ((k[4], (k[0] - 2.0) * k[4], (1.0 - k[1]) * k[4]),
            (1.0, k[2] - 2.0, 1.0 - k[3]))


def tf_key(b, a):
    return "|".join(f"{v:.6f}" for v in (b[0], b[1], b[2], a[1], a[2]))


def tf_db(b, a):
    num = b[0] + b[1] * E1 + b[2] * E2
    den = 1.0 + a[1] * E1 + a[2] * E2
    return 20.0 * np.log10(np.maximum(np.abs(num), 1e-30) /
                           np.maximum(np.abs(den), 1e-30))


# ----------------------------------------------------------------- operation
def landmarks(c):
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
    return len(b) - len(used), len(a) - len(used)


OPERATIONS = [
    ("bypass", "H(z) = 1 exactly: the identity row the corpus parks in "
     "unused stage slots"),
    ("transparent", "leaves the running cascade where it was"),
    ("resonance", "adds a resonant peak and little else"),
    ("resonance + guard zero", "adds a resonant peak with a companion null"),
    ("resonance over low cut", "adds a peak while removing level below 80 Hz"),
    ("resonance over high cut", "adds a peak while removing level above 12 kHz"),
    ("null", "carves a null, adds no resonance"),
    ("null relocation", "removes one null and opens another elsewhere"),
    ("resonance removal", "cancels a resonance the earlier stages built"),
    ("low cut", "removes level below 80 Hz"),
    ("low boost", "adds level below 80 Hz"),
    ("high cut", "removes level above 12 kHz"),
    ("high boost", "adds level above 12 kHz"),
    ("broadband tilt", "moves the two band edges in opposite directions"),
    ("broadband level", "moves the whole response together"),
]


def classify(pa, pr, va, vr, dlevel, dlow, dhigh):
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
    if lo_cut and hi_up or lo_up and hi_cut:
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


# -------------------------------------------------------------------- loader
def corners():
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            st = []
            for s in cor["sections"]:
                p, z = s["pole"], s["zero"]
                st.append((conj_poly(z["hz"], z["r"]), conj_poly(p["hz"], p["r"]),
                           (p["hz"], p["r"], z["hz"], z["r"])))
            yield "morpheus-rom", c["name"], ci, st
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            st = []
            for sec in p["sections"]:
                g = sec["corners"][cn]

                def poly(x):
                    return (real_poly(*x["pair"]) if "pair" in x
                            else conj_poly(x["hz"], x["r"]))

                def geo(x):
                    if "pair" in x:
                        a, b = x["pair"]
                        return (0.3 if a + b >= 0 else NYQ,
                                min(math.sqrt(abs(a * b)), 0.99999))
                    return (x["hz"], x["r"])
                sc = float(g.get("scale", 1.0))
                nb = poly(g["zero"])
                st.append(((sc * nb[0], sc * nb[1], sc * nb[2]), poly(g["pole"]),
                           geo(g["pole"]) + geo(g["zero"])))
            yield "p2k-recipe", p["name"], ci, st
    seen = set()
    for pat in ("ref/presets/*.bin", "ref/cubes/*.body",
                "recipes/hero/*.body", "recipes/extrusions/*.body"):
        for fn in sorted(glob.glob(os.path.join(ROOT, pat))):
            raw = open(fn, "rb").read()
            if raw in seen or len(raw) not in (240, 560):
                continue
            seen.add(raw)
            nc, ns = (8, 7) if len(raw) == 560 else (4, 6)
            w = struct.unpack(f"<{len(raw)//2}H", raw)
            nm = os.path.splitext(os.path.basename(fn))[0]
            for c in range(nc):
                words = [tuple(w[(c * ns + s) * 5:(c * ns + s) * 5 + 5])
                         for s in range(ns)]
                if all(x == IDENTITY for x in words):
                    continue
                st = []
                for x in words:
                    b, a = kernel_biquad(x)
                    st.append((b, a, None))
                yield "native-body", nm, c, st


def main():
    entries = defaultdict(lambda: dict(seat=None, occ=0, filters=set(),
                                       positions=Counter(), ops=Counter(),
                                       sources=Counter()))
    per_stage = Counter()
    for src, name, ci, stages in corners():
        cum = np.zeros(len(GRID))
        prev = landmarks(cum)
        for k, (b, a, geo) in enumerate(stages):
            per_stage[k + 1] += 1
            db = tf_db(b, a)
            if float(np.abs(db).max()) < 1e-9:
                # H(z) = 1 exactly. It still has a transfer function and a
                # loaded object still contains it, so it must be in the bank.
                op = "bypass"
            else:
                cum = cum + db
                cur = landmarks(cum)
                pa, pr = count_match(prev[0], cur[0])
                va, vr = count_match(prev[1], cur[1])
                op = classify(pa, pr, va, vr, cur[3] - prev[3],
                              cur[4] - prev[4], cur[5] - prev[5])
                prev = cur
            e = entries[tf_key(b, a)]
            if e["seat"] is None and geo:
                e["seat"] = geo
            e["occ"] += 1
            e["filters"].add(name)
            e["positions"][k + 1] += 1
            e["ops"][op] += 1
            e["sources"][src] += 1

    if os.path.exists(TARGET) and not os.path.exists(BACKUP_OP):
        shutil.copy2(TARGET, BACKUP_OP)

    by_tf, pos_entries = {}, defaultdict(list)
    for i, (key, e) in enumerate(sorted(entries.items(),
                                        key=lambda t: -t[1]["occ"])):
        eid = f"g{i:05d}"
        by_tf[key] = eid
        seat = e["seat"] or (0.0, 0.0, 0.0, 0.0)
        dom, domn = e["ops"].most_common(1)[0]
        rec = {
            "name": eid,
            "tf": [round(float(x), 6) for x in key.split("|")],
            "seat": {"pole_hz": round(seat[0], 2), "pole_r": round(seat[1], 6),
                     "zero_hz": round(seat[2], 2), "zero_r": round(seat[3], 6)},
            "operation": dom,
            "operation_share": round(domn / e["occ"], 3),
            "operations": dict(e["ops"].most_common()),
            "evidence": {
                "occurrences": e["occ"],
                "distinct_filters": len(e["filters"]),
                "by_corpus": dict(e["sources"]),
                "corpora_spanned": len(e["sources"]),
                "exemplar_filters": sorted(e["filters"])[:8],
                "clusters": [], "letters": [],
            },
        }
        for st in e["positions"]:
            pos_entries[st].append(rec)

    positions = {}
    for st in sorted(per_stage):
        lst = sorted(pos_entries.get(st, []),
                     key=lambda r: -r["evidence"]["occurrences"])
        positions[f"S{st}"] = {"stage_instances": per_stage[st], "shapes": lst}

    out = {
        "contract": "stage bank keyed by transfer function. key = rounded "
                    "(b0,b1,b2,a1,a2) of H_k(z) at the 39,062.5 Hz datum, so "
                    "any corpus stage resolves exactly and totally. The "
                    "operation a stage performs depends on the stages before "
                    "it, so it is carried per entry as a distribution over the "
                    "contexts the corpus puts that stage in, never as the key.",
        "generated_by": "dev/derive_tf_bank.py",
        "supersedes": "ref/stage_vocabulary.operations.json (operation-keyed), "
                      "ref/stage_vocabulary.silhouette.json (shape-named)",
        "datum_sr_hz": FS,
        "shapes": {n: {"structure": d, "free": [], "freedom": [],
                       "slaving": None} for n, d in OPERATIONS},
        "by_tf": by_tf,
        "positions": positions,
        "coverage": {f"S{st}": {"instances": per_stage[st],
                                "classified": per_stage[st], "share": 1.0}
                     for st in sorted(per_stage)},
    }
    json.dump(out, open(TARGET, "w", encoding="utf-8"), separators=(",", ":"))

    amb = sum(1 for e in entries.values() if len(e["ops"]) > 1)
    print(f"wrote {TARGET}  ({os.path.getsize(TARGET)/1e6:.1f} MB)")
    print(f"distinct transfer functions : {len(entries)}")
    print(f"stage instances             : {sum(e['occ'] for e in entries.values())}")
    print(f"entries whose operation varies with context: {amb} "
          f"({100*amb/len(entries):.1f}%)  -- carried as a distribution")
    print(f"entries spanning >1 corpus  : "
          f"{sum(1 for e in entries.values() if len(e['sources'])>1)}")
    print()
    print("dominant operation across the bank:")
    agg = Counter()
    for e in entries.values():
        agg[e["ops"].most_common(1)[0][0]] += e["occ"]
    tot = sum(agg.values())
    for op, n in agg.most_common():
        print(f"  {op:<26}{n:7d}  {100*n/tot:5.1f}%")


if __name__ == "__main__":
    main()
