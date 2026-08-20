import os
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

from shape_grammar import load_morpheus, is_null, FGRID, FS, resp_db, ORGAN_CODE

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "plots", "corpus")
os.makedirs(OUT, exist_ok=True)

SURF = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
BASE = "#c3c2b7"
BLUE = "#2a78d6"
ORANGE = "#eb6834"

plt.rcParams.update({
    "figure.facecolor": SURF, "axes.facecolor": SURF, "savefig.facecolor": SURF,
    "text.color": INK, "axes.labelcolor": INK2, "axes.edgecolor": BASE,
    "xtick.color": MUTED, "ytick.color": MUTED,
    "font.family": "sans-serif", "font.sans-serif": ["Segoe UI", "DejaVu Sans"],
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.6,
    "axes.spines.top": False, "axes.spines.right": False,
})

LO, HI = 40.0, 16000.0
B = (FGRID >= LO) & (FGRID <= HI)
FB = FGRID[B]


def stage_db(o, i):
    """One section's magnitude from its native decoded biquad coefficients."""
    return resp_db(np.array([o.fp[i]]), np.array([o.rp[i]]),
                   np.array([o.fz[i]]), np.array([o.rz[i]]), FGRID)[0]


def corner_curves(o):
    s = np.array([stage_db(o, i) for i in range(7)])
    return s, s.sum(axis=0) + 20.0 * math.log10(max(o.gain, 1e-9))


def mini(ax, y, col, lim, fill=True):
    ax.plot(FB, y, color=col, lw=1.25, solid_capstyle="round")
    if fill:
        ax.fill_between(FB, 0.0, y, color=col, alpha=0.15, lw=0)
    ax.axhline(0, color=BASE, lw=0.7)
    ax.set_xscale("log")
    ax.set_xlim(LO, HI)
    ax.set_ylim(*lim)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.grid(False)
    for s in ax.spines.values():
        s.set_visible(True)
        s.set_color(GRID)
        s.set_linewidth(0.8)


def idle_mini(ax, lim):
    ax.plot(FB, np.zeros_like(FB), color=BASE, lw=1.0, ls=(0, (3, 3)))
    ax.set_xscale("log")
    ax.set_xlim(LO, HI)
    ax.set_ylim(*lim)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.grid(False)
    ax.text(0.5, 0.5, "idle", transform=ax.transAxes, ha="center", va="center",
            fontsize=7, color=MUTED, style="italic")
    for s in ax.spines.values():
        s.set_color(GRID)
        s.set_linewidth(0.8)


def frame(ax, col, lw=2.2):
    for s in ax.spines.values():
        s.set_visible(True)
        s.set_color(col)
        s.set_linewidth(lw)


CROWN_LO, CROWN_HI = -40.0, 36.0


def nice_lim(vals, pad=6.0):
    """Frame on the documented crown band; expand only if the data needs it."""
    lo = min(CROWN_LO, math.floor((np.min(vals) - pad) / 10.0) * 10.0)
    hi = max(CROWN_HI, math.ceil((np.max(vals) + pad) / 10.0) * 10.0)
    return max(lo, hi - 160.0), hi


def authored(cube):
    """Corner indices that carry an authored response. The collapsed plane of
    a .4 cube is null padding; never select from it."""
    return [i for i in range(8) if not is_null(cube[i])]


def pick_corner(cube):
    live = authored(cube)
    b = (FGRID >= LO) & (FGRID <= HI)
    scored = []
    for i in live:
        _, tot = corner_curves(cube[i])
        crown = float(tot[b].max())
        scored.append((crown > CROWN_HI, -int((cube[i].rp > 0.3).sum()), i))
    return sorted(scored)[0][2] if scored else 0


