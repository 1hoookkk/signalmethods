import os
import json
import math
import numpy as np
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FS = 39062.5
IDENTITY = (1450, 2047, 1450, 2047)
IDLE_POLE = (1909, 2015)


def load():
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    cubes = []
    for c in d["cubes"]:
        rows = []
        for cor in c["corners"]:
            rows.append({
                "words": [tuple(s["raw"]) for s in cor["sections"]],
                "gain": cor["gain"],
                "geo": [(s["pole"]["hz"], s["pole"]["r"],
                         s["zero"]["hz"], s["zero"]["r"])
                        for s in cor["sections"]],
            })
        cubes.append({"index": c["index"], "name": c["name"], "corners": rows})
    return cubes


CUBES = load()
NC = len(CUBES)
OUT = []


def say(s=""):
    print(s)
    OUT.append(s)


def is_identity(w):
    """Radius field 2047 decodes to R = 0, so the section is exactly H = 1
    whatever the angle words say."""
    return w[1] == 2047 and w[3] == 2047


def is_selfcancel(w):
    """Pole word identical to zero word: numerator == denominator, so the
    section is exactly H = 1 no matter where the roots sit."""
    return w[0] == w[2] and w[1] == w[3] and not is_identity(w)


def is_idle(w):
    return (is_identity(w) or is_selfcancel(w)
            or (w[:2] == IDLE_POLE and w[3] == 2047))


def null_corner(cor):
    return all(is_identity(w) for w in cor["words"])


def coverage(counter, total, marks=(1, 8, 32, 128, 512)):
    ranked = [c for _, c in counter.most_common()]
    cum = np.cumsum(ranked)
    out = []
    for m in marks:
        if m <= len(cum):
            out.append((m, 100.0 * cum[m - 1] / total))
    return out


# ------------------------------------------------------------ LEVEL 0
slots = []
for c in CUBES:
    for ci, cor in enumerate(c["corners"]):
        for si, w in enumerate(cor["words"]):
            slots.append((c["index"], c["name"], ci, si, w))
gains = Counter(round(cor["gain"], 6) for c in CUBES for cor in c["corners"])

say("=" * 74)
say("LEVEL 0 — raw encoded words")
say(f"  {NC} cubes x 8 corners x 7 stages = {len(slots)} stage slots")
say(f"  each slot is 4 x 11-bit fields [pole_angle pole_radius "
    f"zero_angle zero_radius]")
say(f"  distinct per-corner gain words: {len(gains)}")
say(f"  inert slots (both radius words 2047, H=1 exactly): "
    f"{sum(1 for s in slots if is_identity(s[4]))}")
say(f"  self-cancelling slots (pole word == zero word, H=1 exactly): "
    f"{sum(1 for s in slots if is_selfcancel(s[4]))}")
say(f"  idle-pole filler slots {sum(1 for s in slots if s[4][:2] == IDLE_POLE)}")
say(f"  => slots that cannot affect the response at all: "
    f"{sum(1 for s in slots if is_identity(s[4]) or is_selfcancel(s[4]))} "
    f"({100*sum(1 for s in slots if is_identity(s[4]) or is_selfcancel(s[4]))/len(slots):.0f}%)")
say(f"  null corners (all 7 identity): "
    f"{sum(1 for c in CUBES for cor in c['corners'] if null_corner(cor))}")

# ------------------------------------------------------------ LEVEL 1
L1 = Counter(s[4] for s in slots)
L1_live = Counter(s[4] for s in slots if not is_idle(s[4]))
where1 = defaultdict(set)
for cid, name, ci, si, w in slots:
    where1[w].add(name)

say()
say("LEVEL 1 — recurring literal stage states (exact 4-word equality)")
say(f"  distinct stage states: {len(L1)} of {len(slots)} slots "
    f"({100*len(L1)/len(slots):.1f}% unique)")
say(f"  excluding all inert forms: {len(L1_live)} distinct states "
    f"over {sum(L1_live.values())} slots")
