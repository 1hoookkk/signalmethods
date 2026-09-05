import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "native", "python"))
import trench_core as tc
import novel_bodies as nb

CORNERS = ["M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1"]


def section_curve(row, hz):
    bq = (tc.ctypes.c_double * 5)()
    w = (tc.ctypes.c_uint16 * 5)(*row)
    tc._dll.trench_section_design(tc.ctypes.byref(w), tc.ctypes.c_double(nb.DATUM), tc.ctypes.byref(bq))
    b = [float(bq[i]) for i in range(5)]
    return np.array([tc.section_response_db(b, f, nb.DATUM) for f in hz])


def labels(row, cur):
    g = nb.geom_get(row)
    if g.pole_type != 1:
        return "FC  -", "BW  -", "GAIN  -"
    bw = -math.log(max(g.pole_b, 1e-6)) * nb.DATUM / math.pi
    return f"FC  {g.pole_a:.0f} Hz", f"BW  {bw:.0f} Hz", f"GAIN  {cur.max():+.0f} dB"


def render(path, corners, name, notes):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    hz = np.geomspace(20, 20000, 300)
    fig = plt.figure(figsize=(14, 9), dpi=120, facecolor="white")
    gs = fig.add_gridspec(4, 8, left=0.12, right=0.99, top=0.9, bottom=0.1, wspace=0.55, hspace=0.9, width_ratios=[1] * 6 + [0.2, 1.6])
    for c, words in enumerate(corners):
        y0 = 1.0
        for s in range(6):
            ax = fig.add_subplot(gs[c, s])
            cur = section_curve(words[s], hz)
            ax.plot(hz, cur, color="black", lw=1.1)
            ax.set_xscale("log"); ax.set_xlim(20, 20000); ax.set_ylim(-30, 30)
            ax.axhline(0, color="#999999", lw=0.5)
            ax.set_xticks([]); ax.set_yticks([])
            for sp in ax.spines.values(): sp.set_linewidth(1.4)
            fc, bw, gain = labels(words[s], cur)
            ax.text(0.5, -0.12, fc, transform=ax.transAxes, ha="center", va="top", fontsize=7, family="monospace")
            ax.text(0.5, -0.30, bw, transform=ax.transAxes, ha="center", va="top", fontsize=7, family="monospace")
            ax.text(0.5, -0.48, gain, transform=ax.transAxes, ha="center", va="top", fontsize=7, family="monospace")
            if c == 0: ax.set_title(f"{s + 1}", fontsize=9, family="monospace", pad=4)
            if s == 0: ax.text(-0.35, 0.5, CORNERS[c] + "\n" + notes[c], transform=ax.transAxes, ha="right", va="center", fontsize=8, family="monospace")
        ax = fig.add_subplot(gs[c, 7])
        ax.plot(hz, nb.response(corners, c & 1, c >> 1, hz), color="black", lw=1.4)
        ax.set_xscale("log"); ax.set_xlim(20, 20000); ax.set_ylim(-30, 30)
        ax.axhline(0, color="#999999", lw=0.5)
        ax.set_xticks([100, 1000, 10000]); ax.set_xticklabels(["100", "1k", "10k"], fontsize=7)
        ax.set_yticks([-20, 0, 20]); ax.tick_params(labelsize=7)
        ax.grid(True, color="#dddddd", lw=0.5, ls=":")
        if c == 0: ax.set_title("cascade", fontsize=9, family="monospace", pad=4)
    fig.text(0.12, 0.95, f"{name}   6 sections in series, 240 bytes, four corners; MORPH across, Q up", fontsize=11, family="monospace")
    fig.savefig(path, facecolor="white")
    print(path)


if __name__ == "__main__":
    front, left, behind, above = (nb.frame(n) for n in ("left ear az 0 el 0", "left ear az 90 el 0", "left ear az 180 el 0", "left ear az 0 el 60"))
    corners = [nb.unity(c) for c in (front, left, behind, above)]
    render(sys.argv[1], corners, "head", ["front", "left side", "behind", "front, 60 up"])
