#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools import runtime_probe as rp  # noqa: E402
from tools.plot_measured_octave_morph_cascade import SAMPLE_RATE  # noqa: E402
from tools.plot_measured_vowel_signal_so_far import compile_body  # noqa: E402

OUT = ROOT / "dev" / "reference" / "ETL_S0003_o_i_isolated_sections_44100.png"

def main() -> None:
    body = compile_body()
    rp.verify_word_law(body)
    m0 = rp.stage_curves(body, 0.0, 0.0, SAMPLE_RATE)
    m1 = rp.stage_curves(body, 1.0, 0.0, SAMPLE_RATE)

    fig, axes = plt.subplots(1, 6, figsize=(17, 3.75))
    for s, ax in enumerate(axes):
        offset = float(np.median(np.concatenate([m0[s], m1[s]])))
        ax.semilogx(rp.GRID, m0[s] - offset, color="#7f1d1d", lw=1.65,
                    label="Morph 0  /o/" if s == 0 else None)
        ax.semilogx(rp.GRID, m1[s] - offset, color="#e8a13a", lw=1.65,
                    label="Morph 100  /i/" if s == 0 else None)
        ax.set_xlim(20.0, 20_000.0)
        ax.set_ylim(-45.0, 45.0)
        ax.set_xticks([])
        ax.set_yticks([])
        ax.axhline(0.0, color="0.85", lw=0.6, zorder=0)
        for spine in ax.spines.values():
            spine.set_color("0.55")
        travel = float(np.max(np.abs(m1[s] - m0[s])))
        ax.set_title(f"S{s + 1}", fontsize=11, weight="bold")
        ax.text(0.5, -0.12, f"travel {travel:.0f} dB · level {offset:+.0f} dB",
                transform=ax.transAxes, ha="center", fontsize=8)
        if s == 0:
            ax.text(-0.16, 0.5, "IN", transform=ax.transAxes, ha="right",
                    va="center", fontsize=10, weight="bold")
        if s == 5:
            ax.text(1.16, 0.5, "OUT", transform=ax.transAxes, ha="left",
                    va="center", fontsize=10, weight="bold")
        if s < 5:
            ax.annotate("", xy=(1.14, 0.5), xytext=(1.0, 0.5),
                        xycoords="axes fraction",
                        arrowprops={"arrowstyle": "->", "color": "0.4"})

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=2, frameon=False,
               bbox_to_anchor=(0.5, 0.90))
    fig.suptitle("ETL measured /o/ → /i/ — isolated SOS sections", fontsize=15, y=0.99)
    fig.text(0.5, 0.045,
             "Same cascade-inspector section row · shared level offset per box · shape is not normalized independently",
             ha="center", fontsize=9)
    fig.tight_layout(rect=(0.025, 0.12, 0.985, 0.78), w_pad=1.8)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT, dpi=180, facecolor="#f7f5f0", bbox_inches="tight")
    plt.close(fig)
    print(OUT)

if __name__ == "__main__":
    main()
