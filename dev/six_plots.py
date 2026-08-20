import os
import json
import glob
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
from matplotlib.colors import ListedColormap, BoundaryNorm
from mpl_toolkits.mplot3d import Axes3D  # noqa
from collections import Counter, defaultdict
from scipy.optimize import linear_sum_assignment
import networkx as nx

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plots", "corpus")
os.makedirs(OUT, exist_ok=True)
FS = 39062.5
NYQ = FS / 2.0
IDLE = (1909, 2015)
LOG = []


def say(s=""):
    print(s)
    LOG.append(s)


SURF, INK, INK2, MUTED = "#fcfcfb", "#0b0b0b", "#52514e", "#898781"
GRID, BASE = "#e1e0d9", "#c3c2b7"
CAT = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300",
       "#4a3aa7", "#e34948"]
plt.rcParams.update({
    "figure.facecolor": SURF, "axes.facecolor": SURF, "savefig.facecolor": SURF,
    "text.color": INK, "axes.labelcolor": INK2, "axes.edgecolor": BASE,
    "xtick.color": MUTED, "ytick.color": MUTED,
    "font.family": "sans-serif", "font.sans-serif": ["Segoe UI", "DejaVu Sans"],
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
    "axes.titlesize": 10, "axes.labelsize": 8.5,
    "xtick.labelsize": 7.5, "ytick.labelsize": 7.5,
    "axes.spines.top": False, "axes.spines.right": False,
})


def dewarp(f):
    if f <= 0:
        return 0.0
    return (FS / math.pi) * math.tan(math.pi * min(f, NYQ * 0.999) / FS)


def cents(a, b):
    return 1200.0 * math.log2(max(b, 1e-9) / max(a, 1e-9))


# ------------------------------------------------------------------ corpora
def load():
    out = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            secs = []
            for si, s in enumerate(cor["sections"]):
                raw = tuple(s["raw"])
                secs.append(dict(stage=si, fp=s["pole"]["hz"], rp=s["pole"]["r"],
                                 fz=s["zero"]["hz"], rz=s["zero"]["r"],
                                 idle=raw[:2] == IDLE,
                                 sentinel=(raw[1] == 2047 and raw[3] == 2047)))
            out.append(dict(era="Morpheus 1993", corpus="morpheus",
                            name=c["name"], corner=ci,
                            t2=ci & 1, m=(ci >> 1) & 1, q=(ci >> 2) & 1,
                            sections=secs))
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            secs = []
            for si, sec in enumerate(p["sections"]):
                g = sec["corners"][cn]

                def rd(x):
                    if "pair" in x:
                        a, b = x["pair"]
                        return (0.3 if (a + b) >= 0 else NYQ,
                                min(math.sqrt(abs(a * b)), 0.99999))
                    return (x["hz"], min(x["r"], 0.99999))
                fp, rp = rd(g["pole"])
                fz, rz = rd(g["zero"])
                secs.append(dict(stage=si, fp=fp, rp=rp, fz=fz, rz=rz,
                                 idle=False, sentinel=(rp < 1e-6 and rz < 1e-6)))
            out.append(dict(era="P2K 1999", corpus="p2k", name=p["name"],
                            corner=ci, t2=0, m=ci & 1, q=(ci >> 1) & 1,
                            sections=secs))
    return out


C = load()
MOR = [c for c in C if c["corpus"] == "morpheus"]
P2K = [c for c in C if c["corpus"] == "p2k"]
say(f"corners: {len(MOR)} Morpheus (7 stages) + {len(P2K)} P2K (6 stages)")


def poles(cor, fmax=16000.0, thr=0.3):
    return sorted(s["fp"] for s in cor["sections"]
                  if s["rp"] > thr and not s["idle"] and s["fp"] < fmax)


# Bell 1961 uniform tube: (2n-1)c/4L, c=35000 cm/s, L=17.5 cm -> 500,1500,...
BELL = [500.0 * (2 * n - 1) for n in range(1, 5)]
# Klatt 1980 reference vowel formants (F1,F2,F3). NOT present in this repo;
# these are the standard published values and are labelled as unverified here.
KLATT = {"i": (310, 2020, 2960), "e": (530, 1680, 2500),
         "a": (700, 1220, 2600), "o": (570, 840, 2410),
         "u": (350, 1250, 2200)}


