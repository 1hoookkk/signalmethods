import os
import glob
import struct
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plots", "corpus")
SR = 39062.5

HZ_LO, HZ_HI = 40.0, 16000.0
HZ_TICKS = [40, 100, 200, 500, 1000, 2000, 5000, 10000, 16000]
DB_LO, DB_HI, CROWN = -60.0, 40.0, 36.0

WELL = "#101014"
GRAT = "#26262c"
GRAT_MAJ = "#34343c"
INK = "#0b0b0b"
INK2 = "#52514e"
MUTED = "#898781"
SURF = "#fcfcfb"
LIVE = "#48c8ff"
CUM = "#9aa0a6"
PAIR = "#ffb454"
CEIL = "#e05a4f"

GRID = np.geomspace(HZ_LO, HZ_HI, 1024)


def decode(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))


def stage_words_to_kernel(w):
    d = [decode(x) for x in w]
    return [4 * d[0] + d[1], d[1], 4 * d[2] + d[3], d[3], 4 * d[4]]


def kernel_to_biquad(k):
    return [k[4], (k[0] - 2) * k[4], (1 - k[1]) * k[4], k[2] - 2, 1 - k[3]]


def row_db(c, hz, sr=SR):
    w = 2 * np.pi * np.asarray(hz) / sr
    cw, sw = np.cos(w), np.sin(w)
    c2, s2 = np.cos(2 * w), np.sin(2 * w)
    nr = c[0] + c[1] * cw + c[2] * c2
    ni = -(c[1] * sw + c[2] * s2)
    dr = 1 + c[3] * cw + c[4] * c2
    di = -(c[3] * sw + c[4] * s2)
    return 10.0 * np.log10(np.maximum(nr * nr + ni * ni, 1e-30) /
                           np.maximum(dr * dr + di * di, 1e-30))


def load_body(path):
    raw = open(path, "rb").read()
    assert len(raw) == 560, path
    w = struct.unpack("<280H", raw)
    return np.array(w).reshape(8, 7, 5)


def biquads(body, corner):
    return [kernel_to_biquad(stage_words_to_kernel(body[corner][s]))
            for s in range(7)]


def is_inert(row, tol=1e-9):
    d = row_db(row, GRID)
    return float(np.abs(d).max()) < 0.05


def frame(ax, ylab=True, xlab=False, small=False):
    ax.set_facecolor(WELL)
    ax.set_xscale("log")
    ax.set_xlim(HZ_LO, HZ_HI)
    ax.set_ylim(DB_LO, DB_HI)
    for db in range(-60, 41, 10):
        ax.axhline(db, color=GRAT_MAJ if db == 0 else GRAT, lw=0.8, zorder=0)
    for hz in HZ_TICKS:
        ax.axvline(hz, color=GRAT, lw=0.8, zorder=0)
    ax.axhline(CROWN, color=CEIL, lw=1.0, ls=(0, (4, 3)), zorder=1)
    ticks = [100, 1000, 10000] if small else HZ_TICKS
    ax.set_xticks(ticks)
    ax.set_xticklabels([(f"{h//1000}k" if h >= 1000 else str(h))
                        for h in ticks] if xlab else [])
    ax.set_yticks([-60, -30, 0, 30] if small else range(-60, 41, 20))
    if not ylab:
        ax.set_yticklabels([])
    ax.tick_params(colors=MUTED, labelsize=7 if small else 8, length=3)
    for sp in ax.spines.values():
        sp.set_color(GRAT_MAJ)
    ax.grid(False)


