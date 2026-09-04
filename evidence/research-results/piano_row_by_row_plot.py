import math

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from morpheus_axis_census import RATE, bw_hz, is_identity, read_cubes

F = np.geomspace(20.0, 20000.0, 16384)
Z1 = np.exp(-2j * np.pi * F / RATE)
Z2 = Z1 * Z1


def num(zh, zr):
    tz = 2.0 * np.pi * zh / RATE
    return 1.0 - 2.0 * zr * math.cos(tz) * Z1 + zr * zr * Z2


def den(ph, pr):
    tp = 2.0 * np.pi * ph / RATE
    return 1.0 - 2.0 * pr * math.cos(tp) * Z1 + pr * pr * Z2


def db(h):
    return 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))


def grid(ax, title):
    ax.axhline(0.0, color="k", lw=1.0)
    ax.set_xlim(20.0, 20000.0)
    ax.set_ylim(-30.0, 30.0)
    ax.set_yticks(range(-30, 31, 10))
    ax.grid(alpha=0.3, which="both")
    ax.set_title(title, fontsize=10)


def main():
    cubes = read_cubes()
    _, name, secs, g = cubes[237]
    rows = [s[0] for s in secs if not is_identity(s[0])]
    poles = sorted((r[0], r[1]) for r in rows if r[1] > 0.01)
    zeros = sorted((r[2], r[3]) for r in rows if r[3] > 0.01)
    rungs = []
    for zh, zr in zeros:
        ph, pr = min(poles, key=lambda p: abs(math.log(p[0] / zh)))
        poles.remove((ph, pr))
        rungs.append((ph, pr, zh, zr))
    spare = poles
    steps = [("Null cube: every row identity, flat", None)]
    for ph, pr, zh, zr in rungs:
        steps.append(("rung %.0f Hz: pole bw %.1f Hz, zero bw %.1f Hz" % (ph, bw_hz(pr), bw_hz(zr)), num(zh, zr) / den(ph, pr)))
    for ph, pr in spare:
        steps.append(("spare pole %.0f Hz, bw %.0f Hz: treble tilt" % (ph, bw_hz(pr)), 1.0 / den(ph, pr)))
    steps.append(("corner trim %.1f dB last" % (20.0 * math.log10(g[0])), np.full_like(Z1, g[0])))
    stored = np.full_like(Z1, g[0])
    for r in rows:
        stored *= num(r[2], r[3]) / den(r[0], r[1])
    fig, axes = plt.subplots(3, 3, figsize=(18, 12), sharex=True, sharey=True)
    h = np.ones_like(Z1)
    for i, (ax, (title, part)) in enumerate(zip(axes.flat, steps)):
        if part is not None:
            ax.semilogx(F, db(h), color="0.6", lw=1.0, label="before")
            ax.semilogx(F, db(part), color="C1", lw=0.9, alpha=0.9, label="this row alone")
            h = h * part
        ax.semilogx(F, db(h), color="k", lw=2.0, label="after")
        grid(ax, "%d. %s" % (i, title))
        if i == 1:
            ax.legend(fontsize=8, loc="lower left")
    err = np.max(np.abs(db(h) - db(stored)))
    fig.suptitle("237 %s corner 0 built row by row, raw product, each rung = pole with its zero at the same frequency; final curve = stored rows' product (max diff %.1e dB); fixed +-30 dB grid" % (name, err), fontsize=11)
    fig.tight_layout()
    fig.savefig("piano_row_by_row.png", dpi=90)
    print("rungs (pole Hz, pole bw, zero bw, peak dB above the line):")
    for ph, pr, zh, zr in rungs:
        d = db(num(zh, zr) / den(ph, pr))
        print("  %7.1f  %6.2f  %6.2f  %+5.1f" % (ph, bw_hz(pr), bw_hz(zr), d.max()))
    print("spare poles: %s; final vs stored max diff %.2e dB" % (["%.0f Hz r %.3f" % p for p in spare], err))


if __name__ == "__main__":
    main()
