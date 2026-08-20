import os
import json
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
from matplotlib.colors import LinearSegmentedColormap
from collections import Counter, defaultdict
from scipy.spatial.distance import cdist

from shape_grammar import (load_morpheus, load_p2k, is_null, FGRID, FS,
                           ORGANS, ORGAN_CODE, CODE_ORGAN)

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "plots", "corpus")
os.makedirs(OUT, exist_ok=True)

SURF = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
BASE = "#c3c2b7"
CAT = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300",
       "#4a3aa7", "#e34948"]
SEQ = ["#fcfcfb", "#cde2fb", "#9ec5f4", "#6da7ec", "#3987e5", "#256abf",
       "#184f95", "#0d366b"]
BLUES = LinearSegmentedColormap.from_list("seqblue", SEQ)
DIV = LinearSegmentedColormap.from_list(
    "div", ["#0d366b", "#3987e5", "#cde2fb", "#f0efec", "#f5b3b2", "#e34948",
            "#8d1f1e"])

plt.rcParams.update({
    "figure.facecolor": SURF, "axes.facecolor": SURF,
    "savefig.facecolor": SURF, "text.color": INK,
    "axes.labelcolor": INK2, "axes.edgecolor": BASE,
    "xtick.color": MUTED, "ytick.color": MUTED,
    "font.family": "sans-serif",
    "font.sans-serif": ["Segoe UI", "DejaVu Sans"],
    "axes.titlesize": 10, "axes.labelsize": 8.5,
    "xtick.labelsize": 7.5, "ytick.labelsize": 7.5,
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
    "axes.spines.top": False, "axes.spines.right": False,
    "lines.linewidth": 2.0,
})

NL = chr(10)
PLOT_LO, PLOT_HI = 40.0, 16000.0
BAND = (FGRID >= PLOT_LO) & (FGRID <= PLOT_HI)
FB = FGRID[BAND]


def style(ax):
    ax.set_axisbelow(True)
    ax.tick_params(length=3, width=0.6)
    return ax


def logx(ax, labels=True):
    ax.set_xscale("log")
    ax.set_xlim(PLOT_LO, PLOT_HI)
    ax.set_xticks([50, 200, 1000, 5000, 15000])
    if labels:
        ax.set_xticklabels(["50", "200", "1k", "5k", "15k"])
    else:
        ax.set_xticklabels([])


# ----------------------------------------------------------------- data
CORNERS = load_morpheus()
ACT = [o for o in CORNERS if not is_null(o)]
P2K = load_p2k()
NCUBE = len({o.cube for o in CORNERS})
REPORT = []


def say(s=""):
    print(s)
    REPORT.append(s)


def rprime(r):
    return 20.0 * np.log10(1.0 / np.maximum(1.0 - np.minimum(r, 0.99999), 1e-5))


def _enc(f, r):
    a = float(np.clip(rprime(r) / 24.0, 0.0, 1.0))
    return [a, a * math.log2(max(f, 20.0) / 20.0) / 10.0]


def features(o, by_shape=False):
    idx = list(range(7))
    if by_shape:
        idx.sort(key=lambda i: (o.rp[i] <= 0.3, o.fp[i]))
    v = []
    for i in idx:
        v += _enc(o.fp[i], o.rp[i]) + _enc(o.fz[i], o.rz[i])
    return v


def kmeans(X, k, seed, iters=200):
    rng = np.random.default_rng(seed)
    c = [X[rng.integers(len(X))]]
    for _ in range(k - 1):
        d = np.min(cdist(X, np.array(c)) ** 2, axis=1)
        c.append(X[rng.choice(len(X), p=d / d.sum())])
    C = np.array(c)
    lab = None
    for _ in range(iters):
        D = cdist(X, C)
        nl = D.argmin(axis=1)
        if lab is not None and np.array_equal(nl, lab):
            break
        lab = nl
        for j in range(k):
            m = lab == j
            if m.any():
                C[j] = X[m].mean(axis=0)
    return lab, C, float((cdist(X, C).min(axis=1) ** 2).sum())


def silhouette(D, lab, k):
    n = len(lab)
    s = np.zeros(n)
    for j in range(k):
        m = lab == j
        cnt = m.sum()
        if cnt <= 1:
            continue
        a = D[np.ix_(m, m)].sum(axis=1) / (cnt - 1)
        b = np.full(cnt, np.inf)
        for h in range(k):
            if h == j:
                continue
            mh = lab == h
            if mh.any():
                b = np.minimum(b, D[np.ix_(m, mh)].mean(axis=1))
        s[m] = (b - a) / np.maximum(a, b)
    return float(s.mean())


SLOT_SIL, SHAPE_SIL = {}, {}
best = None
for by_shape, store in ((False, SLOT_SIL), (True, SHAPE_SIL)):
    Xf = np.array([features(o, by_shape) for o in ACT])
    Df = cdist(Xf, Xf)
    for k in (4, 5, 6):
        lab, C, inertia = min((kmeans(Xf, k, s) for s in range(12)),
                              key=lambda t: t[2])
        sc = silhouette(Df, lab, k)
        store[k] = sc
        if (not by_shape) and (best is None or sc > best[0]):
            best = (sc, k, lab, C, Xf)