# ============================================================ PLOT 1
def plot1():
    pts, tags = [], []
    for c in C:
        p = [dewarp(f) for f in poles(c)]
        if len(p) >= 3:
            pts.append(p[:3])
            tags.append(c["corpus"])
    P = np.array(pts)
    tags = np.array(tags)
    dmin = []
    for row in P:
        d = min(math.sqrt(np.mean([cents(t[i], row[i]) ** 2 for i in range(3)]))
                for t in list(KLATT.values()) + [tuple(BELL[:3])])
        dmin.append(d)
    dmin = np.array(dmin)
    vocal = dmin <= 400

    fig = plt.figure(figsize=(15.6, 6.4))
    fig.text(0.03, 0.955, "1 — De-warped acoustic formant scatter",
             fontsize=15, fontweight="bold")
    fig.text(0.03, 0.917,
             f"All corner poles below 16 kHz, inverse-bilinear de-warped to "
             f"continuous Hz. n={len(P)} corners with >=3 poles. "
             f"Within 400 cents RMS of a reference vowel or the Bell tube: "
             f"{100*vocal.mean():.1f}%.", fontsize=9, color=INK2)
    ax = fig.add_subplot(1, 2, 1, projection="3d")
    ax.scatter(P[~vocal, 0], P[~vocal, 1], P[~vocal, 2], s=5, c=MUTED,
               alpha=0.30, lw=0, label=f"outliers >400c ({int((~vocal).sum())})")
    ax.scatter(P[vocal, 0], P[vocal, 1], P[vocal, 2], s=9, c=CAT[0],
               alpha=0.75, lw=0, label=f"within 400c ({int(vocal.sum())})")
    for k, v in KLATT.items():
        ax.scatter(*[[x] for x in v], s=90, marker="*", c=CAT[7], lw=0)
        ax.text(v[0], v[1], v[2], f" /{k}/", fontsize=8, color=CAT[7])
    ax.scatter([BELL[0]], [BELL[1]], [BELL[2]], s=110, marker="P", c=CAT[3], lw=0)
    ax.text(BELL[0], BELL[1], BELL[2], "  Bell tube", fontsize=8, color=CAT[3])
    ax.set_xlabel("F1 Hz"); ax.set_ylabel("F2 Hz"); ax.set_zlabel("F3 Hz")
    ax.set_xlim(0, 2000); ax.set_ylim(0, 5000); ax.set_zlim(0, 8000)
    ax.legend(fontsize=7.5, loc="upper left", frameon=False)
    ax.set_title("F1 / F2 / F3", fontsize=10, loc="left")

    ax2 = fig.add_subplot(1, 2, 2)
    for tag, col in (("morpheus", CAT[0]), ("p2k", CAT[1])):
        m = tags == tag
        ax2.scatter(P[m, 0], P[m, 1], s=8, c=col, alpha=0.45, lw=0,
                    label=f"{tag} ({int(m.sum())})")
    for k, v in KLATT.items():
        ax2.scatter([v[0]], [v[1]], s=130, marker="*", c=CAT[7], lw=0, zorder=5)
        ax2.annotate(f"/{k}/", (v[0], v[1]), textcoords="offset points",
                     xytext=(8, 4), fontsize=9, color=CAT[7], fontweight="bold")
    ax2.scatter([BELL[0]], [BELL[1]], s=150, marker="P", c=CAT[3], lw=0, zorder=5)
    ax2.annotate("Bell 500/1500", (BELL[0], BELL[1]),
                 textcoords="offset points", xytext=(8, 4), fontsize=9,
                 color=CAT[3], fontweight="bold")
    ax2.set_xscale("log"); ax2.set_yscale("log")
    ax2.set_xlim(60, 8000); ax2.set_ylim(100, 16000)
    ax2.set_xlabel("F1 (de-warped Hz)"); ax2.set_ylabel("F2 (de-warped Hz)")
    ax2.set_title("F1 / F2 projection", fontsize=10, loc="left")
    ax2.legend(fontsize=8, frameon=False)
    ax2.set_axisbelow(True)
    fig.subplots_adjust(left=0.03, right=0.98, top=0.87, bottom=0.09, wspace=0.16)
    fig.savefig(f"{OUT}/p1_formant_scatter.png", dpi=150)
    plt.close(fig)
    say(f"P1: {len(P)} poses; within 400c of a vowel/tube target "
        f"{100*vocal.mean():.1f}%  (morpheus "
        f"{100*vocal[tags=='morpheus'].mean():.1f}%, p2k "
        f"{100*vocal[tags=='p2k'].mean():.1f}%)")
    return dmin, tags


