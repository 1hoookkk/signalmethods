import json
import math
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OCTAVES = 10.0
SR = 39_062.5
REF_HZ = SR / 2048.0
CORNER_NAMES = ["M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100"]
CORNER_COLOURS = ["#c96a54", "#4a7c9b", "#7b9e6b", "#9b7bb0"]

def theta_prime(hz):
    theta = 2.0 * math.pi * max(hz, REF_HZ) / SR
    return math.pi * (OCTAVES + math.log2(theta / math.pi)) / OCTAVES

def resonance_db(r):
    return 20.0 * math.log10(1.0 / max(1.0 - min(r, 0.999999), 1e-9))

def read_geometry(path):
    d = json.loads(Path(path).read_text())
    out = []
    for corner in d["corners"]:
        pts = [(s["pole_hz"], s["pole_r"]) for s in corner]
        out.append(pts)
    return d.get("name", Path(path).stem), out

def plot_body(path, ax_polar, ax_arma, show_legend):
    name, corners = read_geometry(path)
    for ci, pts in enumerate(corners):
        colour = CORNER_COLOURS[ci % len(CORNER_COLOURS)]
        label = CORNER_NAMES[ci] if show_legend else None
        th = [2 * math.pi * hz / 44100.0 for hz, _ in pts]
        rr = [r for _, r in pts]
        ax_polar.scatter(th, rr, s=42, c=colour, edgecolors="none",
                         alpha=0.85, label=label, zorder=3)
        th2 = [theta_prime(hz) for hz, _ in pts]
        rr2 = [resonance_db(r) for _, r in pts]
        ax_arma.scatter(th2, rr2, s=42, c=colour, edgecolors="none",
                        alpha=0.85, label=label, zorder=3)
    return name

def style_polar(ax):
    ax.set_title("traditional polar z-plane\n(Rossum Fig 3 - the armadillo)",
                 fontsize=9, pad=14)
    ax.set_ylim(0, 1.0)
    ax.set_thetamin(0)
    ax.set_thetamax(180)
    ax.set_yticks([0.5, 0.9, 0.99, 1.0])
    ax.set_yticklabels(["0.5", "0.9", "0.99", "1"], fontsize=7)
    ax.set_xticks([math.pi * k / 6 for k in range(7)])
    ax.set_xticklabels([f"{44100 * k / 12:.0f}" for k in range(7)], fontsize=7)
    ax.grid(alpha=0.3)

def style_arma(ax, rmax):
    ax.set_title("ARMAdillo plane\nradius = dB resonance, angle = octaves",
                 fontsize=9, pad=14)
    ax.set_ylim(0, rmax)
    ax.set_thetamin(0)
    ax.set_thetamax(180)
    ax.set_xticks([math.pi * k / OCTAVES for k in range(0, 11, 2)])
    ax.set_xticklabels([f"{REF_HZ * 2 ** k:.0f}" for k in range(0, 11, 2)], fontsize=7)
    ax.set_yticks([20, 40, 60, 80])
    ax.set_yticklabels(["20 dB", "40", "60", "80"], fontsize=7)
    ax.grid(alpha=0.3)

def main():
    paths = sys.argv[1:]
    if not paths:
        paths = sorted(str(p) for p in
                       Path("plugin/presets/bodies").glob("*.geometry.json"))[:6]
    if not paths:
        raise SystemExit("no *.geometry.json found")

    n = len(paths)
    fig, axes = plt.subplots(n, 2, figsize=(8.2, 3.6 * n),
                             subplot_kw={"projection": "polar"})
    if n == 1:
        axes = [axes]
    for row, path in zip(axes, paths):
        name = plot_body(path, row[0], row[1], show_legend=(path == paths[0]))
        style_polar(row[0])
        style_arma(row[1], rmax=90)
        row[0].set_ylabel(name, fontsize=8, labelpad=28)
    if n:
        axes[0][1].legend(loc="upper right", bbox_to_anchor=(1.28, 1.18),
                          fontsize=7, frameon=False)
    fig.suptitle("Rossum log plot - even coefficient density = even perceptual mapping",
                 fontsize=11, y=0.998)
    fig.tight_layout(rect=(0, 0, 1, 0.985))
    out = Path("plots/armadillo_plane.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, dpi=150)
    print(f"wrote {out} ({n} bodies)")

if __name__ == "__main__":
    main()
