import ctypes
import sys
from pathlib import Path

import numpy as np

ROOT = Path(r"C:\Users\hooki\trench-workstation")
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib  # noqa: E402

lib.trench_stage_roots_from_words_at.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_double,
    ctypes.POINTER(ctypes.c_double)]
lib.trench_stage_roots_from_words_at.restype = ctypes.c_int

RATE = 44_100.0

BODIES = [ROOT / "ref" / "presets" / "P2k_013_talking_hedz.bin",
          ROOT / "bodies" / "archive" / "P2K_TB_or_Not_TB.body240"] + sorted(
    (ROOT / "evidence" / "p2k_inspect").glob("P2k_*.body240"))

def corner_geometry(body: bytes, corner: int):
    words = np.frombuffer(body, dtype="<u2").reshape(4, 6, 5)
    rows = []
    for s in range(6):
        w = (ctypes.c_uint16 * 5)(*words[corner, s])
        out = (ctypes.c_double * 5)()
        rows.append(None if lib.trench_stage_roots_from_words_at(w, RATE, out) != 0
                    else tuple(out))
    return rows

import math
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

n = len(BODIES)
ncols = 3
nrows = math.ceil(n / ncols)
fig, axes = plt.subplots(nrows, ncols, figsize=(16, 2.7 * nrows))
for ax in axes.flat[n:]:
    ax.axis("off")
for ax, path in zip(axes.flat, BODIES):
    body = path.read_bytes()
    geo = [corner_geometry(body, ci) for ci in range(4)]
    for s in range(6):
        y = 6 - s
        for (ca, cb), alpha in (((0, 1), 0.95), ((2, 3), 0.30)):
            a, b = geo[ca][s], geo[cb][s]
            if a is None or b is None:
                continue
            fa, fb = max(a[0], 20.0), max(b[0], 20.0)
            ax.plot([fa, fb], [y + 0.16, y + 0.16], color="#7f1d1d", lw=1.1, alpha=alpha)
            ax.plot(fa, y + 0.16, "o", color="#7f1d1d", ms=3.5, alpha=alpha)
            ax.plot(fb, y + 0.16, ">", color="#7f1d1d", ms=3.5, alpha=alpha)
            za, zb = max(a[2], 20.0), max(b[2], 20.0)
            ax.plot([za, zb], [y - 0.16, y - 0.16], color="#14847a", lw=1.1, alpha=alpha)
            ax.plot(za, y - 0.16, "o", mfc="none", mec="#14847a", ms=3.5, alpha=alpha)
            ax.plot(zb, y - 0.16, ">", mfc="none", mec="#14847a", ms=3.5, alpha=alpha)
    ax.set_xscale("log")
    ax.set_xlim(20, 20_000)
    ax.set_ylim(0.3, 6.9)
    ax.set_yticks([6 - s for s in range(6)])
    ax.set_yticklabels([f"S{s+1}" for s in range(6)], fontsize=7)
    ax.grid(alpha=0.25, which="both", axis="x")
    ax.tick_params(labelsize=6)
    name = path.stem.replace("P2k_", "").replace("_inspect", "")
    name = " ".join(w.capitalize() for w in name.split("_")[1:]) or path.stem
    ax.set_title(name, fontsize=9)
fig.suptitle("pole/zero lanes per preset: \u25cf pole  \u25cb zero, arrow = M0 \u2192 M100"
             "  (bold = Q0, faded = Q100)", fontsize=12)
fig.tight_layout(rect=[0, 0, 1, 0.98])
out = Path(__file__).with_name("lane_sheet.png")
fig.savefig(out, dpi=115)
print(out)