# ============================================================ PLOT 2
ROLES = ["sentinel", "air cap / pad", "pinna notch", "formant peak",
         "shelf / termination", "other"]
RCOL = ["#e8e8e4", "#9aa0a6", "#4a3aa7", "#2a78d6", "#eb6834", "#eda100"]


def role_of(s):
    if s["sentinel"] or (s["rp"] < 0.05 and s["rz"] < 0.05):
        return 0
    if s["idle"] or (s["rp"] > 0.05 and s["fp"] >= 12000):
        return 1
    if s["rz"] >= 0.95 and 6000 <= s["fz"] <= 9000:
        return 2
    if s["rp"] >= 0.90 and 150 <= s["fp"] <= 4500:
        return 3
    if s["fp"] < 60 or s["fz"] < 60 or s["rp"] >= 0.95:
        return 4
    return 5


def plot2():
    fig = plt.figure(figsize=(15.6, 6.8))
    gs = GridSpec(2, 1, figure=fig, height_ratios=[1, 1], hspace=0.36,
                  left=0.055, right=0.90, top=0.855, bottom=0.08)
    fig.text(0.03, 0.955, "2 — Section-to-lane functional map (7 → 6 collapse)",
             fontsize=15, fontweight="bold")
    fig.text(0.03, 0.915,
             "Every corner of every preset, one column each, stage on Y, "
             "coloured by geometric role. Morpheus 7 stages above, P2K 6 below.",
             fontsize=9, color=INK2)
    cmap = ListedColormap(RCOL)
    for row, (tag, pool, ns) in enumerate((("Morpheus 1993", MOR, 7),
                                           ("P2K 1999", P2K, 6))):
        pool = sorted(pool, key=lambda c: (c["name"], c["corner"]))
        M = np.full((ns, len(pool)), 5)
        for j, c in enumerate(pool):
            for s in c["sections"]:
                M[s["stage"], j] = role_of(s)
        ax = fig.add_subplot(gs[row])
        ax.imshow(M, aspect="auto", cmap=cmap, vmin=-0.5, vmax=5.5,
                  interpolation="nearest")
        ax.set_yticks(range(ns))
        ax.set_yticklabels([f"S{i+1}" for i in range(ns)])
        ax.set_xticks([])
        ax.set_title(f"{tag}  ·  {len(pool)} corners", fontsize=10, loc="left")
        ax.grid(False)
        for sp in ax.spines.values():
            sp.set_visible(False)
        say(f"P2 {tag}: role mix by stage")
        for i in range(ns):
            cnt = Counter(M[i])
            top = ", ".join(f"{ROLES[k]} {100*v/len(pool):.0f}%"
                            for k, v in cnt.most_common(3))
            say(f"     S{i+1}: {top}")
    handles = [plt.Line2D([], [], marker="s", ls="", ms=10, color=RCOL[i],
                          label=ROLES[i]) for i in range(6)]
    fig.legend(handles=handles, loc="center right", frameon=False, fontsize=8.5,
               bbox_to_anchor=(1.0, 0.5))
    fig.savefig(f"{OUT}/p2_lane_roles.png", dpi=150)
    plt.close(fig)


