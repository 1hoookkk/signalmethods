"""Axis-edge ground truth: deltas, static scaffolds, operator centroids,
and the parallelogram closure test. Everything recomputed here, nothing taken
on faith."""
import os
import json
import glob
import math
import itertools
import numpy as np
import polars as pl
from collections import Counter, defaultdict
from scipy.spatial.distance import cdist

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plotdata", "emu")
FS = 39062.5
NYQ = FS / 2.0
IDLE = (1909, 2015)
LOG = []


def say(s=""):
    print(s)
    LOG.append(s)


def st(a, b):
    return 12.0 * math.log2(max(b, 1e-9) / max(a, 1e-9))


def load():
    objs = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        corners = []
        for cor in c["corners"]:
            secs = []
            for s in cor["sections"]:
                w = s["raw"]
                secs.append(dict(
                    fp=s["pole"]["hz"], rp=s["pole"]["r"],
                    fz=s["zero"]["hz"], rz=s["zero"]["r"],
                    key=tuple(w),
                    dead=(w[1] == 2047 and w[3] == 2047)))
            corners.append(secs)
        objs.append(dict(corpus="morpheus", name=c["name"], ns=7,
                         corners=corners, axes={0: "T", 1: "M", 2: "Q"}))
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        corners = []
        for cn in ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]:
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
                secs.append(dict(fp=fp, rp=rp, fz=fz, rz=rz,
                                 key=(round(fp, 4), round(rp, 6),
                                      round(fz, 4), round(rz, 6)),
                                 dead=(rp < 1e-6 and rz < 1e-6)))
            corners.append(secs)
        objs.append(dict(corpus="p2k", name=p["name"], ns=6,
                         corners=corners, axes={0: "M", 1: "Q"}))
    return objs


OBJ = load()
say("=" * 78)
say("1. CORPUS INVENTORY (recomputed)")
mor = [o for o in OBJ if o["corpus"] == "morpheus"]
p2k = [o for o in OBJ if o["corpus"] == "p2k"]
say(f"  Morpheus 1993 : {len(mor)} cubes x 8 corners x 7 stages = "
    f"{len(mor)*8*7:,} stage instances")
say(f"  P2K 1999      : {len(p2k)} presets x 4 corners x 6 stages = "
    f"{len(p2k)*4*6:,} stage instances")
say(f"  total objects : {len(OBJ)}   corners {len(mor)*8 + len(p2k)*4:,}   "
    f"stage instances {len(mor)*8*7 + len(p2k)*4*6:,}")
voc = json.load(open(os.path.join(ROOT, "ref/stage_vocabulary.json"),
                    encoding="utf-8"))
alp = json.load(open(os.path.join(ROOT, "recipes/alphabet.json"),
                    encoding="utf-8"))
say(f"  vocabulary shapes {len(voc['shapes'])}   letter_shapes "
    f"{len(voc['letter_shapes'])}   alphabet anchors {len(alp['anchors'])}")
say(f"  datum {FS:g} Hz")


def null_corner(secs):
    return all(s["dead"] for s in secs)


# ------------------------------------------------- 2. axis edge deltas
say()
say("2. AXIS EDGE DELTAS   (edges where both corners are live)")
edge_rows = []
for o in OBJ:
    n = len(o["corners"])
    for bit, axis in o["axes"].items():
        for i in range(n):
            if i & (1 << bit):
                continue
            j = i | (1 << bit)
            if j >= n:
                continue
            A, B = o["corners"][i], o["corners"][j]
            if null_corner(A) or null_corner(B):
                continue
            mutated = sum(1 for a, b in zip(A, B) if a["key"] != b["key"])
            dsp, dsz, drp, drz = [], [], [], []
            for a, b in zip(A, B):
                if a["rp"] > 0.05 and b["rp"] > 0.05:
                    dsp.append(st(a["fp"], b["fp"]))
                    drp.append(b["rp"] - a["rp"])
                if a["rz"] > 0.05 and b["rz"] > 0.05:
                    dsz.append(st(a["fz"], b["fz"]))
                    drz.append(b["rz"] - a["rz"])
            edge_rows.append(dict(
                corpus=o["corpus"], name=o["name"], axis=axis, src=i, dst=j,
                ns=o["ns"], mutated=mutated,
                med_dst_p=float(np.median(dsp)) if dsp else None,
                med_dst_z=float(np.median(dsz)) if dsz else None,
                med_dr_p=float(np.median(drp)) if drp else None,
                med_dr_z=float(np.median(drz)) if drz else None,
                mean_abs_dst_p=float(np.mean(np.abs(dsp))) if dsp else None,
                mean_abs_dr_p=float(np.mean(np.abs(drp))) if drp else None,
                mean_abs_dr_z=float(np.mean(np.abs(drz))) if drz else None))