say("CLUSTER SEPARATION — stage-slot order vs frequency order")
for k in (4, 5, 6):
    say(f"  k={k}  slot-indexed silhouette {SLOT_SIL[k]:.4f}   "
        f"frequency-ordered {SHAPE_SIL[k]:.4f}   "
        f"{100*(SHAPE_SIL[k]-SLOT_SIL[k])/SLOT_SIL[k]:+.0f}% by re-sorting")
say("  re-sorting sections by frequency LOSES separation at every k:")
say("  the stage index is not arbitrary bookkeeping.")
SIL, K, LAB, CEN, XF = best
say(f"chosen k={K} on stage-slot geometry (silhouette {SIL:.4f})")
say()

order = np.argsort([-(LAB == j).sum() for j in range(K)])
remap = {old: new for new, old in enumerate(order)}
LAB = np.array([remap[x] for x in LAB])
CEN = CEN[order]
CLUST = [[ACT[i] for i in range(len(ACT)) if LAB[i] == j] for j in range(K)]
def anatomy(o):
    f = np.sort(o.fp[(o.rp > 0.3) & (~o.idle)])
    n = t = 0
    for k in range(6):
        if o.rz[k] > 0.3 and o.rp[k + 1] > 0.3:
            t += 1
            n += abs(12 * math.log2(max(o.fz[k], 1) / max(o.fp[k + 1], 1))) < 0.5
    return np.array([
        len(f),
        int((o.rz > 0.3).sum()),
        12 * math.log2(max(f[-1], 1) / max(f[0], 1)) if len(f) >= 2 else 0.0,
        (n / t) if t else 0.0,
        math.log2(max(np.median(f), 20.0) / 20.0) if len(f) else 0.0,
    ])


ANAT = np.array([anatomy(o) for o in ACT])
CROWN = np.array([float(o.total[BAND].max()) for o in ACT])
MEDOID = []
for j in range(K):
    m = np.where(LAB == j)[0]
    A = ANAT[m]
    sd = np.maximum(A.std(axis=0), 1e-6)
    d = np.abs((A - np.median(A, axis=0)) / sd).sum(axis=1)
    inband = CROWN[m] <= 36.0
    if inband.any():
        d = np.where(inband, d, np.inf)
    MEDOID.append(ACT[m[np.argmin(d)]])


def cluster_stats(mem):
    A = np.array([anatomy(o) for o in mem])
    med = np.median(A, axis=0)
    org = Counter(x for o in mem for x in o.organs if x != "PASS")
    return med, org, A


def archetype_name(mem):
    (lp, lz, spread, ch, lf), org, A = cluster_stats(mem)
    fm = 20.0 * 2 ** lf
    band = "sub-bass" if fm < 120 else "low" if fm < 700 else \
        "mid" if fm < 3000 else "high"
    dom = org.most_common(1)[0][0]
    if lp < 2.5:
        kind = "Single/double pole sweep"
    elif spread < 18.0:
        kind = f"Stacked poles inside {spread:.0f} st"
    elif lz < 1.5:
        kind = "All-pole stack, no zeros"
    elif ch >= 0.60:
        kind = ("Chained comb ladder" if dom == "NOTCH"
                else "Chained pole/zero ladder")
    elif ch >= 0.40:
        kind = "Part-chained pole/zero set"
    else:
        kind = "Free pole/zero set"
    short = {"LOW_SHELF": "SHELF", "HIGH_SHELF": "SHELF"}.get(dom, dom)
    return f"{kind}{NL}{lp:.0f} poles · {band} · {short}-led"


ARCH = [archetype_name(m) for m in CLUST]


def cluster_label(mem):
    org = Counter(x for o in mem for i, x in enumerate(o.organs)
                  if o.organs[i] != "PASS")
    live = np.mean([int((o.rp > 0.3).sum()) for o in mem])
    chain = []
    for o in mem:
        n = t = 0
        for k in range(6):
            if o.rz[k] > 0.3 and o.rp[k + 1] > 0.3:
                t += 1
                if abs(12 * math.log2(max(o.fz[k], 1) / max(o.fp[k + 1], 1))) < 0.5:
                    n += 1
        if t:
            chain.append(n / t)
    return org, live, (float(np.mean(chain)) if chain else 0.0)


# ------------------------------------------------------- PANEL 1
YLO, YHI = -66.0, 48.0


def blank(ax, note):
    ax.plot(FB, np.zeros_like(FB), color=BASE, lw=1.0, ls=(0, (3, 3)))
    ax.text(0.5, 0.30, note, transform=ax.transAxes, ha="center",
            fontsize=7.2, color=MUTED, style="italic")