# ============================================================ PLOT 3
def plot3():
    dst, rz, fz, corp = [], [], [], []
    for c in C:
        for s in c["sections"]:
            if s["idle"] or s["rp"] <= 0.3 or s["rz"] <= 0.02:
                continue
            wp, wz = dewarp(s["fp"]), dewarp(s["fz"])
            dst.append(12 * math.log2(max(wz, 1e-6) / max(wp, 1e-6)))
            rz.append(s["rz"])
            fz.append(wz)
            corp.append(c["corpus"])
    dst = np.array(dst); rz = np.array(rz); fz = np.array(fz)
    corp = np.array(corp)
    A = (rz >= 0.95) & (dst >= 6) & (dst <= 14)
    B = (rz >= 0.99) & (fz >= 6000) & (fz <= 9000)
    Cc = rz <= 0.15

    fig, ax = plt.subplots(figsize=(13.6, 6.6))
    fig.text(0.03, 0.955, "3 — Zero-to-pole offset phase space",
             fontsize=15, fontweight="bold")
    fig.text(0.03, 0.912,
             f"Every section with a live pole and a live zero (n={len(dst)}), "
             f"de-warped. X = 12 log2(fz/fp), Y = zero radius.",
             fontsize=9, color=INK2)
    ax.scatter(np.clip(dst, -40, 40), rz, s=5, c=MUTED, alpha=0.25, lw=0)
    for mask, col, lab in ((A, CAT[0], f"A carver zeros  rz>=0.95, +6..+14 st  "
                                       f"({int(A.sum())}, {100*A.mean():.1f}%)"),
                           (B, CAT[4], f"B 6-9 kHz unit-circle notches  "
                                       f"({int(B.sum())}, {100*B.mean():.1f}%)"),
                           (Cc, CAT[1], f"C dissolving zeros  rz<=0.15  "
                                        f"({int(Cc.sum())}, {100*Cc.mean():.1f}%)")):
        ax.scatter(np.clip(dst[mask], -40, 40), rz[mask], s=9, c=col,
                   alpha=0.65, lw=0, label=lab)
    for x in (-12, -6, 0, 6, 12):
        ax.axvline(x, color=BASE, lw=0.9, ls=(0, (4, 3)), zorder=0)
    ax.axhline(1.0, color=CAT[7], lw=1.0, ls=(0, (3, 3)))
    ax.text(-39, 1.005, "unit circle — a true null", fontsize=7.5, color=CAT[7])
    ax.set_xlim(-40, 40); ax.set_ylim(-0.02, 1.06)
    ax.set_xlabel("zero offset from its own pole (semitones)")
    ax.set_ylabel("zero radius  $r_z$")
    ax.legend(fontsize=8.5, frameon=False, loc="lower left")
    ax.set_axisbelow(True)
    fig.subplots_adjust(left=0.06, right=0.985, top=0.875, bottom=0.10)
    fig.savefig(f"{OUT}/p3_zero_phase_space.png", dpi=150)
    plt.close(fig)
    say(f"P3: n={len(dst)}  A carver {100*A.mean():.1f}%  "
        f"B 6-9kHz nulls {100*B.mean():.1f}%  C dissolving {100*Cc.mean():.1f}%  "
        f"median offset {np.median(dst):+.1f} st")


# ============================================================ PLOT 4
def plot4():
    def key(s):
        if s["rp"] < 0.05 and s["rz"] < 0.05:
            return None
        return (round(dewarp(s["fp"]) / 2.0), round(s["rp"], 3),
                round(dewarp(s["fz"]) / 2.0), round(s["rz"], 3))
    nodes, sets = [], []
    for c in C:
        ks = {key(s) for s in c["sections"] if key(s)}
        if len(ks) >= 4:
            nodes.append(c)
            sets.append(ks)
    inv = defaultdict(list)
    for i, ks in enumerate(sets):
        for k in ks:
            inv[k].append(i)
    cand = Counter()
    for k, lst in inv.items():
        if len(lst) < 2 or len(lst) > 60:
            continue
        for a in range(len(lst)):
            for b in range(a + 1, len(lst)):
                cand[(lst[a], lst[b])] += 1
    G = nx.Graph()
    for (a, b), n in cand.items():
        if n >= 4 and nodes[a]["name"] != nodes[b]["name"]:
            G.add_edge(a, b, weight=n)
    say(f"P4: {G.number_of_nodes()} corners linked by >=4 shared sections, "
        f"{G.number_of_edges()} edges")
    comps = sorted(nx.connected_components(G), key=len, reverse=True)
    say(f"    components: {len(comps)}, largest {len(comps[0]) if comps else 0}")
    fig, ax = plt.subplots(figsize=(13.6, 8.6))
    fig.text(0.03, 0.965, "4 — Pose reuse and scaffold mutation graph",
             fontsize=15, fontweight="bold")
    fig.text(0.03, 0.930,
             f"Nodes are corners; an edge means >=4 identical section states "
             f"between different presets. {G.number_of_nodes()} nodes, "
             f"{G.number_of_edges()} edges, {len(comps)} components.",
             fontsize=9, color=INK2)
    if G.number_of_nodes():
        H = G.subgraph(set().union(*comps[:14])) if comps else G
        pos = nx.spring_layout(H, seed=3, k=0.55, iterations=140)
        cols = [CAT[0] if nodes[n]["corpus"] == "morpheus" else CAT[1]
                for n in H.nodes]
        nx.draw_networkx_edges(H, pos, ax=ax, edge_color=BASE, width=0.7,
                               alpha=0.7)
        nx.draw_networkx_nodes(H, pos, ax=ax, node_size=42, node_color=cols,
                               linewidths=0)
        lab = {}
        for comp in comps[:14]:
            sub = [n for n in comp if n in H]
            if not sub:
                continue
            hub = max(sub, key=lambda n: H.degree(n))
            lab[hub] = nodes[hub]["name"]
        nx.draw_networkx_labels(H, pos, labels=lab, ax=ax, font_size=7.5,
                                font_color=INK)
    ax.axis("off")
    ax.scatter([], [], c=CAT[0], s=42, label="Morpheus corner")
    ax.scatter([], [], c=CAT[1], s=42, label="P2K corner")
    ax.legend(fontsize=8.5, frameon=False, loc="lower right")
    fig.subplots_adjust(left=0.02, right=0.98, top=0.90, bottom=0.02)
    fig.savefig(f"{OUT}/p4_reuse_graph.png", dpi=150)
    plt.close(fig)
    for comp in comps[:5]:
        nm = sorted({nodes[n]["name"] for n in comp})
        say(f"    cluster of {len(comp)} corners / {len(nm)} presets: "
            f"{', '.join(nm[:6])}{'...' if len(nm) > 6 else ''}")


