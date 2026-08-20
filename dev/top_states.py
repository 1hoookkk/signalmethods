import os
import json
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plots", "corpus")
os.makedirs(OUT, exist_ok=True)

SR = 39062.5
HZ_LO, HZ_HI = 40.0, 16000.0
DB_LO, DB_HI, CROWN = -60.0, 40.0, 36.0
GRID = np.geomspace(HZ_LO, HZ_HI, 512)
W = 2.0 * np.pi * GRID / SR

WELL, GRAT, GRATM = "#101014", "#26262c", "#34343c"
SURF, INK, INK2, MUTED = "#fcfcfb", "#0b0b0b", "#52514e", "#898781"
LIVE, CEIL = "#48c8ff", "#e05a4f"

D = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
assert D["law"]["hz_datum"] == SR


def section_db(fp, rp, fz, rz):
    """Standard biquad direct form: b = [1, -2R_z cos w_z, R_z^2],
    a = [1, -2R_p cos w_p, R_p^2]."""
    e1, e2 = np.exp(-1j * W), np.exp(-2j * W)
    num = 1 - 2 * rz * math.cos(2 * math.pi * fz / SR) * e1 + rz ** 2 * e2
    den = 1 - 2 * rp * math.cos(2 * math.pi * fp / SR) * e1 + rp ** 2 * e2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


FAMILY = [
    ("vowel/formant", ("vow", "voce", "hedz", "be-ye", "ee-yi", "ii-yi", "uhr",
                       "yeah", "yah", "yoyo", "bouche", "orator", "shaper",
                       "vocal", "choral", "para")),
    ("flange/comb", ("flng", "flange", "comb", "phas", "notch", "ntch", "cut")),
    ("lowpass/sweep", ("lp", "low", "pole", "sweep", "swp", "past", "brickwa")),
    ("highpass/shelf", ("hp", "hi ", "high", "hgh", "rizer", "brite", "air")),
    ("bell/EQ", ("eq", "bell", "boost", "peak", "prmtrc", "metric", "band")),
    ("modal/metal", ("chime", "clang", "wire", "glass", "cym", "piano", "bell",
                     "string", "tam", "wine", "ring")),
]


def family_of(name):
    n = name.lower()
    for fam, keys in FAMILY:
        if any(k in n for k in keys):
            return fam
    return "other"


STATES = []
for c in D["cubes"]:
    tag = ".4" if c["name"].endswith(".4") else "full"
    fam = family_of(c["name"])
    for ci, cor in enumerate(c["corners"]):
        for si, s in enumerate(cor["sections"]):
            w = tuple(s["raw"])
            g = (s["pole"]["hz"], s["pole"]["r"], s["zero"]["hz"], s["zero"]["r"])
            STATES.append((w, g, c["name"], tag, fam, ci, si))


def inert(g):
    return g[1] <= 1e-9 and g[3] <= 1e-9


def selfcancel(w):
    return w[0] == w[2] and w[1] == w[3]


LIVE_STATES = [s for s in STATES if not inert(s[1]) and not selfcancel(s[0])]


def rprime(r):
    if r >= 1:
        return 60.0
    if r <= 0:
        return 0.0
    return min(60.0, 20 * math.log10(1 / (1 - r)))


def acoustic_key(g):
    fp, rp, fz, rz = g
    def qf(f, r):
        if r <= 0.05:
            return ("off",)
        return (round(12 * math.log2(max(f, 20.0) / 20.0) / 0.5),
                round(rprime(r) / 3.0))
    return qf(fp, rp) + qf(fz, rz)


def summarise(members):
    cubes = Counter(m[2] for m in members)
    typ = Counter(m[3] for m in members)
    fam = Counter(m[4] for m in members)
    corner = Counter(m[5] for m in members)
    stage = Counter(m[6] for m in members)
    return cubes, typ, fam, corner, stage


def frame(ax):
    ax.set_facecolor(WELL)
    ax.set_xscale("log")
    ax.set_xlim(HZ_LO, HZ_HI)
    ax.set_ylim(DB_LO, DB_HI)
    for db in (-30, 0, 30):
        ax.axhline(db, color=GRATM if db == 0 else GRAT, lw=0.7, zorder=0)
    for hz in (100, 1000, 10000):
        ax.axvline(hz, color=GRAT, lw=0.7, zorder=0)
    ax.axhline(CROWN, color=CEIL, lw=0.8, ls=(0, (3, 3)), zorder=1)
    ax.set_xticks([])
    ax.set_yticks([])
    for sp in ax.spines.values():
        sp.set_color(GRATM)
    ax.grid(False)