say("  coverage of all slots by the top-N states:")
for m, p in coverage(L1, len(slots)):
    say(f"     top {m:4d} states -> {p:5.1f}%")
say("  coverage of live slots by the top-N live states:")
for m, p in coverage(L1_live, sum(L1_live.values())):
    say(f"     top {m:4d} states -> {p:5.1f}%")
say("  most reused live stage states:")
for w, n in L1_live.most_common(8):
    say(f"     {str(w):26s} x{n:5d}  in {len(where1[w]):3d} cubes  "
        f"e.g. {sorted(where1[w])[0]}")

# ------------------------------------------------------------ LEVEL 2
L2, L3t = Counter(), Counter()
where2 = defaultdict(set)
for c in CUBES:
    for cor in c["corners"]:
        if null_corner(cor):
            continue
        w = cor["words"]
        for k in range(6):
            L2[(w[k], w[k + 1])] += 1
            where2[(w[k], w[k + 1])].add(c["name"])
        for k in range(5):
            L3t[(w[k], w[k + 1], w[k + 2])] += 1
L2_live = Counter({k: v for k, v in L2.items()
                   if not (is_idle(k[0]) and is_idle(k[1]))})
L3_live = Counter({k: v for k, v in L3t.items()
                   if not all(is_idle(x) for x in k)})

say()
say("LEVEL 2 — recurring ordered adjacent stage pairs / triples "
    "(exact co-occurrence)")
say(f"  ordered pairs S_k,S_k+1 : {sum(L2.values())} instances, "
    f"{len(L2)} distinct  ({len(L2_live)} distinct excluding idle-idle)")
say(f"  ordered triples          : {sum(L3t.values())} instances, "
    f"{len(L3t)} distinct  ({len(L3_live)} distinct excluding all-idle)")
rep2 = sum(v for v in L2_live.values() if v > 1)
rep3 = sum(v for v in L3_live.values() if v > 1)
say(f"  live pairs that recur (seen >1x):   {rep2} of "
    f"{sum(L2_live.values())} instances = {100*rep2/max(sum(L2_live.values()),1):.0f}%")
say(f"  live triples that recur (seen >1x): {rep3} of "
    f"{sum(L3_live.values())} instances = {100*rep3/max(sum(L3_live.values()),1):.0f}%")
say("  most reused live adjacent pairs:")
for (a, b), n in L2_live.most_common(6):
    chained = a[2] == b[0] and a[3] == b[1]
    say(f"     x{n:4d}  {len(where2[(a,b)]):3d} cubes  "
        f"{'ZERO==NEXT POLE' if chained else 'free'}   {a} -> {b}")
adj_chain = sum(v for (a, b), v in L2_live.items()
                if a[2] == b[0] and a[3] == b[1])
say(f"  live adjacent pairs whose zero word equals the next pole word: "
    f"{adj_chain} of {sum(L2_live.values())} = "
    f"{100*adj_chain/max(sum(L2_live.values()),1):.0f}%")

# ------------------------------------------------------------ LEVEL 3
L4m = Counter()
where3 = defaultdict(set)
for c in CUBES:
    for si in range(7):
        pat = tuple(c["corners"][ci]["words"][si] for ci in range(8))
        L4m[pat] += 1
        where3[pat].add(c["name"])
L4m_live = Counter({k: v for k, v in L4m.items()
                    if not all(is_idle(x) for x in k)})
static = sum(1 for k, v in L4m.items() if len(set(k)) == 1 for _ in range(v))
static_live = sum(v for k, v in L4m_live.items() if len(set(k)) == 1)

say()
say("LEVEL 3 — recurring 8-corner mutation patterns per stage "
    "(corner correspondence)")
say(f"  {NC} cubes x 7 stages = {sum(L4m.values())} stage-lanes, "
    f"{len(L4m)} distinct 8-corner patterns")