# ============================================================ PLOT 5
def plot5():
    groups = defaultdict(dict)
    for c in C:
        groups[(c["corpus"], c["name"], c["q"], c["t2"])][c["m"]] = c
    xs, ys, names, cols = [], [], [], []
    for (corp, nm, q, t2), mm in groups.items():
        if 0 not in mm or 1 not in mm:
            continue
        a = poles(mm[0], fmax=19000.0)
        b = poles(mm[1], fmax=19000.0)
        if len(a) < 3 or len(a) != len(b):
            continue
        la = np.array([math.log2(dewarp(f)) for f in a])
        lb = np.array([math.log2(dewarp(f)) for f in b])
        lane = float(np.abs(la - lb).sum())
        Cm = np.abs(la[:, None] - lb[None, :])
        r, cidx = linear_sum_assignment(Cm)
        opt = float(Cm[r, cidx].sum())
        ratio = lane / opt if opt > 1e-9 else 1.0
        invs = sum(1 for i in range(len(cidx)) for j in range(i + 1, len(cidx))
                   if cidx[i] > cidx[j])
        xs.append(min(ratio, 8.0)); ys.append(invs); names.append(nm)
        cols.append(CAT[0] if corp == "morpheus" else CAT[1])
    xs = np.array(xs); ys = np.array(ys)
    fig, ax = plt.subplots(figsize=(13.6, 6.8))
    fig.text(0.03, 0.955, "5 — Morph travel ratio vs lane inversions",
             fontsize=15, fontweight="bold")
    fig.text(0.03, 0.912,
             f"X = lane-preserving travel / Hungarian minimum travel "
             f"(1.00 = no lane could be swapped to shorten the move). "
             f"Y = inversions in the optimal assignment. n={len(xs)} M0->M100 "
             f"transitions.", fontsize=9, color=INK2)
    jit = (np.random.default_rng(0).random(len(ys)) - 0.5) * 0.35
    ax.scatter(xs, ys + jit, s=16, c=cols, alpha=0.55, lw=0)
    ax.axvline(1.0, color=BASE, lw=1.0, ls=(0, (4, 3)))
    ax.text(1.03, ax.get_ylim()[1] * 0.94, "pure lane-preserving morph",
            fontsize=8, color=MUTED)
    pure = (xs <= 1.001) & (ys == 0)
    say(f"P5: {len(xs)} transitions; ratio==1.00 and 0 inversions: "
        f"{int(pure.sum())} ({100*pure.mean():.0f}%); "
        f"ratio>3 and >=2 inversions: "
        f"{int(((xs>3)&(ys>=2)).sum())}")
    for i in np.argsort(-xs)[:6]:
        ax.annotate(names[i], (xs[i], ys[i] + jit[i]),
                    textcoords="offset points", xytext=(6, 3), fontsize=7.5,
                    color=INK2)
        say(f"    high travel: {names[i]:16s} ratio {xs[i]:.2f} inversions {ys[i]}")
    ax.set_xlabel("travel ratio  (1.00 = Hungarian minimum)")
    ax.set_ylabel("lane inversions")
    ax.set_xlim(0.9, 8.2)
    ax.set_axisbelow(True)
    ax.scatter([], [], c=CAT[0], s=20, label="Morpheus")
    ax.scatter([], [], c=CAT[1], s=20, label="P2K")
    ax.legend(fontsize=8.5, frameon=False, loc="upper right")
    fig.subplots_adjust(left=0.06, right=0.985, top=0.875, bottom=0.10)
    fig.savefig(f"{OUT}/p5_morph_travel.png", dpi=150)
    plt.close(fig)


