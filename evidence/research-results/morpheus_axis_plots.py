import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from morpheus_axis_census import AXES, RATE, read_cubes

PICK = [22, 29, 43, 48, 63, 73, 85, 96, 156, 198, 225, 245]
F = np.geomspace(20.0, 20000.0, 2048)
Z1 = np.exp(-2j * np.pi * F / RATE)
Z2 = Z1 * Z1


def corner_db(secs, g, c):
    h = np.full_like(Z1, g[c])
    for s in secs:
        ph, pr, zh, zr = s[c][:4]
        tp, tz = 2.0 * np.pi * ph / RATE, 2.0 * np.pi * zh / RATE
        h *= (1.0 - 2.0 * zr * math.cos(tz) * Z1 + zr * zr * Z2) / (1.0 - 2.0 * pr * math.cos(tp) * Z1 + pr * pr * Z2)
    return 20.0 * np.log10(np.maximum(np.abs(h), 1e-9))


def main():
    cubes = read_cubes()
    for axis, bit in AXES.items():
        fig, axes = plt.subplots(3, 4, figsize=(20, 11), sharex=True, sharey=True)
        for ax, k in zip(axes.flat, PICK):
            _, name, secs, g = cubes[k]
            a, b = 0, bit
            ax.semilogx(F, corner_db(secs, g, a), color="0.45", lw=1.6, label="corner %d (axis 0)" % a)
            ax.semilogx(F, corner_db(secs, g, b), color="C3", lw=1.6, label="corner %d (axis 1)" % b)
            ax.axhline(0.0, color="k", lw=1.0)
            ax.set_xlim(20.0, 20000.0)
            ax.set_ylim(-30.0, 30.0)
            ax.set_yticks(range(-30, 31, 10))
            ax.grid(alpha=0.3, which="both")
            ax.set_title("%d %s" % (k, name), fontsize=10)
        axes.flat[0].legend(fontsize=8, loc="lower left")
        fig.suptitle("Morpheus %s axis: raw slot 0 (grey) -> slot %d (red), other axes at 0; cascade of 7 sections x corner gain at %.1f Hz; fixed +-30 dB grid" % (
            axis, bit, RATE), fontsize=13)
        fig.tight_layout()
        fig.savefig("morpheus_axis_%s.png" % axis.lower(), dpi=90)
        plt.close(fig)


if __name__ == "__main__":
    main()