def figure(name, body, corner, path):
    rows = biquads(body, corner)
    own = [row_db(r, GRID) for r in rows]
    live = [not is_inert(r) for r in rows]
    cum = np.cumsum(np.array(own), axis=0)
    total = cum[-1]

    fig = plt.figure(figsize=(16.2, 9.4))
    fig.patch.set_facecolor(SURF)
    gs = GridSpec(4, 7, figure=fig, height_ratios=[1.55, 1, 1, 1],
                  hspace=0.40, wspace=0.10,
                  left=0.045, right=0.988, top=0.858, bottom=0.058)

    fig.text(0.045, 0.966, f"{name}   ·   corner {corner}", fontsize=17,
             fontweight="bold")
    fig.text(0.045, 0.930,
             r"Serial factorization  $H(z)=\prod_{k=1}^{7}H_k(z)$   ·   "
             f"native 560-byte body words → kernel → biquad → 10·log10|N|²/|D|²  "
             f"·  {SR:g} Hz  ·  lane order fixed, never sorted or re-paired",
             fontsize=9.5, color=INK2)

    ax = fig.add_subplot(gs[0, :4])
    frame(ax, xlab=True)
    ax.plot(GRID, np.clip(total, DB_LO, DB_HI), color=LIVE, lw=1.8)
    ax.set_title("complete product   H(z) = H₁·H₂·…·H₇", fontsize=10,
                 color=INK, loc="left", pad=5, fontweight="bold")
    ax.set_ylabel("dB", color=INK2)

    ax2 = fig.add_subplot(gs[0, 4:])
    frame(ax2, ylab=False, xlab=True)
    for k in range(7):
        if live[k]:
            ax2.plot(GRID, np.clip(own[k], DB_LO, DB_HI), color=LIVE, lw=1.0,
                     alpha=0.55)
    ax2.plot(GRID, np.clip(total, DB_LO, DB_HI), color=LIVE, lw=1.8)
    ax2.set_title("the seven factors over their product  ·  "
                  "no factor is an EQ band", fontsize=10, color=INK,
                  loc="left", pad=5, fontweight="bold")

    for k in range(7):
        a = fig.add_subplot(gs[1, k])
        frame(a, ylab=(k == 0), small=True)
        if live[k]:
            a.plot(GRID, np.clip(own[k], DB_LO, DB_HI), color=LIVE, lw=1.3)
        else:
            a.text(0.5, 0.5, "H = 1", transform=a.transAxes, ha="center",
                   va="center", color=MUTED, fontsize=8, style="italic")
        a.set_title(f"$H_{{{k+1}}}$", fontsize=9, color=INK if live[k] else MUTED,
                    pad=3)

    for k in range(6):
        a = fig.add_subplot(gs[2, k])
        frame(a, ylab=(k == 0), small=True)
        pair = own[k] + own[k + 1]
        a.plot(GRID, np.clip(pair, DB_LO, DB_HI), color=PAIR, lw=1.3)
        a.set_title(f"$H_{{{k+1}}}H_{{{k+2}}}$", fontsize=9, color=INK, pad=3)
    a = fig.add_subplot(gs[2, 6])
    a.axis("off")
    a.text(0.0, 0.62, "adjacent\nproducts", fontsize=8.5, color=INK2,
           va="center", linespacing=1.5, transform=a.transAxes)

    for k in range(7):
        a = fig.add_subplot(gs[3, k])
        frame(a, ylab=(k == 0), xlab=True, small=True)
        a.plot(GRID, np.clip(cum[k], DB_LO, DB_HI), color=CUM, lw=1.3)
        a.set_title(f"H₁..H{chr(0x2080+k+1)}  running", fontsize=8.5,
                    color=INK, pad=3)
    fig.savefig(path, dpi=150, facecolor=SURF)
    plt.close(fig)


if __name__ == "__main__":
    files = {os.path.basename(p): p
             for p in glob.glob(os.path.join(ROOT, "ref/morpheus/bodies/*.body"))}
    ident = np.array([0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF])
    k = kernel_to_biquad(stage_words_to_kernel(ident))
    print("identity row max |dB| =",
          float(np.abs(row_db(k, GRID)).max()), "(must be ~0)")
    picks = ["001_LPFlange.4.body", "029_Vocal_Cube.body", "022_AEParaVowel.body",
             "225_Phaser.body", "034_Be-Ye.4.body"]
    for f in picks:
        if f not in files:
            cand = [n for n in files if n.split("_", 1)[-1].startswith(
                f.split("_", 1)[-1][:6])]
            if not cand:
                print("missing", f)
                continue
            f = cand[0]
        body = load_body(files[f])
        name = f.split("_", 1)[1].replace(".body", "")
        best, bestlive = 0, -1
        for c in range(8):
            n = sum(1 for r in biquads(body, c) if not is_inert(r))
            if n > bestlive:
                best, bestlive = c, n
        figure(name, body, best,
               os.path.join(OUT, "x3_" + name.lower().replace(" ", "_")
                            .replace(".", "") + ".png"))
        print(f"  {name:14s} corner {best}  live factors {bestlive}")