# ============================================================ PLOT 6
def plot6():
    pairs = defaultdict(dict)
    for c in C:
        pairs[(c["corpus"], c["name"], c["m"], c["t2"])][c["q"]] = c
    dfs, drs, f0s, cols = [], [], [], []
    for k, qq in pairs.items():
        if 0 not in qq or 1 not in qq:
            continue
        for s0, s1 in zip(qq[0]["sections"], qq[1]["sections"]):
            if s0["rp"] <= 0.3 or s1["rp"] <= 0.3 or s0["idle"] or s1["idle"]:
                continue
            f0 = dewarp(s0["fp"])
            dfs.append(cents(f0, dewarp(s1["fp"])) / 100.0)
            drs.append(s1["rp"] - s0["rp"])
            f0s.append(f0)
            cols.append(CAT[0] if k[0] == "morpheus" else CAT[1])
    dfs = np.array(dfs); drs = np.array(drs); f0s = np.array(f0s)
    fig = plt.figure(figsize=(15.4, 6.4))
    gs = GridSpec(1, 2, figure=fig, width_ratios=[1.25, 1], wspace=0.20,
                  left=0.055, right=0.985, top=0.855, bottom=0.11)
    fig.text(0.03, 0.955, "6 — Q dilation:  Q0 → Q100 per section",
             fontsize=15, fontweight="bold")
    fig.text(0.03, 0.912,
             f"Each live section's move when the Q axis flips. "
             f"X = semitone shift of the pole, Y = change in pole radius. "
             f"n={len(dfs)} section transitions.", fontsize=9, color=INK2)
    ax = fig.add_subplot(gs[0])
    ax.scatter(np.clip(dfs, -24, 24), drs, s=6, c=cols, alpha=0.35, lw=0)
    ax.axhline(0, color=BASE, lw=1.0)
    ax.axvline(0, color=BASE, lw=1.0)
    ax.set_xlabel("pole shift (semitones)")
    ax.set_ylabel(r"$\Delta r_p$")
    ax.set_xlim(-24, 24)
    ax.set_axisbelow(True)
    lift = (np.abs(dfs) < 0.5) & (drs > 0.002)
    say(f"P6: {len(dfs)} transitions; pure radius lift (|shift|<0.5st, dr>0): "
        f"{int(lift.sum())} = {100*lift.mean():.0f}%; "
        f"median dr {np.median(drs):+.4f}; median shift {np.median(dfs):+.2f} st")
    ax.set_title(f"pure radius lift: {100*lift.mean():.0f}% of sections",
                 fontsize=10, loc="left")
    ax2 = fig.add_subplot(gs[1])
    ax2.hist(np.clip(drs, -0.2, 0.2), bins=80, color=CAT[0])
    ax2.set_xlabel(r"$\Delta r_p$ from Q0 to Q100")
    ax2.set_ylabel("sections")
    ax2.set_title("is there one shared lift constant?", fontsize=10, loc="left")
    ax2.set_axisbelow(True)
    top = Counter(np.round(drs, 3)).most_common(5)
    say("    most common dr values: " +
        ", ".join(f"{v:+.3f} x{n}" for v, n in top))
    fig.savefig(f"{OUT}/p6_q_dilation.png", dpi=150)
    plt.close(fig)


plot1()
plot2()
plot3()
plot4()
plot5()
plot6()
open(f"{OUT}/six_plots_report.txt", "w").write("\n".join(LOG))
print("\nwrote", OUT)