E = pl.DataFrame(edge_rows)
E.write_parquet(os.path.join(OUT, "axis_edges.parquet"))
say(f"  live edges: {E.height}  " +
    "  ".join(f"{a}={E.filter(pl.col('axis') == a).height}"
             for a in ("M", "Q", "T")))
say()
say(f"  {'stat':26s}" + "".join(f"{a+' axis':>16s}" for a in ("M", "Q", "T")))
for lab, col, fmt in (("median pole shift (st)", "med_dst_p", "{:+.2f}"),
                      ("median zero shift (st)", "med_dst_z", "{:+.2f}"),
                      ("median pole radius d", "med_dr_p", "{:+.4f}"),
                      ("median zero radius d", "med_dr_z", "{:+.4f}"),
                      ("mean |pole shift| st", "mean_abs_dst_p", "{:.2f}"),
                      ("mean |pole radius d|", "mean_abs_dr_p", "{:.4f}"),
                      ("mean |zero radius d|", "mean_abs_dr_z", "{:.4f}")):
    cells = []
    for a in ("M", "Q", "T"):
        s = E.filter((pl.col("axis") == a) & pl.col(col).is_not_null())[col]
        cells.append(fmt.format(float(np.median(s.to_numpy())) if "median" in lab
                                else float(np.mean(s.to_numpy())))
                     if s.len() else "n/a")
    say(f"  {lab:26s}" + "".join(f"{c:>16s}" for c in cells))
say()
say("  stages mutated per edge (% of that axis's edges)")
for a in ("M", "Q", "T"):
    sub = E.filter(pl.col("axis") == a)
    if not sub.height:
        continue
    h = Counter(sub["mutated"].to_list())
    line = "  ".join(f"{k}:{100*v/sub.height:4.1f}%" for k, v in sorted(h.items()))
    say(f"    {a} (n={sub.height:4d})  {line}")

# ------------------------------------------------- 3. static scaffolds
say()
say("3. STATIC SCAFFOLDS   (stage identical across every live corner of a cube)")
scaf = Counter()
examples = defaultdict(list)
locked_hist = Counter()
for o in OBJ:
    live = [c for c in o["corners"] if not null_corner(c)]
    if len(live) < 2:
        continue
    frozen = []
    for k in range(o["ns"]):
        a = live[0][k]
        ok = all(abs(c[k]["fp"] - a["fp"]) <= 2.0 and
                 abs(c[k]["rp"] - a["rp"]) <= 0.005 and
                 abs(c[k]["fz"] - a["fz"]) <= 2.0 and
                 abs(c[k]["rz"] - a["rz"]) <= 0.005 for c in live[1:])
        if ok and not a["dead"]:
            frozen.append(k)
    locked_hist[len(frozen)] += 1
    if frozen:
        key = "+".join(f"S{k+1}" for k in frozen)
        scaf[key] += 1
        examples[key].append(o["name"])
tot_obj = len(OBJ)
say(f"  objects with >=1 frozen live stage: "
    f"{sum(v for k, v in locked_hist.items() if k >= 1)} "
    f"({100*sum(v for k, v in locked_hist.items() if k >= 1)/tot_obj:.1f}%)")
say(f"  objects with >=3 frozen live stages: "
    f"{sum(v for k, v in locked_hist.items() if k >= 3)} "
    f"({100*sum(v for k, v in locked_hist.items() if k >= 3)/tot_obj:.1f}%)")
say("  most common frozen sets:")
for key, n in scaf.most_common(8):
    say(f"    {key:24s} {n:3d} objects   e.g. {', '.join(examples[key][:4])}")

# ------------------------------------------------- 4. operator centroids
say()
say("4. CORNER OPERATOR CENTROIDS   (k-means on per-edge lane delta vectors)")
vecs, meta = [], []
for r in edge_rows:
    pass