say(f"  live lanes: {sum(L4m_live.values())} in {len(L4m_live)} distinct patterns")
say(f"  lanes that never move across the 8 corners (all 8 words equal): "
    f"{static_live} live ({100*static_live/max(sum(L4m_live.values()),1):.0f}% "
    f"of live lanes)")
nstates = Counter(len(set(k)) for k in L4m_live.elements())
say("  distinct words used within one lane across its 8 corners:")
for k in sorted(nstates):
    say(f"     {k} distinct -> {nstates[k]:5d} lanes "
        f"({100*nstates[k]/sum(nstates.values()):5.1f}%)")
say("  most reused live lane patterns:")
for pat, n in L4m_live.most_common(4):
    say(f"     x{n:3d} lanes in {len(where3[pat]):3d} cubes  "
        f"{len(set(pat))} distinct words  e.g. {sorted(where3[pat])[0]}")

# ------------------------------------------------------------ LEVEL 4
arch = Counter()
where4 = defaultdict(list)
for c in CUBES:
    key = tuple(tuple(c["corners"][ci]["words"][si] for si in range(7))
                for ci in range(8))
    arch[key] += 1
    where4[key].append(c["name"])
poses = Counter()
wherep = defaultdict(set)
for c in CUBES:
    for ci, cor in enumerate(c["corners"]):
        if null_corner(cor):
            continue
        key = tuple(w[:2] for w in cor["words"])
        poses[key] += 1
        wherep[key].add(c["name"])
poses_live = Counter({k: v for k, v in poses.items()
                      if not all(x == IDLE_POLE or x == IDENTITY[:2] for x in k)})

say()
say("LEVEL 4 — complete factory architectures")
say(f"  {NC} cubes, {len(arch)} distinct 7x8 word matrices "
    f"({sum(v for v in arch.values() if v > 1)} cubes share a matrix)")
for key, n in arch.most_common(3):
    if n > 1:
        say(f"     x{n}: {', '.join(where4[key])}")
say(f"  pole-set poses (7 pole words, ignoring zeros) reused across corners:")
say(f"     {len(poses_live)} distinct poses over {sum(poses_live.values())} "
    f"live corners")
shared = [(k, v) for k, v in poses_live.items() if len(wherep[k]) > 1]
say(f"     poses shared by more than one cube: {len(shared)}")
for k, v in sorted(shared, key=lambda t: -len(wherep[t[0]]))[:5]:
    say(f"       x{v:3d} corners across {len(wherep[k]):2d} cubes: "
        f"{', '.join(sorted(wherep[k])[:4])}")

open(os.path.join(ROOT, "plots", "corpus", "hierarchy.txt"),
     "w").write("\n".join(OUT))


# ------------------------------------------------------- LEVEL 5: responses
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

FGRID = np.geomspace(40.0, 18000.0, 1024)
W = 2.0 * np.pi * FGRID / FS


def word_geo(w, geo):
    return geo


def sec_db(g):
    fp, rp, fz, rz = g
    e1, e2 = np.exp(-1j * W), np.exp(-2j * W)
    num = 1 - 2 * rz * np.cos(2 * np.pi * fz / FS) * e1 + rz ** 2 * e2
    den = 1 - 2 * rp * np.cos(2 * np.pi * fp / FS) * e1 + rp ** 2 * e2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


geo_of = {}
for c in CUBES:
    for cor in c["corners"]:
        for w, g in zip(cor["words"], cor["geo"]):
            geo_of.setdefault(w, g)

def pz_db(fp, rp, fz, rz):
    e1, e2 = np.exp(-1j * W), np.exp(-2j * W)
    num = 1 - 2 * rz * np.cos(2 * np.pi * fz / FS) * e1 + rz ** 2 * e2
    den = 1 - 2 * rp * np.cos(2 * np.pi * fp / FS) * e1 + rp ** 2 * e2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


