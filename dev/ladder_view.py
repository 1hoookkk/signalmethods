import os
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

from shape_grammar import load_morpheus, is_null, FGRID, FS, resp_db

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "plots", "corpus")
os.makedirs(OUT, exist_ok=True)

SURF, INK, INK2, MUTED = "#fcfcfb", "#0b0b0b", "#52514e", "#898781"
GRID, BASE = "#e1e0d9", "#c3c2b7"
CRIT = "#d03b3b"
STAGE = ["#86b6ef", "#6da7ec", "#5598e7", "#3987e5", "#256abf", "#184f95",
         "#0d366b"]

plt.rcParams.update({
    "figure.facecolor": SURF, "axes.facecolor": SURF, "savefig.facecolor": SURF,
    "text.color": INK, "axes.labelcolor": INK2, "axes.edgecolor": BASE,
    "xtick.color": MUTED, "ytick.color": MUTED,
    "font.family": "sans-serif", "font.sans-serif": ["Segoe UI", "DejaVu Sans"],
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
    "axes.spines.top": False, "axes.spines.right": False,
})

HZ_LO, HZ_HI, RP_MAX = 40.0, 18500.0, 60.0
B = (FGRID >= HZ_LO) & (FGRID <= HZ_HI)
FB = FGRID[B]
CENTS, DRP = 10.0, 1.0


def rprime_raw(r):
    if r >= 1.0:
        return 400.0
    if r <= 0.0:
        return 0.0
    return 20.0 * math.log10(1.0 / (1.0 - r))


def rprime(r):
    return min(RP_MAX, rprime_raw(r))


def cents(a, b):
    return 1200.0 * math.log2(max(b, 1e-6) / max(a, 1e-6))


def cascade(fp, rp, fz, rz, gain=1.0):
    if len(fp) == 0:
        return np.zeros_like(FGRID)
    a = (np.array(fp), np.array(rp), np.array(fz), np.array(rz))
    return resp_db(*a, FGRID).sum(axis=0) + 20.0 * math.log10(max(gain, 1e-9))


def match(zf, zr, pf, pr):
    return (abs(cents(zf, pf)) < CENTS and
            abs(rprime_raw(zr) - rprime_raw(pr)) < DRP)


def residual(o):
    """A cascade is a product: any zero cancels any equal pole, whichever
    stage holds it. Returns surviving poles, surviving zeros, and the
    (zero_stage, pole_stage) pairs that annihilate."""
    P = [(i, o.fp[i], o.rp[i]) for i in range(7) if o.rp[i] > 0.05]
    Z = [(i, o.fz[i], o.rz[i]) for i in range(7) if o.rz[i] > 0.05]
    kp, kz, pairs = list(P), [], []
    for zi, zf, zr in Z:
        hit = None
        for k, (pi, pf, pr) in enumerate(kp):
            if match(zf, zr, pf, pr):
                hit = k
                break
        if hit is None:
            kz.append((zi, zf, zr))
        else:
            pairs.append((zi, kp.pop(hit)[0]))
    return kp, kz, pairs


def roots_axes(ax, o):
    ax.set_xscale("log")
    ax.set_xlim(HZ_LO, HZ_HI)
    ax.set_ylim(0, RP_MAX)
    ax.set_xticks([100, 1000, 10000])
    ax.set_xticklabels(["100", "1k", "10k"])
    ax.set_yticks([0, 20, 40, 60])
    ax.set_ylabel("R'  dB", labelpad=1)
    ax.set_axisbelow(True)

    _, _, pairs = residual(o)
    dead = {(zi, pi) for zi, pi in pairs}
    for zi, pi in pairs:
        x0, y0 = o.fz[zi], rprime(o.rz[zi])
        x1, y1 = o.fp[pi], rprime(o.rp[pi])
        if abs(cents(x0, x1)) > 3.0:
            ax.annotate("", xy=(x1, y1), xytext=(x0, y0),
                        arrowprops=dict(arrowstyle="-|>", lw=1.6, color=CRIT,
                                        alpha=0.95, shrinkA=5, shrinkB=5))
        ax.plot([x1], [y1], "o", ms=15, mfc="none", mec=CRIT, mew=1.8, zorder=4)
        ax.plot([x1], [y1], "x", ms=8, color=CRIT, mew=1.8, zorder=5)
    for k in range(6):
        if o.rz[k] > 0.05 and o.rp[k + 1] > 0.05 and (k, k + 1) not in dead:
            ax.annotate("", xy=(o.fp[k + 1], rprime(o.rp[k + 1])),
                        xytext=(o.fz[k], rprime(o.rz[k])),
                        arrowprops=dict(arrowstyle="-|>", lw=1.0, color=BASE,
                                        alpha=0.7, shrinkA=5, shrinkB=5))
    for i in range(7):
        col = STAGE[i]
        pr, zr = rprime(o.rp[i]), rprime(o.rz[i])
        if o.rp[i] > 0.05 and o.rz[i] > 0.05:
            ax.plot([o.fp[i], o.fz[i]], [pr, zr], color=col, lw=1.0, alpha=0.35,
                    zorder=1)
        if o.rp[i] > 0.05:
            ax.plot([o.fp[i]], [pr], "o", ms=7, color=col, zorder=3)
            ax.annotate(f"{i+1}", (o.fp[i], pr), textcoords="offset points",
                        xytext=(6, 3), fontsize=7, color=INK2)
        if o.rz[i] > 0.05:
            ax.plot([o.fz[i]], [zr], "o", ms=7, mfc=SURF, mec=col, mew=1.8,
                    zorder=3)
            ax.annotate(f"{i+1}", (o.fz[i], zr), textcoords="offset points",
                        xytext=(6, 3), fontsize=7, color=INK2)