for o in OBJ:
    n = len(o["corners"])
    for bit, axis in o["axes"].items():
        for i in range(n):
            if i & (1 << bit):
                continue
            j = i | (1 << bit)
            if j >= n or null_corner(o["corners"][i]) or \
                    null_corner(o["corners"][j]):
                continue
            A, B = o["corners"][i], o["corners"][j]
            v = []
            for k in range(6):
                a, b = A[k], B[k]
                dsp = st(a["fp"], b["fp"]) if (a["rp"] > 0.05 and
                                               b["rp"] > 0.05) else 0.0
                dsz = st(a["fz"], b["fz"]) if (a["rz"] > 0.05 and
                                               b["rz"] > 0.05) else 0.0
                v += [np.clip(dsp, -72, 72) / 72.0, b["rp"] - a["rp"],
                      np.clip(dsz, -72, 72) / 72.0, b["rz"] - a["rz"]]
            vecs.append(v)
            meta.append((axis, o["name"]))
X = np.array(vecs)


def kmeans(X, k, seed):
    rng = np.random.default_rng(seed)
    C = X[rng.choice(len(X), k, replace=False)]
    lab = None
    for _ in range(120):
        nl = cdist(X, C).argmin(axis=1)
        if lab is not None and np.array_equal(nl, lab):
            break
        lab = nl
        for c in range(k):
            if (lab == c).any():
                C[c] = X[lab == c].mean(axis=0)
    return lab, C


K = 8
lab, C = kmeans(X, K, 5)
say(f"  {len(X)} edge vectors, k={K}")
for c in np.argsort(-np.bincount(lab, minlength=K)):
    m = lab == c
    if not m.any():
        continue
    ax = Counter(meta[i][0] for i in np.where(m)[0]).most_common(1)[0]
    prof = np.median(X[m], axis=0)
    say(f"    C{c}  {100*m.mean():5.1f}% ({int(m.sum()):4d} edges)  "
        f"dominant axis {ax[0]} ({100*ax[1]/m.sum():.0f}%)")
    for k in range(6):
        dsp, drp, dsz, drz = prof[4 * k:4 * k + 4]
        if abs(dsp * 72) < 0.5 and abs(drp) < 0.01 and abs(dsz * 72) < 0.5:
            continue
        say(f"         S{k+1}: pole {dsp*72:+6.1f} st (dR {drp:+.3f})   "
            f"zero {dsz*72:+6.1f} st (dR {drz:+.3f})")

# ------------------------------------------------- 5. parallelogram closure
say()
say("5. PARALLELOGRAM CLOSURE   C_ab  vs  C_00 + (C_a0-C_00) + (C_0b-C_00)")
errs_f, errs_r, faces = [], [], 0
strict = 0
for o in OBJ:
    n = len(o["corners"])
    bits = sorted(o["axes"])
    for b1, b2 in itertools.combinations(bits, 2):
        for base in range(n):
            if base & (1 << b1) or base & (1 << b2):
                continue
            c00 = o["corners"][base]
            c10 = o["corners"][base | (1 << b1)]
            c01 = o["corners"][base | (1 << b2)]
            c11 = o["corners"][base | (1 << b1) | (1 << b2)]
            if any(null_corner(c) for c in (c00, c10, c01, c11)):
                continue
            faces += 1
            ef, er = 0.0, 0.0
            for k in range(o["ns"]):
                if not all(c[k]["rp"] > 0.05 for c in (c00, c10, c01, c11)):
                    continue
                pred = (st(c00[k]["fp"], c10[k]["fp"]) +
                        st(c00[k]["fp"], c01[k]["fp"]))
                act = st(c00[k]["fp"], c11[k]["fp"])
                ef = max(ef, abs(pred - act))
                predr = ((c10[k]["rp"] - c00[k]["rp"]) +
                         (c01[k]["rp"] - c00[k]["rp"]))
                actr = c11[k]["rp"] - c00[k]["rp"]
                er = max(er, abs(predr - actr))
            errs_f.append(ef)
            errs_r.append(er)
            if ef < 0.5 and er < 0.01:
                strict += 1
errs_f = np.array(errs_f)
errs_r = np.array(errs_r)
say(f"  faces tested (all four corners live): {faces}")
say(f"  strictly additive (max pole err <0.5 st and dR <0.01): "
    f"{strict} ({100*strict/max(faces,1):.1f}%)")
say(f"  median max pole-frequency interaction: {np.median(errs_f):.2f} st")
say(f"  median max pole-radius interaction:    {np.median(errs_r):.4f}")
say(f"  p90 pole interaction: {np.percentile(errs_f, 90):.2f} st")

open(os.path.join(ROOT, "plots", "corpus", "axis_ground_truth.txt"),
     "w").write("\n".join(LOG))
print("\nwrote plots/corpus/axis_ground_truth.txt")
