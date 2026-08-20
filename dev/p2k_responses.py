"""Literal magnitude responses of the decoded P2K presets.

H(z) = prod_k  scale_k * N_k(z) / D_k(z)

Root geometry is taken exactly as decoded. Conjugate roots give
[1, -2R cos w, R^2]; real-axis pairs [a, b] give [1, -(a+b), a*b] --
they are NOT collapsed to a single radius.

Sample rate comes from the recipe's own datum_sr_hz field; nothing assumed.
"""
import os
import json
import glob
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plots", "p2k")
os.makedirs(OUT, exist_ok=True)

CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
CCOL = ["#2a78d6", "#eb6834", "#1baf7a", "#a05ad6"]
NPTS = 8192
DB_LO, DB_HI = -90.0, 45.0

SURF, INK, INK2, MUTED = "#fcfcfb", "#0b0b0b", "#52514e", "#898781"
WELL, GRAT, GRATM = "#101014", "#26262c", "#3a3a42"

plt.rcParams.update({
    "figure.facecolor": SURF, "savefig.facecolor": SURF, "text.color": INK,
    "font.family": "sans-serif", "font.sans-serif": ["Segoe UI", "DejaVu Sans"],
    "axes.labelcolor": INK2, "xtick.color": MUTED, "ytick.color": MUTED,
})


def load_preset(path):
    d = json.load(open(path, encoding="utf-8"))
    sr = float(d["datum_sr_hz"])          # from the file, not assumed
    return d, sr


def root_coeffs(g, sr):
    """Return the 3 polynomial coefficients [1, c1, c2] for one root pair."""
    if "pair" in g:
        a, b = g["pair"]
        return 1.0, -(a + b), a * b
    r = float(g["r"])
    w = 2.0 * math.pi * float(g["hz"]) / sr
    return 1.0, -2.0 * r * math.cos(w), r * r


def corner_response(sections, corner, sr, grid):
    """Complete serial cascade, and each section's own factor."""
    w = 2.0 * math.pi * grid / sr
    e1, e2 = np.exp(-1j * w), np.exp(-2j * w)
    per = []
    total = np.zeros(len(grid))
    for sec in sections:
        c = sec["corners"][corner]
        nb = root_coeffs(c["zero"], sr)
        da = root_coeffs(c["pole"], sr)
        scale = float(c.get("scale", 1.0))
        num = scale * (nb[0] + nb[1] * e1 + nb[2] * e2)
        den = da[0] + da[1] * e1 + da[2] * e2
        db = 20.0 * np.log10(np.maximum(np.abs(num), 1e-30) /
                             np.maximum(np.abs(den), 1e-30))
        per.append(db)
        total += db
    return total, per