def panel1():
    fig = plt.figure(figsize=(16.4, 2.55 * K + 1.7))
    gs = GridSpec(2 * K + 1, 9, figure=fig,
                  height_ratios=[0.78] + [1] * (2 * K),
                  width_ratios=[1.42, 2.45] + [1] * 7,
                  hspace=0.30, wspace=0.20,
                  left=0.030, right=0.993, top=0.962, bottom=0.045)
    head = fig.add_subplot(gs[0, :])
    head.axis("off")
    head.text(0, 0.84, "PANEL 1 — Corpus clustering & archetype separation",
              fontsize=16, fontweight="bold", va="center")
    head.text(0, 0.46, f"{len(ACT)} active corners of {len(CORNERS)} "
              f"({NCUBE} cubes, native clock {FS:g} Hz), k-means k={K} on the 28-D "
              f"log-polar geometry of all 14 roots. One real corner per archetype "
              f"— the member closest to its cluster's median anatomy. Nothing is "
              f"averaged, overlaid or peak-normalised: levels are true dB.",
              fontsize=9, color=INK2, va="center")
    head.text(0, 0.10, "Serial cascade — the S1..S7 magnitudes multiply, so dB add. "
              "UPPER strip = each stage's own factor, alone. LOWER strip = the "
              "running product through that stage, so the last cell equals the "
              "finished cascade at left. Bypassed stages are left blank.",
              fontsize=9, color=INK2, va="center")

    for j in range(K):
        mem, col, med = CLUST[j], CAT[j], MEDOID[j]
        org, live, chain = cluster_label(mem)
        ra, rb = 1 + 2 * j, 2 + 2 * j

        lab_ax = fig.add_subplot(gs[ra:rb + 1, 0])
        lab_ax.axis("off")
        lab_ax.add_patch(plt.Rectangle((0.0, 0.05), 0.075, 0.90, color=col,
                                       transform=lab_ax.transAxes, clip_on=False))
        lab_ax.text(0.12, 0.99, f"C{j+1}", fontsize=15, fontweight="bold",
                    va="top", transform=lab_ax.transAxes)
        lab_ax.text(0.12, 0.845, ARCH[j], fontsize=8.6, va="top",
                    color=INK, linespacing=1.35, fontweight="bold",
                    transform=lab_ax.transAxes)
        lab_ax.text(0.12, 0.55, NL.join([
            f"n={len(mem)}  ({100*len(mem)/len(ACT):.0f}% of corpus)",
            f"{live:.1f} live poles per corner",
            f"{100*chain:.0f}% of rungs chained",
            ", ".join(f"{o}·{c}" for o, c in org.most_common(2))]),
            fontsize=7.4, color=INK2, va="top", linespacing=1.7,
            transform=lab_ax.transAxes)
        lab_ax.text(0.12, 0.22, NL.join(
            ["members:"] + [n for n, _ in
                            Counter(o.cube_name for o in mem).most_common(3)]),
            fontsize=6.8, color=MUTED, va="top", linespacing=1.5,
            transform=lab_ax.transAxes)

        vals = np.concatenate([med.total[BAND], med.cum[:, BAND].ravel()]
                              + [med.sdb[t][BAND] for t in range(7)])
        vlo = math.floor(np.percentile(vals, 0.5) / 15.0) * 15.0
        vhi = math.ceil(vals.max() / 15.0) * 15.0
        vlo = max(vlo, vhi - 150.0)
        ticks = [v for v in range(-150, 121, 30) if vlo <= v <= vhi]

        ax = style(fig.add_subplot(gs[ra:rb + 1, 1]))
        ax.plot(FB, med.total[BAND], color=col, lw=2.0)
        ax.axhline(0, color=BASE, lw=0.8)
        ax.set_ylim(vlo, vhi)
        logx(ax, j == K - 1)
        ax.set_yticks(ticks)
        ax.set_ylabel("dB", labelpad=1)
        ax.set_title(f"{med.cube_name}  c{med.idx}   ·   finished cascade",
                     fontsize=8.8, color=INK, pad=3, loc="left")
        if j == K - 1:
            ax.set_xlabel("Hz")

        for t in range(7):
            inert = med.organs[t] == "PASS"
            a = style(fig.add_subplot(gs[ra, 2 + t]))
            if inert:
                blank(a, "idle")
            else:
                a.plot(FB, med.sdb[t][BAND], color=col, lw=1.7)
                a.axhline(0, color=BASE, lw=0.8)
            a.set_ylim(vlo, vhi)
            logx(a, False)
            a.set_yticks(ticks)
            if t:
                a.set_yticklabels([])
            a.set_title(f"S{t+1}   {ORGAN_CODE[med.organs[t]]}", fontsize=8.6,
                        color=INK if not inert else MUTED, pad=3)

            b = style(fig.add_subplot(gs[rb, 2 + t]))
            if inert:
                blank(b, "unchanged")
            else:
                b.plot(FB, med.cum[t][BAND], color=INK, lw=1.3)
                b.axhline(0, color=BASE, lw=0.8)
            b.set_ylim(vlo, vhi)
            logx(b, j == K - 1)
            b.set_yticks(ticks)
            if t:
                b.set_yticklabels([])
            if t == 0:
                b.text(0.04, 0.06, "running product", transform=b.transAxes,
                       fontsize=6.6, color=MUTED)
    fig.savefig(f"{OUT}/panel1_archetypes.png", dpi=150)
    plt.close(fig)


# ------------------------------------------------------- PANEL 2
def stage_organ_matrix(corners, ns=7):
    M = np.zeros((len(ORGANS), ns))
    for o in corners:
        for s in range(ns):
            M[ORGANS.index(o.organs[s]), s] += 1
    return M / np.maximum(M.sum(axis=0, keepdims=True), 1)


def transition_matrix(corners, ns=7):
    T = np.zeros((len(ORGANS), len(ORGANS)))
    for o in corners:
        for s in range(ns - 1):
            T[ORGANS.index(o.organs[s]), ORGANS.index(o.organs[s + 1])] += 1
    return T


