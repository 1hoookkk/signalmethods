import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from morpheus_axis_census import RATE, is_identity, read_cubes

F = np.geomspace(20.0, 20000.0, 8192)
Z1 = np.exp(-2j * np.pi * F / RATE)
Z2 = Z1 * Z1


def section(sec):
    ph, pr, zh, zr = sec[:4]
    tp, tz = 2.0 * np.pi * ph / RATE, 2.0 * np.pi * zh / RATE
    return (1.0 - 2.0 * zr * math.cos(tz) * Z1 + zr * zr * Z2) / (1.0 - 2.0 * pr * math.cos(tp) * Z1 + pr * pr * Z2)


def db(h):
    return 20.0 * np.log10(np.maximum(np.abs(h), 1e-9))


def grid(ax, title):
    ax.axhline(0.0, color="k", lw=1.0)
    ax.set_xlim(20.0, 20000.0)
    ax.set_ylim(-30.0, 30.0)
    ax.set_yticks(range(-30, 31, 10))
    ax.grid(alpha=0.3, which="both")
    ax.set_title(title, fontsize=11)


def main():
    cubes = read_cubes()
    fig, axes = plt.subplots(1, 3, figsize=(21, 6), sharey=True)
    for ax, k in zip(axes[:2], (237, 96)):
        _, name, secs, g = cubes[k]
        live = [s[0] for s in secs if not is_identity(s[0])]
        h = np.full_like(Z1, g[0])
        for i, sec in enumerate(live):
            hs = section(sec)
            h *= hs
            ax.semilogx(F, db(hs), lw=0.9, alpha=0.8, label="row %d: pole %.0f Hz, zero %.0f Hz" % (i + 1, sec[0], sec[2]))
        ax.semilogx(F, db(h), color="k", lw=2.2, label="cascade x trim %.1f dB" % (20.0 * math.log10(g[0])))
        grid(ax, "%d %s, corner 0: each row thin, cascade bold" % (k, name))
        ax.legend(fontsize=7, loc="lower left")
    _, name, secs, g = cubes[237]
    ax = axes[2]
    for c in range(8):
        h = np.full_like(Z1, g[c])
        for s in secs:
            if not is_identity(s[c]):
                h *= section(s[c])
        ax.semilogx(F, db(h), lw=1.1, label="corner %d (T%d F%d M%d)" % (c, c & 1, (c >> 1) & 1, (c >> 2) & 1))
    grid(ax, "237 PianoSndBrd: all eight corners")
    ax.legend(fontsize=7, loc="lower left", ncol=2)
    fig.suptitle("Morpheus piano cubes at %.1f Hz, unity-gain sections x corner trim; fixed +-30 dB grid" % RATE, fontsize=12)
    fig.tight_layout()
    fig.savefig("piano_ladder.png", dpi=90)


if __name__ == "__main__":
    main()