def small_multiples(items, title, sub, path, rows=5, cols=10):
    fig = plt.figure(figsize=(16.0, 1.62 * rows + 1.5))
    fig.patch.set_facecolor(SURF)
    fig.text(0.028, 0.974, title, fontsize=16, fontweight="bold")
    fig.text(0.028, 0.941, sub, fontsize=9.2, color=INK2)
    for i, (label, g, n, extra) in enumerate(items[:rows * cols]):
        band = 0.905 / rows
        ax = fig.add_axes([0.028 + (i % cols) * 0.0967,
                           0.925 - (i // cols + 1) * band,
                           0.089, band - 0.052])
        frame(ax)
        ax.plot(GRID, np.clip(section_db(*g), DB_LO, DB_HI), color=LIVE, lw=1.2)
        ax.set_title(f"{i+1}. x{n}", fontsize=7.6, color=INK, pad=2)
        ax.text(0.5, -0.13, extra, transform=ax.transAxes, ha="center",
                va="top", fontsize=6.3, color=MUTED, linespacing=1.3)
    fig.savefig(path, dpi=150, facecolor=SURF)
    plt.close(fig)


# ---------------------------------------------------------- exact states
exact = Counter(s[0] for s in LIVE_STATES)
by_exact = defaultdict(list)
for s in LIVE_STATES:
    by_exact[s[0]].append(s)
top_exact = exact.most_common(50)

items = []
for w, n in top_exact:
    mem = by_exact[w]
    g = mem[0][1]
    cubes, typ, fam, corner, stage = summarise(mem)
    items.append((w, g, n,
                  f"{len(cubes)} cubes\n{g[0]:.0f}Hz r{g[1]:.3f}"))
small_multiples(items,
                "Top 50 exact stage states by occurrence",
                f"Exact equality of the 4 packed words. {len(exact)} distinct "
                f"live states over {sum(exact.values())} live slots; these 50 "
                f"cover {100*sum(n for _, n in top_exact)/sum(exact.values()):.1f}%. "
                f"Each panel is that factor's own magnitude |H_k|, 40 Hz-16 kHz, "
                f"-60..+40 dB, crown at 36 dB, {SR:g} Hz.",
                os.path.join(OUT, "top50_exact_states.png"))

# ------------------------------------------------------ acoustic archetypes
by_ac = defaultdict(list)
for s in LIVE_STATES:
    by_ac[acoustic_key(s[1])].append(s)
top_ac = sorted(by_ac.items(), key=lambda t: -len(t[1]))[:50]

items = []
for key, mem in top_ac:
    fp = np.median([m[1][0] for m in mem])
    rp = np.median([m[1][1] for m in mem])
    fz = np.median([m[1][2] for m in mem])
    rz = np.median([m[1][3] for m in mem])
    med = min(mem, key=lambda m: abs(m[1][0] - fp) / max(fp, 1)
              + abs(m[1][1] - rp))
    cubes, typ, fam, corner, stage = summarise(mem)
    items.append((key, med[1], len(mem),
                  f"{len(cubes)} cubes\n{med[1][0]:.0f}Hz r{med[1][1]:.3f}"))
small_multiples(items,
                "Top 50 acoustic state archetypes by occurrence",
                f"States grouped by acoustic equivalence (pole/zero binned to "
                f"1/2 semitone and 3 dB of R' = 20 log10(1/(1-R))). "
                f"{len(by_ac)} distinct archetypes over {len(LIVE_STATES)} live "
                f"slots; these 50 cover "
                f"{100*sum(len(m) for _, m in top_ac)/len(LIVE_STATES):.1f}%. "
                f"Curve drawn from the archetype's medoid member.",
                os.path.join(OUT, "top50_acoustic_archetypes.png"))

# ---------------------------------------------------------------- table
rows = []
hdr = ("rank\tkind\tcount\tpct_live\tcubes\tfull\tdot4\ttop_family\tfam_pct\t"
       "corner_hist\tstage_hist\tpole_hz\tpole_r\tzero_hz\tzero_r\twords")
rows.append(hdr)
for rank, (w, n) in enumerate(top_exact, 1):
    mem = by_exact[w]
    cubes, typ, fam, corner, stage = summarise(mem)
    g = mem[0][1]
    f0, fc = fam.most_common(1)[0]
    rows.append("\t".join([
        str(rank), "exact", str(n), f"{100*n/len(LIVE_STATES):.2f}",
        str(len(cubes)), str(typ.get("full", 0)), str(typ.get(".4", 0)),
        f0, f"{100*fc/n:.0f}",
        "|".join(str(corner.get(i, 0)) for i in range(8)),
        "|".join(str(stage.get(i, 0)) for i in range(7)),
        f"{g[0]:.1f}", f"{g[1]:.4f}", f"{g[2]:.1f}", f"{g[3]:.4f}",
        str(list(w))]))
for rank, (key, mem) in enumerate(top_ac, 1):
    cubes, typ, fam, corner, stage = summarise(mem)
    g = min(mem, key=lambda m: m[1][0])[1]
    f0, fc = fam.most_common(1)[0]
    rows.append("\t".join([
        str(rank), "acoustic", str(len(mem)),
        f"{100*len(mem)/len(LIVE_STATES):.2f}",
        str(len(cubes)), str(typ.get("full", 0)), str(typ.get(".4", 0)),
        f0, f"{100*fc/len(mem):.0f}",
        "|".join(str(corner.get(i, 0)) for i in range(8)),
        "|".join(str(stage.get(i, 0)) for i in range(7)),
        f"{np.median([m[1][0] for m in mem]):.1f}",
        f"{np.median([m[1][1] for m in mem]):.4f}",
        f"{np.median([m[1][2] for m in mem]):.1f}",
        f"{np.median([m[1][3] for m in mem]):.4f}",
        str(key)]))
open(os.path.join(OUT, "top50_states.tsv"), "w").write("\n".join(rows))

print(f"live slots {len(LIVE_STATES)} of {len(STATES)}")
print(f"distinct exact live states {len(exact)}  "
      f"top50 covers {100*sum(n for _,n in top_exact)/len(LIVE_STATES):.1f}%")
print(f"distinct acoustic archetypes {len(by_ac)}  "
      f"top50 covers {100*sum(len(m) for _,m in top_ac)/len(LIVE_STATES):.1f}%")
print("\nrank  count  cubes  family            pole            zero")
for r, (key, mem) in enumerate(top_ac[:12], 1):
    cubes, typ, fam, corner, stage = summarise(mem)
    print(f"{r:4d}  {len(mem):5d}  {len(cubes):5d}  {fam.most_common(1)[0][0]:16s}  "
          f"{np.median([m[1][0] for m in mem]):7.0f}Hz r{np.median([m[1][1] for m in mem]):.3f}  "
          f"{np.median([m[1][2] for m in mem]):7.0f}Hz r{np.median([m[1][3] for m in mem]):.3f}")