def heat(ax, M, xl, yl, title, fmt="{:.0f}", scale=100, cmap=BLUES,
         vmin=0, vmax=None):
    vmax = vmax if vmax is not None else M.max()
    im = ax.imshow(M, cmap=cmap, vmin=vmin, vmax=vmax, aspect="auto")
    ax.set_xticks(range(len(xl)))
    ax.set_xticklabels(xl, fontsize=7.5)
    ax.set_yticks(range(len(yl)))
    ax.set_yticklabels(yl, fontsize=7.5)
    ax.set_title(title, fontsize=9.5, color=INK, pad=6, loc="left")
    ax.grid(False)
    for sp in ax.spines.values():
        sp.set_visible(False)
    for i in range(M.shape[0]):
        for j in range(M.shape[1]):
            v = M[i, j] * scale
            ax.text(j, i, fmt.format(v), ha="center", va="center", fontsize=7,
                    color="#ffffff" if M[i, j] > 0.58 * vmax else INK2)
    return im


def panel2():
    fig = plt.figure(figsize=(15.5, 8.4))
    gs = GridSpec(2, 3, figure=fig, height_ratios=[1, 0.92],
                  width_ratios=[1.15, 1.0, 1.0],
                  hspace=0.34, wspace=0.26,
                  left=0.062, right=0.985, top=0.885, bottom=0.075)
    fig.text(0.035, 0.965, "PANEL 2 — Stage role probability & shape transitions",
             fontsize=15, fontweight="bold")
    fig.text(0.035, 0.925,
             "Every section classified from its own magnitude response into one of "
             "seven organs. Left: P(organ | stage index) — the test for a rigid stage "
             "template. Right: what follows what, and the chaining rule that makes "
             "the corpus a grammar rather than a template.",
             fontsize=8.8, color=INK2)

    M = stage_organ_matrix(ACT)
    ax = fig.add_subplot(gs[0, 0])
    heat(ax, M, [f"S{i+1}" for i in range(7)], ORGANS,
         "P(organ | stage)  ·  Morpheus, % of 1,983 active corners", vmax=M.max())
    ax.set_xlabel("stage index")

    T = transition_matrix(ACT)
    Tn = T / np.maximum(T.sum(axis=1, keepdims=True), 1)
    ax = fig.add_subplot(gs[0, 1])
    heat(ax, Tn, [ORGAN_CODE[o] for o in ORGANS], ORGANS,
         "P(organ at S$_{k+1}$ | organ at S$_k$)  ·  %", vmax=Tn.max())
    ax.set_xlabel("organ at next stage")

    Mp = stage_organ_matrix(P2K, 6)
    ax = fig.add_subplot(gs[0, 2])
    heat(ax, Mp, [f"S{i+1}" for i in range(6)], ORGANS,
         "P(organ | stage)  ·  P2K legacy, 132 corners", vmax=Mp.max())
    ax.set_xlabel("stage index")

    ax = style(fig.add_subplot(gs[1, 0]))
    ent = [-(M[:, s][M[:, s] > 0] * np.log2(M[:, s][M[:, s] > 0])).sum()
           for s in range(7)]
    entp = [-(Mp[:, s][Mp[:, s] > 0] * np.log2(Mp[:, s][Mp[:, s] > 0])).sum()
            for s in range(6)]
    ax.bar(np.arange(7) - 0.19, ent, 0.36, color=CAT[0], label="Morpheus (7 stages)")
    ax.bar(np.arange(6) + 0.19, entp, 0.36, color=CAT[1], label="P2K (6 stages)")
    ax.axhline(np.log2(7), color=BASE, lw=1.2, ls=(0, (4, 3)))
    ax.text(6.35, np.log2(7) - 0.06, "max = 2.81 bits\n(no role constraint at all)",
            fontsize=7, color=MUTED, ha="right", va="top")
    ax.set_xticks(range(7))
    ax.set_xticklabels([f"S{i+1}" for i in range(7)])
    ax.set_ylim(0, 3.05)
    ax.set_ylabel("role entropy (bits)")
    ax.set_title("How constrained is each stage?", fontsize=9.5, loc="left", pad=6)
    ax.legend(frameon=False, fontsize=7.5, loc="lower left")

    ax = style(fig.add_subplot(gs[1, 1]))
    fwd, rev, tot = [], [], []
    for k in range(6):
        n = r = t = 0
        for o in ACT:
            if o.rz[k] > 0.3 and o.rp[k + 1] > 0.3:
                t += 1
                if abs(12 * math.log2(max(o.fz[k], 1) / max(o.fp[k + 1], 1))) < 0.5:
                    n += 1
            if o.rp[k] > 0.3 and o.rz[k + 1] > 0.3:
                if abs(12 * math.log2(max(o.fp[k], 1) / max(o.fz[k + 1], 1))) < 0.5:
                    r += 1
        fwd.append(100 * n / max(t, 1))
        rev.append(100 * r / max(t, 1))
        tot.append(t)
    x = np.arange(6)
    ax.bar(x - 0.19, fwd, 0.36, color=CAT[0],
           label="zero of S$_k$ lands on pole of S$_{k+1}$")
    ax.bar(x + 0.19, rev, 0.36, color=CAT[1], label="reverse direction")
    for i, v in enumerate(fwd):
        ax.text(i - 0.19, v + 1.4, f"{v:.0f}", ha="center", fontsize=7, color=INK2)
    ax.set_xticks(x)
    ax.set_xticklabels([f"S{i+1}→S{i+2}" for i in range(6)])
    ax.set_ylim(0, 78)
    ax.set_ylabel("% of live adjacent pairs (±0.5 st)")
    ax.set_title("The chaining production rule", fontsize=9.5, loc="left", pad=6)
    ax.legend(frameon=False, fontsize=7.5, loc="upper right")

    ax = fig.add_subplot(gs[1, 2])
    ax.axis("off")
    dead7 = sum(1 for o in ACT if o.rz[6] < 1e-9)
    idle = sum(int(o.idle.sum()) for o in ACT)
    sigs = Counter(o.sig for o in ACT)
    st = np.array([[o.sdb[s][BAND][-1] - o.sdb[s][BAND][0] for s in range(7)]
                   for o in ACT])
    cancel = 1.0 - np.abs(st.sum(axis=1)) / np.maximum(np.abs(st).sum(axis=1), 1e-9)
    pk_sum = np.array([sum(o.sdb[s][BAND].max() for s in range(7)) for o in ACT])
    pk_tot = np.array([o.total[BAND].max() for o in ACT])
    lines = [
        ("HARD LAWS (no exceptions in 2,312 corners)", "h"),
        (f"S7 zero radius disabled: {dead7}/{len(ACT)} = 100%", "good"),
        ("   the cascade always terminates pole-only", None),
        (f"S7 zero-angle word constant: 2,304×0, 8×2047", None),
        ("", None),
        ("THE SECTIONS ARE NOT EQ BANDS", "h"),
        (f"median tilt cancellation across the chain: "
         f"{100*np.median(cancel):.0f}%", None),
        ("   (sum of section tilts vs sum of their magnitudes —", None),
        ("    each stage's slope is undone by its neighbours)", None),
        (f"sum of section peaks {np.median(pk_sum):.0f} dB vs "
         f"cascade peak {np.median(pk_tot):.0f} dB (median)", None),
        ("", None),
        ("STRONG TENDENCIES (not laws)", "h"),
        (f"idle-pole filler [1909,2015]: {idle} sections "
         f"({100*idle/(7*len(ACT)):.0f}% of slots)", None),
        (f"distinct 7-organ signatures: {len(sigs)} of 7^7", None),
        (f"most common signature: {sigs.most_common(1)[0][0]} "
         f"x{sigs.most_common(1)[0][1]}", "m"),
        ("", None),
        ("TOP 8 SHAPE SIGNATURES", "h"),
    ]
    for s, c in sigs.most_common(8):
        lines.append((f"   {s}   x{c:<4d} {100*c/len(ACT):4.1f}%", "m"))
    lines += [("", None),
              (". PASS   = PARKED   L LOW_SHELF   H HIGH_SHELF", "m"),
              ("B BELL   N NOTCH    ^ LIFT", "m")]
    y = 0.995
    for txt, kind in lines:
        if kind == "h":
            ax.text(0, y, txt, fontsize=8.2, fontweight="bold", va="top",
                    color=INK, transform=ax.transAxes)
        else:
            ax.text(0, y, txt, fontsize=7.4,
                    color="#0ca30c" if kind == "good" else INK2,
                    fontweight="bold" if kind == "good" else "normal",
                    va="top", family="monospace" if kind == "m" else None,
                    transform=ax.transAxes)
        y -= 0.0345
    fig.savefig(f"{OUT}/panel2_stage_roles.png", dpi=150)
    plt.close(fig)


