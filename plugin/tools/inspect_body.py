from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from runtime_probe import (GRID, load_body, stage_curves, response,  # noqa: E402
                           corner_geometry, interpolate_words, roots_from_words,
                           is_cube, stages_of, frames_of)

FRAMES = ["M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100",
          "M0 Q0 T2", "M100 Q0 T2", "M0 Q100 T2", "M100 Q100 T2"]
FRAME_MQZ = [(0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (1.0, 1.0, 0.0),
             (0.0, 0.0, 1.0), (1.0, 0.0, 1.0), (0.0, 1.0, 1.0), (1.0, 1.0, 1.0)]
FRAME_COL = ["#7f1d1d", "#c96a54", "#14847a", "#2bd8c3",
             "#8c5a12", "#e8a13a", "#3b4a8c", "#7f8fd8"]

def peak_map(db: np.ndarray):
    out = []
    for i in range(2, len(db) - 2):
        if db[i] > db[i - 1] and db[i] > db[i + 1]:
            base = min(db[max(0, i - 60):i].min(), db[i:i + 60].min())
            if db[i] - base > 3.0:
                out.append((GRID[i], db[i] - base))
    return out

def main():
    body_path = Path(sys.argv[1]).resolve()
    rate = float(sys.argv[2]) if len(sys.argv) > 2 else 48_000.0
    body = load_body(body_path)

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    NUM = stages_of(body)
    CUBE = is_cube(body)
    cols = NUM * 6
    fig = plt.figure(figsize=(17 if not CUBE else 19, 15))
    gs = fig.add_gridspec(4, cols, height_ratios=[1.4, 0.62, 0.62, 1.5],
                          hspace=0.5, wspace=0.28)
    cmap = plt.get_cmap("YlOrRd_r")

    rides = [("riding MORPH at Q0  (dark = wheel down)",
              lambda t: response(body, t, 0.0, rate, 0.0)),
             ("riding FREQ. TRACKING at M50  (dark = wheel down)",
              lambda t: response(body, 0.5, t, rate, 0.0))]
    if CUBE:
        rides.append(("riding TRANSFORM 2 at M0 Q0  (dark = wheel down)",
                      lambda t: response(body, 0.0, 0.0, rate, t)))
    span = cols // len(rides)
    for i, (title, fn) in enumerate(rides):
        ax = fig.add_subplot(gs[0, i * span:(i + 1) * span if i < len(rides) - 1 else cols])
        for j, t in enumerate(np.linspace(0, 1, 11)):
            ax.semilogx(GRID, fn(float(t)), color=cmap(j / 10), lw=1.1)
        ax.set_title(title, fontsize=10)

    big_axes = list(fig.axes)

    m0 = stage_curves(body, 0.0, 0.0, rate)
    m1 = stage_curves(body, 1.0, 0.0, rate)
    for s in range(NUM):
        ax = fig.add_subplot(gs[1, s * 6:(s + 1) * 6])
        offset = float(np.median(np.concatenate([m0[s], m1[s]])))
        ax.semilogx(GRID, m0[s] - offset, color="#7f1d1d", lw=1.3)
        ax.semilogx(GRID, m1[s] - offset, color="#e8a13a", lw=1.3)
        ax.set_xlim(20, 20_000); ax.set_ylim(-45, 45)
        ax.set_xticks([]); ax.set_yticks([])
        ax.axhline(0.0, color="0.85", lw=0.6, zorder=0)
        for sp in ax.spines.values():
            sp.set_color("0.55")
        travel = float(np.max(np.abs(m1[s] - m0[s])))
        ax.set_title(f"S{s+1}", fontsize=10, weight="bold")
        ax.text(0.5, -0.16, f"travel {travel:.0f} dB \xb7 level {offset:+.0f} dB",
                transform=ax.transAxes, ha="center", fontsize=8)
        if s == 0:
            ax.text(-0.16, 0.5, "IN", transform=ax.transAxes, ha="right",
                    va="center", fontsize=10, weight="bold")
        if s == NUM - 1:
            ax.text(1.16, 0.5, "OUT", transform=ax.transAxes, ha="left",
                    va="center", fontsize=10, weight="bold")
        if s < NUM - 1:
            ax.annotate("", xy=(1.14, 0.5), xytext=(1.0, 0.5),
                        xycoords="axes fraction",
                        arrowprops=dict(arrowstyle="->", color="0.4"))

    for k in range(NUM):
        ax = fig.add_subplot(gs[2, k * 6:(k + 1) * 6])
        cum0 = np.sum(m0[:k + 1], axis=0)
        cum1 = np.sum(m1[:k + 1], axis=0)
        offset = float(np.median(np.concatenate([cum0, cum1])))
        ax.semilogx(GRID, cum0 - offset, color="#7f1d1d", lw=1.3)
        ax.semilogx(GRID, cum1 - offset, color="#e8a13a", lw=1.3)
        ax.set_xlim(20, 20_000); ax.set_ylim(-45, 45)
        ax.set_xticks([]); ax.set_yticks([])
        ax.axhline(0.0, color="0.85", lw=0.6, zorder=0)
        for sp in ax.spines.values():
            sp.set_color("0.55")
        ax.set_title("S1" if k == 0 else f"S1..S{k+1}", fontsize=9)
        if k == NUM - 1:
            ax.text(1.16, 0.5, "= the\nwhole", transform=ax.transAxes,
                    ha="left", va="center", fontsize=8, weight="bold")

    ax = fig.add_subplot(gs[3, :cols // 2])
    nf = frames_of(body)
    for i in range(nf):
        m, q, z = FRAME_MQZ[i]
        ax.semilogx(GRID, response(body, m, q, rate, z), color=FRAME_COL[i],
                    lw=1.3, label=f"frame {i+1}  {FRAMES[i]}")
    ax.legend(fontsize=7, loc="lower left", ncol=2 if nf > 4 else 1)
    ax.set_title(f"the {'eight' if nf > 4 else 'four'} frames (whole cascade)", fontsize=10)
    big_axes.append(ax)

    for ax in big_axes:
        ax.set_xlim(20, 20_000); ax.set_ylim(-60, 30)
        ax.grid(alpha=0.3, which="both")
        ax.tick_params(labelsize=7)

    ax = fig.add_subplot(gs[3, cols // 2:])
    msteps = np.linspace(0.0, 1.0, 9)
    zrides = ((0.0, "-"), (1.0, ":")) if CUBE else ((0.0, "-"),)
    for si in range(NUM):
        y = NUM - si
        for zv, style in zrides:
            for qv, alpha in ((0.0, 0.95), (1.0, 0.35)):
                ph, zh = [], []
                for m in msteps:
                    r = roots_from_words(
                        interpolate_words(body, float(m), qv, zv)[si], rate)
                    ph.append(max(r[0], 20.0) if r else np.nan)
                    zh.append(max(r[2], 20.0) if r else np.nan)
                ax.plot(ph, [y + 0.16] * len(ph), color="#7f1d1d", lw=1.2,
                        alpha=alpha, zorder=2, ls=style)
                if np.isfinite(ph[0]):
                    ax.plot(ph[0], y + 0.16, "o", color="#7f1d1d", ms=5, alpha=alpha)
                if np.isfinite(ph[-1]):
                    ax.plot(ph[-1], y + 0.16, ">", color="#7f1d1d", ms=5, alpha=alpha)
                ax.plot(zh, [y - 0.16] * len(zh), color="#14847a", lw=1.2,
                        alpha=alpha, zorder=2, ls=style)
                if np.isfinite(zh[0]):
                    ax.plot(zh[0], y - 0.16, "o", mfc="none", mec="#14847a", ms=5, alpha=alpha)
                if np.isfinite(zh[-1]):
                    ax.plot(zh[-1], y - 0.16, ">", mfc="none", mec="#14847a", ms=5, alpha=alpha)
    ax.set_xscale("log")
    ax.set_xlim(20, 20_000)
    ax.set_ylim(0.3, NUM + 0.9)
    ax.set_yticks([NUM - s for s in range(NUM)])
    ax.set_yticklabels([f"S{s+1}" for s in range(NUM)], fontsize=8)
    ax.grid(alpha=0.25, which="both", axis="x")
    ax.tick_params(labelsize=7)
    ax.set_title("pole/zero lanes: ● pole  ○ zero, M0 → M100"
                 "  (bold = Q0, faded = Q100"
                 + (", dotted = Transform 2)" if CUBE else ")"), fontsize=9)
    ax.set_xlabel("Hz")

    fig.suptitle(f"{body_path.name}  -  cascade inspector  ({rate:.0f} Hz)", fontsize=13)
    fig.tight_layout(rect=[0, 0, 1, 0.97])
    out = (Path(sys.argv[3]).resolve() if len(sys.argv) > 3
           else body_path.with_name(body_path.stem + "_inspect.png"))
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, dpi=110)

    print(f"peak map, MORPH ride at Q0 ({body_path.name}):")
    for m in np.linspace(0, 1, 6):
        pk = peak_map(response(body, float(m), 0.0, rate))
        desc = "  ".join(f"{f:7.0f} Hz (+{h:4.1f})" for f, h in pk[:5])
        print(f"  M{int(m*100):3d}: {desc if pk else 'smooth - no local peaks'}")
    print(f"plate: {out}")

main()
