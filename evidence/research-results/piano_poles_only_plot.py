import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from morpheus_axis_census import RATE, bw_hz, is_identity, read_cubes

F = np.geomspace(20.0, 20000.0, 8192)
Z1 = np.exp(-2j * np.pi * F / RATE)
Z2 = Z1 * Z1


def pole(ph, pr):
    tp = 2.0 * np.pi * ph / RATE
    return 1.0 / (1.0 - 2.0 * pr * math.cos(tp) * Z1 + pr * pr * Z2)


def db_pinned(h):
    d = 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))
    return d - d.max()


def grid(ax, title):
    ax.axhline(0.0, color="k", lw=1.0)
    ax.set_xlim(20.0, 20000.0)
    ax.set_ylim(-30.0, 30.0)
    ax.set_yticks(range(-30, 31, 10))
    ax.grid(alpha=0.3, which="both")
    ax.set_title(title, fontsize=11)


def main():
    cubes = read_cubes()
    fig, axes = plt.subplots(1, 2, figsize=(15, 6), sharey=True)
    for ax, k in zip(axes, (237, 96)):
        _, name, secs, g = cubes[k]
        poles = [(s[0][0], s[0][1]) for s in secs if not is_identity(s[0]) and s[0][1] > 0.01]
        h = np.ones_like(Z1)
        for i, (ph, pr) in enumerate(sorted(poles)):
            hp = pole(ph, pr)
            h *= hp
            ax.semilogx(F, db_pinned(hp), lw=0.9, alpha=0.85, label="pole %.0f Hz, bw %.1f Hz" % (ph, bw_hz(pr)))
        ax.semilogx(F, db_pinned(h), color="k", lw=2.2, label="all poles together")
        grid(ax, "%d %s, corner 0: poles only, zeros removed" % (k, name))
        ax.legend(fontsize=8, loc="lower left")
    fig.suptitle("Morpheus piano cubes, poles only, at %.1f Hz; every curve pinned to 0 dB at its own peak (section level unproven); fixed +-30 dB grid" % RATE, fontsize=11)
    fig.tight_layout()
    fig.savefig("piano_poles_only.png", dpi=90)


if __name__ == "__main__":
    main()