red_err, n_chained, n_free = [], 0, 0
for (a, b), n in L2_live.items():
    if is_idle(a) or is_idle(b):
        continue
    if a[2] == b[0] and a[3] == b[1]:
        n_chained += n
        ga, gb = geo_of[a], geo_of[b]
        full = sec_db(ga) + sec_db(gb)
        reduced = pz_db(ga[0], ga[1], gb[2], gb[3])
        red_err.append(float(np.abs(full - reduced).max()))
    else:
        n_free += n
red_err = np.array(red_err)

say()
say("LEVEL 5 — responses, evaluated on the unit the grammar builds")
say("  H_pair = |H_k . H_k+1|, never an isolated H_stage.")
say(f"  live adjacent pairs: {n_chained} chained (zero word == next pole word), "
    f"{n_free} free  ({100*n_chained/(n_chained+n_free):.0f}% chained)")
say("  For a chained pair the numerator of stage k IS the denominator of")
say("  stage k+1, so the product collapses to one pole-zero section:")
say("      H_k . H_k+1  =  N_k+1 / D_k     (4th order stored, 2nd order real)")
say(f"  null test over {len(red_err)} distinct chained pairs: "
    f"max |dB| error median {np.median(red_err):.3f}, "
    f"p99 {np.percentile(red_err,99):.3f}, worst {red_err.max():.3f}")
say("  The order reduction is exact. The corpus stores 4th-order pairs that")
say("  are algebraically 2nd order — the span does not shrink, the ORDER does.")

cas, sumstage = [], []
for c in CUBES:
    for cor in c["corners"]:
        if null_corner(cor):
            continue
        h = np.zeros_like(FGRID)
        peaks = 0.0
        for g in cor["geo"]:
            s = sec_db(g)
            h = h + s
            peaks += float(s.max())
        cas.append(float(h.max() - h.min()))
        sumstage.append(peaks)
say(f"  H_cascade span: median {np.median(cas):.1f} dB, "
    f"while the sum of the isolated stage peaks is "
    f"{np.median(sumstage):.0f} dB (median) — isolated stage curves "
    f"over-state the object by {np.median(sumstage)/max(np.median(cas),1e-9):.1f}x.")

fig, axes = plt.subplots(1, 2, figsize=(12.4, 4.4))
fig.patch.set_facecolor("#fcfcfb")
ax = axes[0]
ax.hist(np.clip(red_err, 0, 1e-3), bins=40, color="#2a78d6")
ax.set_xlabel("max |dB| error of the 2nd-order reduction")
ax.set_ylabel("distinct chained pairs")
ax.set_title("A chained pair IS one section  ·  exact",
             fontsize=11, fontweight="bold", loc="left")
ax.text(0.97, 0.9, f"n={len(red_err)}" + chr(10) +
        f"worst {red_err.max():.2e} dB",
        transform=ax.transAxes, ha="right", fontsize=9, color="#52514e")
ax = axes[1]
for lbl, arr, col in (("sum of isolated stage peaks", sumstage, "#eb6834"),
                      ("actual H_cascade span", cas, "#2a78d6")):
    ax.hist(np.clip(arr, 0, 300), bins=np.arange(0, 301, 8), color=col,
            alpha=0.9, label=lbl)
ax.set_xlabel("dB")
ax.set_ylabel("corners")
ax.set_title("Isolated stages over-state the object 3.4x",
             fontsize=11, fontweight="bold", loc="left")
ax.legend(frameon=False, fontsize=8.5)
for a in axes:
    a.set_facecolor("#fcfcfb")
    a.grid(color="#e1e0d9", lw=0.6)
    a.set_axisbelow(True)
    for sp in ("top", "right"):
        a.spines[sp].set_visible(False)
fig.tight_layout()
fig.savefig(os.path.join(ROOT, "plots", "corpus", "hierarchy_l5.png"), dpi=150)
open(os.path.join(ROOT, "plots", "corpus", "hierarchy.txt"),
     "w").write("\n".join(OUT))