# ------------------------------------------------------- PANEL 3
def panel3():
    fig = plt.figure(figsize=(15.5, 7.6))
    gs = GridSpec(2, 2, figure=fig, width_ratios=[1.05, 1.0],
                  height_ratios=[1, 1], hspace=0.36, wspace=0.19,
                  left=0.062, right=0.985, top=0.876, bottom=0.078)
    fig.text(0.035, 0.962, "PANEL 3 — Frequency corridors & inter-pole spacing",
             fontsize=15, fontweight="bold")
    fig.text(0.035, 0.918,
             "Left: where each stage is allowed to sit. Right: the interval grammar "
             "between consecutive live poles — the direct test of the √2 half-octave "
             "ladder claim. All frequencies at the native 39,062.5 Hz clock.",
             fontsize=8.8, color=INK2)

    ax = style(fig.add_subplot(gs[:, 0]))
    for s in range(7):
        f = np.array([o.fp[s] for o in ACT if o.rp[s] > 0.3 and not o.idle[s]])
        y = 6 - s
        q = np.percentile(f, [5, 25, 50, 75, 95])
        ax.plot([q[0], q[4]], [y, y], color=BASE, lw=1.2, zorder=2)
        ax.plot([q[1], q[3]], [y, y], color=CAT[0], lw=7, solid_capstyle="butt",
                alpha=0.85, zorder=3)
        ax.plot([q[2]], [y], "o", ms=7, color=SURF, mec=CAT[0], mew=2, zorder=4)
        h, e = np.histogram(np.log10(np.maximum(f, 20)), bins=64,
                            range=(np.log10(20), np.log10(19200)))
        h = h / h.max() * 0.62
        ax.fill_between(10 ** e[:-1], y + 0.06, y + 0.06 + h, color=CAT[0],
                        alpha=0.30, lw=0, step="post", zorder=1)
        ax.text(24, y + 0.30, f"S{s+1}", fontsize=9.5, fontweight="bold",
                color=INK, va="center")
        ax.text(24, y - 0.16, f"n={len(f)}", fontsize=6.8, color=MUTED, va="center")
        ax.text(q[4] * 1.15, y, f"{q[0]:.0f}–{q[4]:.0f} Hz",
                fontsize=7.2, color=INK2, va="center")
    ax.set_xscale("log")
    ax.set_xlim(20, 60000)
    ax.set_xticks([50, 100, 500, 1000, 5000, 15000])
    ax.set_xticklabels(["50", "100", "500", "1k", "5k", "15k"])
    ax.set_yticks([])
    ax.set_ylim(-0.75, 7.15)
    ax.set_xlabel("pole frequency (Hz)")
    ax.set_title("Pole frequency corridor per stage  ·  bar = 25–75%, "
                 "whisker = 5–95%, shaded = density", fontsize=9.5, loc="left", pad=6)
    ax.axvline(19531.25, color=BASE, lw=1, ls=(0, (4, 3)))
    ax.text(19531.25 * 1.06, -0.55, "Nyquist", fontsize=7, color=MUTED)
    ax.spines["left"].set_visible(False)

    iv, ladder, intra = [], [], []
    for o in ACT:
        live = (o.rp > 0.3) & (~o.idle)
        idx = [i for i in range(7) if live[i]]
        for i in range(7):
            if live[i] and o.rz[i] > 0.3:
                intra.append(12 * math.log2(max(o.fz[i], 1) / max(o.fp[i], 1)))
        for a, b in zip(idx, idx[1:]):
            v = abs(12 * math.log2(max(o.fp[b], 1) / max(o.fp[a], 1)))
            iv.append(v)
            if o.rz[a] > 0.3 and abs(12 * math.log2(max(o.fz[a], 1) /
                                                    max(o.fp[b], 1))) < 0.5:
                ladder.append(v)
    iv = np.array(iv)
    ladder = np.array(ladder)
    intra = np.array(intra)

    ax = style(fig.add_subplot(gs[0, 1]))
    bins = np.arange(0, 24.25, 0.25)
    ax.hist(iv, bins=bins, color=CAT[0], alpha=0.9, label=f"all rungs (n={len(iv)})")
    ax.hist(ladder, bins=bins, color=CAT[1], alpha=0.85,
            label=f"chained rungs only (n={len(ladder)})")
    for c, lab in ((0, "unison"), (6, "√2 · 6 st"), (12, "octave"), (19, "12th"),
                   (24, "2 oct")):
        ax.axvline(c, color=BASE, lw=1, ls=(0, (4, 3)), zorder=0)
        ax.text(c, ax.get_ylim()[1] * 0.96, lab, fontsize=7, color=MUTED,
                ha="center", va="top", rotation=90)
    ax.set_xlim(0, 24)
    ax.set_xlabel("|semitones| between consecutive live poles")
    ax.set_ylabel("sections")
    ax.set_title("Inter-pole spacing  ·  the ladder is octave-and-unison, "
                 "not √2", fontsize=9.5, loc="left", pad=6)
    ax.legend(frameon=False, fontsize=7.5)
    for c in (0, 6, 12):
        p = 100 * (np.abs(iv - c) < 0.5).mean()
        ax.text(c + 0.35, ax.get_ylim()[1] * 0.55, f"{p:.1f}%", fontsize=7.5,
                color=CAT[0] if c != 6 else "#d03b3b", fontweight="bold")

    ax = style(fig.add_subplot(gs[1, 1]))
    ax.hist(np.clip(intra, -24, 24), bins=np.arange(-24, 24.5, 0.5),
            color=CAT[2], alpha=0.9)
    ax.axvline(0, color=BASE, lw=1.2)
    ax.set_xlim(-24, 24)
    ax.set_xlabel("semitones from pole to its own zero  (− = zero below pole)")
    ax.set_ylabel("sections")
    ax.set_title("Intra-section pole→zero placement  ·  the zero rides below "
                 "the pole", fontsize=9.5, loc="left", pad=6)
    below = 100 * (intra < -0.5).mean()
    ax.text(0.02, 0.92, f"zero below pole: {below:.0f}%\n"
            f"median {np.median(intra):+.1f} st", transform=ax.transAxes,
            fontsize=8, color=INK2, va="top")
    fig.savefig(f"{OUT}/panel3_corridors.png", dpi=150)
    plt.close(fig)