def workstation(cube, sel=None, path=None):
    """cube: list of 8 Corner objects for one cube. sel: selected corner index,
    or None to pick the authored corner automatically."""
    if sel is None or is_null(cube[sel]):
        sel = pick_corner(cube)
    curves = [corner_curves(o) for o in cube]
    totals = [c[1][B] for c in curves]
    live = [not is_null(o) for o in cube]
    tl = nice_lim(np.concatenate([t for t, k in zip(totals, live) if k]))
    o = cube[sel]
    sdb, tot = curves[sel]
    sl = nice_lim(np.concatenate([sdb[i][B] for i in range(7)]))

    fig = plt.figure(figsize=(13.6, 7.6))
    gs = GridSpec(2, 2, figure=fig, height_ratios=[1.62, 1.0],
                  width_ratios=[1.72, 1.0], hspace=0.30, wspace=0.13,
                  left=0.055, right=0.98, top=0.855, bottom=0.075)

    fig.text(0.055, 0.955, o.cube_name, fontsize=19, fontweight="bold")
    fig.text(0.055, 0.905,
             f"corner {sel}  ·  M={(sel>>1)&1}  Freq={(sel>>2)&1}  "
             f"Transform2={sel&1}   ·   {FS:g} Hz native clock   ·   "
             f"native decoded coefficients, 7 sections in series",
             fontsize=9, color=INK2)

    ax = fig.add_subplot(gs[0, 0])
    ax.plot(FB, tot[B], color=BLUE, lw=2.4, solid_capstyle="round")
    ax.fill_between(FB, 0.0, tot[B], color=BLUE, alpha=0.12, lw=0)
    ax.axhline(0, color=BASE, lw=0.9)
    ax.set_xscale("log")
    ax.set_xlim(LO, HI)
    ax.set_ylim(*tl)
    ax.set_xticks([50, 100, 200, 500, 1000, 2000, 5000, 10000, 15000])
    ax.set_xticklabels(["50", "100", "200", "500", "1k", "2k", "5k", "10k", "15k"])
    ax.set_xlabel("Hz")
    ax.set_ylabel("dB")
    ax.set_title("RESPONSE", fontsize=10, fontweight="bold", loc="left", pad=8)
    ax.set_axisbelow(True)

    cube_ax = fig.add_subplot(gs[0, 1])
    cube_ax.axis("off")
    cube_ax.set_xlim(0, 1)
    cube_ax.set_ylim(0, 1)
    cube_ax.set_title("CORNERS", fontsize=10, fontweight="bold", loc="left", pad=8)
    w, h = 0.25, 0.20
    step_x, step_y = 0.28, 0.23
    off_x, off_y = 0.44, 0.52
    pos = {}
    for i in range(8):
        t, m, f = i & 1, (i >> 1) & 1, (i >> 2) & 1
        pos[i] = (m * step_x + t * off_x, f * step_y + t * off_y)
    for i in range(8):
        x0, y0 = pos[i]
        for bit in (2, 4, 1):
            j = i | bit
            if j == i:
                continue
            x1, y1 = pos[j]
            cube_ax.plot([x0 + w / 2, x1 + w / 2], [y0 + h / 2, y1 + h / 2],
                         color=GRID, lw=1.0, zorder=0)
    bb = cube_ax.get_position()
    for i in range(8):
        x, y = pos[i]
        a = fig.add_axes([bb.x0 + x * bb.width, bb.y0 + y * bb.height,
                          w * bb.width, h * bb.height])
        if live[i]:
            mini(a, totals[i], BLUE if i == sel else INK2, tl)
        else:
            idle_mini(a, tl)
        if i == sel:
            frame(a, ORANGE)
        a.text(0.05, 0.84, f"{i}", transform=a.transAxes, fontsize=7.5,
               color=ORANGE if i == sel else MUTED, fontweight="bold")
    cube_ax.annotate("", xy=(0.55, -0.03), xytext=(0.0, -0.03),
                     arrowprops=dict(arrowstyle="->", color=MUTED, lw=0.9))
    cube_ax.text(0.20, -0.10, "Morph", fontsize=7.6, color=MUTED)
    cube_ax.annotate("", xy=(-0.035, 0.47), xytext=(-0.035, 0.0),
                     arrowprops=dict(arrowstyle="->", color=MUTED, lw=0.9))
    cube_ax.text(-0.075, 0.13, "Freq", fontsize=7.6, color=MUTED, rotation=90)
    cube_ax.annotate("", xy=(0.66, 0.72), xytext=(0.30, 0.30),
                     arrowprops=dict(arrowstyle="->", color=MUTED, lw=0.9))
    cube_ax.text(0.40, 0.55, "Transform 2", fontsize=7.6, color=MUTED,
                 rotation=38)

    strip = fig.add_subplot(gs[1, :])
    strip.axis("off")
    strip.set_title("STAGES  ·  each section's own contribution, in series order",
                    fontsize=10, fontweight="bold", loc="left", pad=8)
    bb = strip.get_position()
    for i in range(7):
        a = fig.add_axes([bb.x0 + i * (bb.width / 7) + 0.004, bb.y0 + 0.012,
                          bb.width / 7 - 0.012, bb.height - 0.10])
        inert = o.organs[i] == "PASS"
        if inert:
            idle_mini(a, sl)
        else:
            mini(a, sdb[i][B], INK2, sl)
        lab = f"S{i+1}"
        if not inert:
            lab += f"   {o.fp[i]:.0f} Hz"
            if o.rz[i] > 0.3:
                lab += f" / z {o.fz[i]:.0f}"
        a.set_title(lab, fontsize=8.2, color=INK if not inert else MUTED, pad=3)
    fig.savefig(path or f"{OUT}/workstation.png", dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    allc = load_morpheus()
    by = {}
    for c in allc:
        by.setdefault(c.cube_name, {})[c.idx] = c
    picks = ["Vocal Cube", "LPFlange.4", "AEParaVowel", "HiQ 4PoleLP",
             "Phaser", "Be-Ye.4"]
    for name in picks:
        if name not in by:
            continue
        cube = [by[name][i] for i in range(8)]
        sel = pick_corner(cube)
        print(f"  {name:14s} authored corners {authored(cube)} -> corner {sel}")
        slug = name.lower().replace(" ", "_").replace(".", "").replace(">", "_")
        workstation(cube, sel, f"{OUT}/ws_{slug}.png")