def style_axes(ax, sr, ylo=DB_LO, yhi=DB_HI, xlabel=True, ylabel=True):
    ax.set_facecolor(WELL)
    ax.set_xscale("log")
    ax.set_xlim(20.0, sr / 2.0)
    ax.set_ylim(ylo, yhi)
    for db in range(int(ylo) // 10 * 10, int(yhi) + 1, 10):
        ax.axhline(db, color=GRATM if db == 0 else GRAT, lw=0.7, zorder=0)
    for hz in (20, 50, 100, 200, 500, 1000, 2000, 5000, 10000):
        ax.axvline(hz, color=GRAT, lw=0.7, zorder=0)
    ax.set_xticks([20, 50, 100, 200, 500, 1000, 2000, 5000, 10000])
    ax.set_xticklabels(["20", "50", "100", "200", "500", "1k", "2k", "5k",
                        "10k"] if xlabel else [])
    yt = list(range(int(ylo) // 10 * 10, int(yhi) + 1, 20))
    ax.set_yticks(yt)
    ax.set_yticklabels([str(v) for v in yt] if ylabel else [])
    ax.tick_params(labelsize=8, length=3)
    for sp in ax.spines.values():
        sp.set_color(GRATM)
    if xlabel:
        ax.set_xlabel("Hz", fontsize=9)
    if ylabel:
        ax.set_ylabel("dB", fontsize=9)


def figure_corners(d, sr, path):
    grid = np.geomspace(20.0, sr / 2.0 * 0.999, NPTS)
    fig, ax = plt.subplots(figsize=(13.0, 6.4))
    style_axes(ax, sr)
    for ci, cn in enumerate(CORNERS):
        tot, _ = corner_response(d["sections"], cn, sr, grid)
        ax.plot(grid, tot, color=CCOL[ci], lw=1.7, label=cn, zorder=3)
    ax.set_title(f'{d["name"]}   ·   {len(d["sections"])} sections in series   '
                 f'·   datum {sr:g} Hz',
                 fontsize=13, fontweight="bold", loc="left", pad=8, color=INK)
    ax.legend(fontsize=9, frameon=False, labelcolor=INK2, loc="lower left",
              ncol=4)
    fig.tight_layout()
    fig.savefig(path, dpi=140)
    plt.close(fig)


def figure_sections(d, sr, path):
    grid = np.geomspace(20.0, sr / 2.0 * 0.999, NPTS)
    n = len(CORNERS)
    fig, axes = plt.subplots(1, n, figsize=(19.0, 4.8))
    for ci, cn in enumerate(CORNERS):
        tot, per = corner_response(d["sections"], cn, sr, grid)
        ax = axes[ci]
        style_axes(ax, sr, ylabel=(ci == 0))
        for k, p in enumerate(per):
            ax.plot(grid, p, color="#5a8fd0", lw=0.9, alpha=0.55, zorder=2)
        ax.plot(grid, tot, color=CCOL[ci], lw=2.2, zorder=4)
        ax.set_title(cn, fontsize=10, color=INK, loc="left", pad=5)
    fig.suptitle(f'{d["name"]}   ·   thick = complete cascade, '
                 f'thin = each section\'s own factor',
                 fontsize=12, fontweight="bold", x=0.006, ha="left")
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    fig.savefig(path, dpi=140)
    plt.close(fig)


def slug(name):
    return "".join(ch if ch.isalnum() else "_" for ch in name).strip("_").lower()


if __name__ == "__main__":
    files = sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json")))
    picks = ["Ace of Bass", "TalkingHedz", "Ooh-To-Eee", "BassBox-303",
             "ToothComb"]
    lo, hi = [], []
    chosen = []
    for f in files:
        d, sr = load_preset(f)
        if d["name"] in picks:
            chosen.append((f, d, sr))
        grid = np.geomspace(20.0, sr / 2.0 * 0.999, 1024)
        for cn in CORNERS:
            t, _ = corner_response(d["sections"], cn, sr, grid)
            lo.append(t.min())
            hi.append(t.max())
    print(f"corpus response range: min {min(lo):.1f} dB   max {max(hi):.1f} dB")
    print(f"5th/95th pct of per-corner minima/maxima: "
          f"{np.percentile(lo,5):.1f} / {np.percentile(hi,95):.1f} dB")
    print(f"fixed plotting window in use: {DB_LO:.0f} .. {DB_HI:.0f} dB")
    print()
    for f, d, sr in chosen:
        s = slug(d["name"])
        figure_corners(d, sr, os.path.join(OUT, f"{s}_corners.png"))
        figure_sections(d, sr, os.path.join(OUT, f"{s}_sections.png"))
        grid = np.geomspace(20.0, sr / 2.0 * 0.999, 1024)
        rng = []
        for cn in CORNERS:
            t, _ = corner_response(d["sections"], cn, sr, grid)
            rng.append(f"{cn} [{t.min():+.0f},{t.max():+.0f}]")
        print(f"  {d['name']:14s} sr={sr:g}  sections={len(d['sections'])}  "
              + "  ".join(rng))


def gallery_card(d, sr, path, norm=None):
    """Same plot as the verification figures, sized for a gallery card."""
    grid = np.geomspace(20.0, sr / 2.0 * 0.999, NPTS)
    fig, ax = plt.subplots(figsize=(6.6, 3.5))
    lo, hi = (-70.0, 40.0) if norm is not None else (DB_LO, DB_HI)
    style_axes(ax, sr, ylo=lo, yhi=hi)
    for ci, cn in enumerate(CORNERS):
        tot, _ = corner_response(d["sections"], cn, sr, grid)
        ax.plot(grid, tot - (norm or 0.0), color=CCOL[ci], lw=1.4, zorder=3)
    ax.set_title(d["name"], fontsize=11, fontweight="bold", loc="left",
                 pad=5, color=INK)
    fig.tight_layout()
    fig.savefig(path, dpi=100)
    plt.close(fig)


def build_gallery():
    import base64
    files = sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json")))
    cards, cards_n = [], []
    for f in files:
        d, sr = load_preset(f)
        s = slug(d["name"])
        big = os.path.join(OUT, f"{s}_corners.png")
        sec = os.path.join(OUT, f"{s}_sections.png")
        figure_corners(d, sr, big)
        figure_sections(d, sr, sec)
        card = os.path.join(OUT, f"card_{s}.png")
        gallery_card(d, sr, card)
        grid = np.geomspace(20.0, sr / 2.0 * 0.999, 1024)
        peak = max(corner_response(d["sections"], cn, sr, grid)[0].max()
                   for cn in CORNERS)
        cardn = os.path.join(OUT, f"norm_{s}.png")
        gallery_card(d, sr, cardn, norm=float(peak))
        b64 = lambda p: base64.b64encode(open(p, "rb").read()).decode()
        cards.append((d["name"], len(d["sections"]), sr, b64(card)))
        cards_n.append((d["name"], b64(cardn)))
        print(f"  {d['name']:16s} peak {peak:+6.1f} dB")

    def grid_html(items, cap):
        out = [f'<h2>{cap}</h2><div class="grid">']
        for it in items:
            nm = it[0]
            img = it[-1]
            meta = (f"{it[1]} sections · {it[2]:g} Hz" if len(it) > 2 else "")
            out.append(f'<figure><img src="data:image/png;base64,{img}">'
                       f'<figcaption>{nm}<span>{meta}</span></figcaption>'
                       f'</figure>')
        out.append("</div>")
        return "\n".join(out)

    html = f"""<!doctype html><meta charset="utf-8">
<title>P2K serial cascade responses</title>
<style>
 body{{background:#fcfcfb;color:#0b0b0b;font:14px/1.5 "Segoe UI",system-ui,sans-serif;margin:28px}}
 h1{{font-size:24px;margin:0 0 4px}} h2{{font-size:17px;margin:34px 0 12px}}
 p.sub{{color:#52514e;margin:0 0 6px;max-width:1100px}}
 .grid{{display:grid;grid-template-columns:repeat(auto-fill,minmax(520px,1fr));gap:18px}}
 figure{{margin:0;background:#fff;border:1px solid #e1e0d9;border-radius:8px;padding:8px}}
 img{{width:100%;display:block}}
 figcaption{{font-size:12px;color:#52514e;padding:6px 4px 2px;display:flex;
   justify-content:space-between}}
 figcaption span{{color:#898781}}
 .key{{display:flex;gap:18px;font-size:12px;color:#52514e;margin:10px 0 0}}
 .key i{{width:22px;height:3px;display:inline-block;margin-right:6px;
   vertical-align:middle}}
</style>
<h1>P2K factory presets — complete serial cascade magnitude response</h1>
<p class="sub">H(z) = &prod;<sub>k</sub> scale<sub>k</sub>·N<sub>k</sub>(z)/D<sub>k</sub>(z),
 evaluated from the decoded pole/zero geometry exactly as stored. Conjugate roots
 [1, &minus;2R·cos&omega;, R&sup2;]; real-axis pairs [a,b] evaluated as
 [1, &minus;(a+b), a·b]. Sample rate taken from each recipe's own datum_sr_hz field
 (39,062.5 Hz for all 33). 8192 log-spaced points, 20 Hz to Nyquist.
 All four authored corners overlaid.</p>
<div class="key">
 <span><i style="background:{CCOL[0]}"></i>M0_Q0</span>
 <span><i style="background:{CCOL[1]}"></i>M100_Q0</span>
 <span><i style="background:{CCOL[2]}"></i>M0_Q100</span>
 <span><i style="background:{CCOL[3]}"></i>M100_Q100</span>
</div>
{grid_html(cards, f"All {len(cards)} presets · absolute dB, fixed {DB_LO:.0f}…{DB_HI:.0f} dB window")}
{grid_html(cards_n, "Shape comparison · each preset offset by one constant so its loudest corner sits at 0 dB")}
"""
    p = os.path.join(OUT, "index.html")
    open(p, "w", encoding="utf-8").write(html)
    print(f"\nwrote {p}  ({os.path.getsize(p)/1e6:.1f} MB, {len(cards)} presets)")
