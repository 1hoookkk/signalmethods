from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "pyruntime"))

import matplotlib  # noqa: E402

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

from runtime_probe import (GRID, interpolate_words, load_body,  # noqa: E402
                           response, roots_from_words, stage_curves)

RATE = 44_100.0
OX, AMBER = "#7f1d1d", "#c8791a"
SLOTS = 6


def plate(body, title: str, out: Path, rate: float = RATE) -> None:
    m0 = stage_curves(body, 0.0, 0.0, rate)
    m1 = stage_curves(body, 1.0, 0.0, rate)
    cent = [c - np.median(c) for c in list(m0) + list(m1)]
    lim = max(abs(min(c.min() for c in cent)), abs(max(c.max() for c in cent)))
    lim = float(np.ceil(max(lim, 5.0) / 10.0) * 10.0)

    fig = plt.figure(figsize=(21.5, 7.8))
    gs = fig.add_gridspec(2, 7, width_ratios=[1.75, 1, 1, 1, 1, 1, 1],
                          hspace=0.55, wspace=0.22)
    for row, (curves, mval, col, lab) in enumerate(
            [(m0, 0.0, OX, "MORPH 0"), (m1, 1.0, AMBER, "MORPH 100")]):
        whole = fig.add_subplot(gs[row, 0])
        wc = response(body, mval, 0.0, rate)
        whole.semilogx(GRID, wc, color=col, lw=1.0)
        whole.set_xlim(20, 20_000)
        whole.set_ylim(float(np.floor(wc.min() / 10) * 10 - 5),
                       float(np.ceil(wc.max() / 10) * 10 + 5))
        whole.grid(alpha=0.3, which="both")
        whole.tick_params(labelsize=7)
        whole.set_title(f"{lab}  -  the whole cascade", fontsize=11,
                        weight="bold", color=col, pad=6)
        whole.set_xlabel("Hz", fontsize=8)
        for s in range(SLOTS):
            ax = fig.add_subplot(gs[row, s + 1])
            ax.semilogx(GRID, curves[s] - float(np.median(curves[s])),
                        color=col, lw=1.0)
            ax.axhline(0.0, color="0.85", lw=0.6, zorder=0)
            ax.set_xlim(20, 20_000)
            ax.set_ylim(-lim, lim)
            ax.grid(alpha=0.22, which="both")
            ax.tick_params(labelsize=6)
            for sp in ax.spines.values():
                sp.set_color("0.55")
            r = roots_from_words(interpolate_words(body, mval, 0.0)[s], rate)
            ax.set_title(f"slot {s+1}", fontsize=9.5, weight="bold",
                         color=col, pad=6)
            if r:
                ax.text(0.5, -0.19,
                        f"pole {r[0]:,.0f} Hz  r{r[1]:.3f}\n"
                        f"zero {r[2]:,.0f} Hz  r{r[3]:.3f}",
                        transform=ax.transAxes, ha="center", va="top",
                        fontsize=7.6, linespacing=1.5)
            else:
                ax.text(0.5, -0.19, "real-rooted", transform=ax.transAxes,
                        ha="center", va="top", fontsize=7.6, color="0.4")
    fig.suptitle(title, fontsize=14, weight="bold", y=0.985)
    fig.subplots_adjust(left=0.035, right=0.99, top=0.86, bottom=0.13)
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, dpi=110)
    plt.close(fig)


def travel(body, rate: float = RATE) -> str:
    out = []
    for s in range(SLOTS):
        a = roots_from_words(interpolate_words(body, 0.0, 0.0)[s], rate)
        b = roots_from_words(interpolate_words(body, 1.0, 0.0)[s], rate)
        if a and b and a[0] > 0 and b[0] > 0:
            out.append(f"{12 * math.log2(b[0] / a[0]):+.0f}")
        else:
            out.append("--")
    return " ".join(f"{x:>5s}" for x in out)


def main() -> int:
    targets = [ln.split(None, 1) for ln in
               (ROOT / "recipes" / "STUDY_TARGETS.txt")
               .read_text(encoding="utf-8").strip().splitlines()]
    outdir = ROOT / "dev" / "reference" / "study_plates"
    print(f"{len(targets)} study targets -> {outdir}\n")
    print("  id        pole travel per slot, semitones M0 -> M100      name")
    for pid, name in targets:
        src = next((ROOT / "ref" / "presets").glob(f"{pid}_*.bin"), None)
        if src is None:
            print(f"  {pid}  MISSING")
            continue
        body = load_body(src)
        plate(body, f"{name}  ({pid})  -  44100 Hz",
              outdir / f"{pid}_{src.stem.split('_', 2)[-1]}.png")
        print(f"  {pid}  {travel(body)}   {name}")
    print(f"\nwrote {len(targets)} plates")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