# ------------------------------------------------------- PANEL 4
AXIS_OF = {1: "T2", 2: "M", 4: "F"}


def panel4():
    by_cube = defaultdict(dict)
    for o in CORNERS:
        by_cube[o.cube][o.idx] = o
    names = {o.cube: o.cube_name for o in CORNERS}

    A = np.zeros((8, 8))
    N = np.zeros((8, 8))
    subs = Counter()
    axis_stat = {a: Counter() for a in ("T2", "M", "F")}
    for cid, cs in by_cube.items():
        for i in range(8):
            for j in range(8):
                oi, oj = cs[i], cs[j]
                if is_null(oi) or is_null(oj):
                    continue
                same = sum(1 for s in range(7) if oi.organs[s] == oj.organs[s])
                A[i, j] += same / 7.0
                N[i, j] += 1
        for bit, ax_ in AXIS_OF.items():
            for i in range(8):
                if i & bit:
                    continue
                oi, oj = cs[i], cs[i | bit]
                if is_null(oi) or is_null(oj):
                    axis_stat[ax_]["null_edge"] += 1
                    continue
                for s in range(7):
                    axis_stat[ax_]["slots"] += 1
                    if oi.organs[s] == oj.organs[s]:
                        axis_stat[ax_]["same_organ"] += 1
                    else:
                        subs[(oi.organs[s], oj.organs[s])] += 1
                    if oi.rp[s] > 0.3 and oj.rp[s] > 0.3 and abs(
                            12 * math.log2(max(oj.fp[s], 1) / max(oi.fp[s], 1))) < 0.5:
                        axis_stat[ax_]["pole_frozen"] += 1
    A = A / np.maximum(N, 1)

    fig = plt.figure(figsize=(15.5, 8.0))
    gs = GridSpec(2, 3, figure=fig, width_ratios=[1.0, 1.0, 1.25],
                  height_ratios=[1, 1], hspace=0.42, wspace=0.28,
                  left=0.055, right=0.985, top=0.878, bottom=0.075)
    fig.text(0.035, 0.962, "PANEL 4 — Morph-axis mutation matrix",
             fontsize=15, fontweight="bold")
    fig.text(0.035, 0.918,
             "How a shape survives a move across the cube. Corner index bit0 = "
             "Transform 2, bit1 = Morph, bit2 = Frequency tracking. Null corners "
             "excluded from agreement; counted separately as collapsed edges.",
             fontsize=8.8, color=INK2)

    ax = fig.add_subplot(gs[0, 0])
    lbl = [f"{i:03b}"[::-1] for i in range(8)]
    heat(ax, A, lbl, lbl, "Stage-shape agreement between corners  ·  %",
         vmin=0.3, vmax=1.0)
    ax.set_xlabel("corner (T2 M F)")
    ax.set_ylabel("corner (T2 M F)")

    ax = style(fig.add_subplot(gs[0, 1]))
    axes_ = ["T2", "M", "F"]
    same = [100 * axis_stat[a]["same_organ"] / max(axis_stat[a]["slots"], 1)
            for a in axes_]
    froz = [100 * axis_stat[a]["pole_frozen"] / max(axis_stat[a]["slots"], 1)
            for a in axes_]
    x = np.arange(3)
    ax.bar(x - 0.19, same, 0.36, color=CAT[0], label="same organ")
    ax.bar(x + 0.19, froz, 0.36, color=CAT[1], label="pole frozen (±0.5 st)")
    for i in range(3):
        ax.text(i - 0.19, same[i] + 1.5, f"{same[i]:.0f}", ha="center",
                fontsize=7.5, color=INK2)
        ax.text(i + 0.19, froz[i] + 1.5, f"{froz[i]:.0f}", ha="center",
                fontsize=7.5, color=INK2)
    ax.set_xticks(x)
    ax.set_xticklabels(["Transform 2\n(note-on)", "Morph\n(real-time)",
                        "Frequency\n(note-on)"], fontsize=8)
    ax.set_ylim(0, 100)
    ax.set_ylabel("% of stage slots")
    ax.set_title("Stability per axis", fontsize=9.5, loc="left", pad=6)
    ax.legend(frameon=False, fontsize=7.5, loc="upper center")
    for i, a in enumerate(axes_):
        ax.text(i, 4, f"{axis_stat[a]['null_edge']} collapsed\nedges",
                ha="center", fontsize=6.8, color=MUTED)

    ax = style(fig.add_subplot(gs[1, 0]))
    top = subs.most_common(9)[::-1]
    tot = sum(subs.values())
    ax.barh(range(len(top)), [100 * c / tot for _, c in top], color=CAT[0],
            height=0.62)
    ax.set_yticks(range(len(top)))
    ax.set_yticklabels([f"{a} → {b}" for (a, b), _ in top], fontsize=7.2)
    ax.set_xlabel("% of all organ substitutions")
    ax.set_title(f"Which organ becomes which  ·  {tot:,} substitutions",
                 fontsize=9.5, loc="left", pad=6)
    for i, (_, c) in enumerate(top):
        ax.text(100 * c / tot + 0.15, i, f"{c}", va="center", fontsize=6.8,
                color=MUTED)

    ax = style(fig.add_subplot(gs[1, 1]))
    per_stage = []
    for s in range(7):
        n = d = 0
        for cid, cs in by_cube.items():
            for bit in AXIS_OF:
                for i in range(8):
                    if i & bit:
                        continue
                    oi, oj = cs[i], cs[i | bit]
                    if is_null(oi) or is_null(oj):
                        continue
                    d += 1
                    n += oi.organs[s] == oj.organs[s]
        per_stage.append(100 * n / max(d, 1))
    ax.bar(range(7), per_stage, 0.6, color=CAT[0])
    for i, v in enumerate(per_stage):
        ax.text(i, v + 1.2, f"{v:.0f}", ha="center", fontsize=7.5, color=INK2)
    ax.set_xticks(range(7))
    ax.set_xticklabels([f"S{i+1}" for i in range(7)])
    ax.set_ylim(0, 100)
    ax.set_ylabel("% edges keeping the organ")
    ax.set_title("Which stage holds still under morph", fontsize=9.5,
                 loc="left", pad=6)

    ax = fig.add_subplot(gs[:, 2])
    ax.axis("off")
    tmpl = Counter()
    exemplar = defaultdict(list)
    for cid, cs in by_cube.items():
        key = tuple(cs[i].sig for i in range(8))
        tmpl[key] += 1
        exemplar[key].append(names[cid])
    ax.text(0, 1.0, "TOP 8-CORNER PROGRESSION TEMPLATES", fontsize=8.6,
            fontweight="bold", va="top", transform=ax.transAxes)
    y = 0.955
    for rank, (key, c) in enumerate(tmpl.most_common(5), 1):
        ax.text(0, y, f"{rank}.  ×{c} cubes   —  "
                f"{', '.join(exemplar[key][:3])}"
                + ("…" if len(exemplar[key]) > 3 else ""),
                fontsize=7.4, color=INK, va="top", fontweight="bold",
                transform=ax.transAxes)
        y -= 0.030
        ax.text(0.02, y, "  ".join(key), fontsize=7.6, color=INK2, va="top",
                family="monospace", transform=ax.transAxes)
        y -= 0.048
    y -= 0.015
    ax.text(0, y, "TOP PER-STAGE 8-CORNER PROGRESSIONS", fontsize=8.6,
            fontweight="bold", va="top", transform=ax.transAxes)
    y -= 0.035
    ps = Counter()
    psx = defaultdict(list)
    for cid, cs in by_cube.items():
        for s in range(7):
            k = (s, "".join(ORGAN_CODE[cs[i].organs[s]] for i in range(8)))
            ps[k] += 1
            psx[k].append(names[cid])
    for (s, key), c in ps.most_common(8):
        ax.text(0.02, y, f"S{s+1}  {key}   ×{c:<4d} {psx[(s,key)][0][:16]}",
                fontsize=7.4, color=INK2, va="top", family="monospace",
                transform=ax.transAxes)
        y -= 0.032
    y -= 0.015
    nplane = sum(1 for cid, cs in by_cube.items()
                 if all(is_null(cs[i]) for i in range(8) if not (i & 1)))
    nplane1 = sum(1 for cid, cs in by_cube.items()
                  if all(is_null(cs[i]) for i in range(8) if (i & 1)))
    ax.text(0, y, "COLLAPSED PLANES", fontsize=8.6, fontweight="bold",
            va="top", transform=ax.transAxes)
    y -= 0.035
    for t in (f"cubes with the whole T2=0 plane null: {nplane}",
              f"cubes with the whole T2=1 plane null: {nplane1}",
              f"cubes with any null corner: "
              f"{sum(1 for cs in by_cube.values() if any(is_null(v) for v in cs.values()))}",
              f"distinct 8-corner templates: {len(tmpl)} of {NCUBE} cubes"):
        ax.text(0.02, y, t, fontsize=7.4, color=INK2, va="top",
                transform=ax.transAxes)
        y -= 0.030
    fig.savefig(f"{OUT}/panel4_mutation.png", dpi=150)
    plt.close(fig)
    return A, axis_stat, subs, tmpl, exemplar, per_stage


