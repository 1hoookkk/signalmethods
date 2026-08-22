#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import runtime_probe as rp  # noqa: E402

RATE = 44_100.0
OUT = ROOT / "dev" / "reference"
PRESETS = [
    ("P2k_010_ooh_to_eee", "Ooh-To-Eee"),
    ("P2k_012_multi_q_vox", "MultiQVox"),
    ("P2k_013_talking_hedz", "TalkingHedz"),
    ("P2k_020_eeh_to_aah", "Eeh-To-Aah"),
    ("P2k_021_ubu_orator", "UbuOrator"),
    ("P2k_022_deep_bouche", "DeepBouche"),
]
CORNERS = [(0.0, 0.0, "M0 Q0"), (1.0, 0.0, "M100 Q0"),
           (0.0, 1.0, "M0 Q100"), (1.0, 1.0, "M100 Q100")]
COLORS = ["#70757b", "#4477aa", "#66a061", "#b98b2f", "#b65a53", "#25282b"]

def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for stem, title in PRESETS:
        body_path = ROOT / "ref" / "presets" / f"{stem}.bin"
        body = rp.load_body(body_path)
        rp.verify_word_law(body)
        fig, axes = plt.subplots(2, 2, figsize=(15, 10), sharex=True, sharey=True)
        for ax, (morph, q, corner) in zip(axes.ravel(), CORNERS):
            stages = rp.stage_curves(body, morph, q, RATE)
            cumulative = np.cumsum(stages, axis=0)
            for stage in range(6):
                final = stage == 5
                ax.semilogx(rp.GRID, cumulative[stage], color=COLORS[stage],
                            linewidth=2.35 if final else 1.15,
                            alpha=1.0 if final else 0.88,
                            label=f"after S1…S{stage + 1}" if stage else "after S1")
            ax.axhline(0.0, color="#7b8085", linewidth=0.7, alpha=0.6)
            ax.set_xlim(20, 20_000)
            ax.set_ylim(-120, 60)
            ax.set_title(corner, loc="left", fontsize=13, weight="bold")
            ax.grid(which="both", alpha=0.23)
            ax.set_ylabel("cumulative magnitude (dB)")
        for ax in axes[-1]:
            ax.set_xlabel("frequency (Hz)")
        handles, labels = axes[0, 0].get_legend_handles_labels()
        fig.legend(handles, labels, loc="upper center", ncol=6, frameon=False,
                   bbox_to_anchor=(0.5, 0.946))
        fig.suptitle(f"{stem} {title} — packed signal so far",
                     fontsize=19, weight="bold", y=0.99)
        fig.text(0.5, 0.958,
                 "Exact 44.1 kHz bank · serial accumulation S1 → S1×S2 → … → S1×…×S6 · no level normalization",
                 ha="center", fontsize=11)
        fig.tight_layout(rect=(0.025, 0.035, 0.995, 0.91))
        out = OUT / f"{stem}_signal_so_far_44100.png"
        fig.savefig(out, dpi=160, facecolor="#f7f5f0", bbox_inches="tight")
        plt.close(fig)
        print(out)

if __name__ == "__main__":
    main()