def figure(picks, path):
    n = len(picks)
    fig = plt.figure(figsize=(14.6, 2.55 * n + 1.9))
    gs = GridSpec(n + 1, 2, figure=fig, height_ratios=[0.55] + [1] * n,
                  width_ratios=[1.0, 1.18], hspace=0.42, wspace=0.16,
                  left=0.055, right=0.985, top=0.955, bottom=0.055)
    head = fig.add_subplot(gs[0, :])
    head.axis("off")
    head.text(0, 0.86, "The ladder and its cancellation",
              fontsize=17, fontweight="bold", va="center")
    head.text(0, 0.46,
              "Left: the documented roots view — log frequency across, "
              "resonance R' = 20·log10(1/(1−R)) up. Filled = pole, open = "
              "zero, number = the stage holding it. Grey arrow = the corpus "
              "production rule, the zero of stage k placed on the pole of "
              "stage k+1.",
              fontsize=9.2, color=INK2, va="center")
    head.text(0, 0.10,
              "RED arrow = that pair annihilates (within 10 cents and 1 dB of "
              "R'): the cascade is a product, so the numerator cancels the "
              "denominator and neither root is audible. Right: all 7 stages "
              "against the same filter rebuilt from the survivors alone.",
              fontsize=9.2, color=INK2, va="center")

    for r, o in enumerate(picks):
        ax = fig.add_subplot(gs[r + 1, 0])
        roots_axes(ax, o)
        if r == n - 1:
            ax.set_xlabel("Hz")
        ax.set_title(f"{o.cube_name}   corner {o.idx}", fontsize=10,
                     fontweight="bold", loc="left", pad=5)

        kp, kz, pairs = residual(o)
        full = cascade(o.fp, o.rp, o.fz, o.rz, o.gain)
        m = max(len(kp), len(kz))
        fp = [x[1] for x in kp] + [0.0] * (m - len(kp))
        rp = [x[2] for x in kp] + [0.0] * (m - len(kp))
        fz = [x[1] for x in kz] + [0.0] * (m - len(kz))
        rz = [x[2] for x in kz] + [0.0] * (m - len(kz))
        res = cascade(fp, rp, fz, rz, o.gain)
        err = float(np.abs(res[B] - full[B]).max())

        bx = fig.add_subplot(gs[r + 1, 1])
        bx.plot(FB, full[B], color=STAGE[5], lw=2.4, label="all 7 stages")
        bx.plot(FB, res[B], color=CRIT, lw=1.3, ls=(0, (5, 3)),
                label=f"{len(kp)} surviving poles + {len(kz)} zeros")
        bx.axhline(0, color=BASE, lw=0.9)
        bx.set_xscale("log")
        bx.set_xlim(HZ_LO, HZ_HI)
        bx.set_xticks([100, 1000, 10000])
        bx.set_xticklabels(["100", "1k", "10k"])
        bx.set_ylabel("dB", labelpad=1)
        bx.set_axisbelow(True)
        bx.legend(frameon=False, fontsize=7.6, loc="lower left")
        adj = sum(1 for zi, pi in pairs if pi == zi + 1)
        bx.set_title(f"{len(pairs)} of {int((o.rp>0.05).sum())} poles "
                     f"annihilated ({adj} by the k→k+1 rule)   ·   "
                     f"rebuild error {err:.2f} dB",
                     fontsize=10, fontweight="bold", loc="left", pad=5)
        if r == n - 1:
            bx.set_xlabel("Hz")
    fig.savefig(path, dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    allc = load_morpheus()
    by = {}
    for c in allc:
        by[(c.cube_name, c.idx)] = c
    picks = [by[k] for k in [("Vocal Cube", 1), ("LPFlange.4", 1),
                             ("Phaser", 3), ("AEParaVowel", 0),
                             ("Be-Ye.4", 1)] if k in by]
    figure(picks, f"{OUT}/ladder.png")

    act = [o for o in allc if not is_null(o)]
    tot = kil = adj = 0
    errs = []
    for o in act:
        kp, kz, pairs = residual(o)
        tot += int((o.rp > 0.05).sum())
        kil += len(pairs)
        adj += sum(1 for zi, pi in pairs if pi == zi + 1)
        if pairs:
            full = cascade(o.fp, o.rp, o.fz, o.rz, o.gain)
            m = max(len(kp), len(kz))
            res = cascade([x[1] for x in kp] + [0.0] * (m - len(kp)),
                          [x[2] for x in kp] + [0.0] * (m - len(kp)),
                          [x[1] for x in kz] + [0.0] * (m - len(kz)),
                          [x[2] for x in kz] + [0.0] * (m - len(kz)), o.gain)
            errs.append(float(np.abs(res[B] - full[B]).max()))
    errs = np.array(errs)
    print(f"live poles {tot}  annihilated {kil} = {100*kil/tot:.1f}%  "
          f"of which k->k+1 adjacent: {adj} ({100*adj/max(kil,1):.0f}%)")
    print(f"corners with >=1 annihilation: {len(errs)} of {len(act)}")
    print(f"rebuild error dB: median {np.median(errs):.2f}  "
          f"p90 {np.percentile(errs,90):.2f}  max {errs.max():.2f}  "
          f"under 1 dB: {100*(errs<1).mean():.0f}%")