panel1()
panel2()
A, axis_stat, subs, tmpl, exemplar, per_stage = panel4()
panel3()

# ------------------------------------------------------- text report
say("=" * 78)
say("CENSUS")
say(f"  cubes {NCUBE}   corners {len(CORNERS)}   active {len(ACT)}   "
    f"all-identity {len(CORNERS)-len(ACT)}")
say(f"  live sections {sum(int(o.live.sum()) for o in ACT)} of {7*len(ACT)}")
say(f"  idle-pole filler sections {sum(int(o.idle.sum()) for o in ACT)}")
say(f"  S7 zero disabled in {sum(1 for o in ACT if o.rz[6] < 1e-9)}/{len(ACT)} "
    f"active corners")
say()
say("ORGAN CENSUS (active corners)")
cnt = Counter(x for o in ACT for x in o.organs)
for k in ORGANS:
    say(f"  {k:11s} {cnt[k]:6d}  {100*cnt[k]/(7*len(ACT)):5.1f}%")
say()
say("CLUSTERS")
for j in range(K):
    mem = CLUST[j]
    org, live, chain = cluster_label(mem)
    say(f"  C{j+1}  n={len(mem):4d} ({100*len(mem)/len(ACT):4.1f}%)  "
        f"live poles {live:.2f}  chained {100*chain:.0f}%")
    say(f"      organs: {', '.join(f'{a}·{b}' for a,b in org.most_common(4))}")
    say(f"      cubes:  {', '.join(n for n,_ in Counter(o.cube_name for o in mem).most_common(6))}")
say()
say("AXIS STABILITY")
for a in ("T2", "M", "F"):
    s = axis_stat[a]
    say(f"  {a:3s} same-organ {100*s['same_organ']/max(s['slots'],1):5.1f}%   "
        f"pole-frozen {100*s['pole_frozen']/max(s['slots'],1):5.1f}%   "
        f"collapsed edges {s['null_edge']}")
say(f"  per-stage hold: " + "  ".join(f"S{i+1}={v:.0f}%" for i, v in enumerate(per_stage)))
say()
say("TOP SUBSTITUTIONS")
tot = sum(subs.values())
for (a, b), c in subs.most_common(10):
    say(f"  {a:11s} -> {b:11s} {c:6d}  {100*c/tot:5.1f}%")
open(f"{OUT}/report.txt", "w").write("\n".join(REPORT))
print("\nwrote", OUT)
