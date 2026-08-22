#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "dev" / "reference" / "p2k_vowel_isolated_sections_44100.json"
OUTPUT = ROOT / "dev" / "reference" / "p2k_vowel_section_breakdown_44100.png"
CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
COLORS = {
    "M0_Q0": "#0f6f86",
    "M100_Q0": "#d56a25",
    "M0_Q100": "#6b4fa1",
    "M100_Q100": "#3f8b55",
}
OFFSETS = {"M0_Q0": -0.18, "M100_Q0": -0.06, "M0_Q100": 0.06, "M100_Q100": 0.18}

def main() -> None:
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    fig, axes = plt.subplots(3, 2, figsize=(18, 17), sharex=True, sharey=True)
    axes = axes.ravel()
    for ax, preset in zip(axes, data["presets"]):
        ax.set_xscale("log")
        ax.set_xlim(120, 22050)
        ax.set_ylim(6.6, 0.4)
        ax.set_yticks(range(1, 7), [f"S{i}" for i in range(1, 7)])
        ax.grid(axis="x", which="both", alpha=0.22)
        ax.grid(axis="y", alpha=0.18)
        ax.set_title(f"{preset['id']}  {preset['name']}", loc="left", fontsize=15, weight="bold")
        for section in preset["sections"]:
            stage = section["stage"]
            for corner in CORNERS:
                row = section["corners"][corner]
                y = stage + OFFSETS[corner]
                pole = row["pole"]
                zero = row["zero"]
                if pole["topology"] == "conjugate_pair":
                    ax.scatter(pole["hz"], y, s=54, marker="o", color=COLORS[corner],
                               edgecolor="white", linewidth=0.55, zorder=4)
                else:
                    ax.text(128, y, "real P", color=COLORS[corner], fontsize=8, va="center")
                if zero["topology"] == "conjugate_pair":
                    ax.scatter(zero["hz"], y, s=54, marker="x", color=COLORS[corner],
                               linewidth=1.6, zorder=4)
                elif zero["topology"] == "real_pair":
                    ax.text(128, y, "2 real Z", color=COLORS[corner], fontsize=8,
                            va="center", weight="bold")
                else:
                    ax.text(128, y, "off Z", color=COLORS[corner], fontsize=8, va="center")
        order = preset["stage_frequency_order_low_to_high_by_corner"]["M0_Q0"]
        rank = " < ".join(f"S{s}" for s in order["conjugate_stages_low_to_high"])
        ax.text(0.01, -0.13, f"M0_Q0 pole order: {rank}", transform=ax.transAxes,
                fontsize=9, color="#3c4650")

    for ax in axes[-2:]:
        ax.set_xlabel("root-pair frequency at the exact 44.1 kHz datum (Hz)", fontsize=11)
    for ax in axes[::2]:
        ax.set_ylabel("authored stage identity", fontsize=11)

    corner_handles = [Line2D([0], [0], marker="o", color="none", markerfacecolor=COLORS[c],
                             markeredgecolor="white", markersize=8, label=c)
                      for c in CORNERS]
    root_handles = [
        Line2D([0], [0], marker="o", color="#252b31", linestyle="none", markersize=8,
               label="pole pair"),
        Line2D([0], [0], marker="x", color="#252b31", linestyle="none", markersize=8,
               label="zero pair"),
    ]
    fig.legend(handles=corner_handles + root_handles, loc="upper center", ncol=6,
               frameon=False, bbox_to_anchor=(0.5, 0.955), fontsize=11)
    fig.suptitle("P2K vowel presets — every isolated S1–S6 section at all four corners",
                 fontsize=22, weight="bold", y=0.988)
    fig.text(0.5, 0.962,
             "Stage rows preserve authored identity; they are deliberately not renumbered by frequency. "
             "Circle = pole pair, × = zero pair.",
             ha="center", va="top", fontsize=12)
    fig.text(0.5, 0.012,
             "Exact packed-bank decode. DeepBouche S5 M0_Q100 contains two real zeros and is labeled instead of assigned a fake Hz value.",
             ha="center", fontsize=10)
    fig.tight_layout(rect=(0.035, 0.035, 0.995, 0.935), h_pad=2.8, w_pad=1.5)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=170, facecolor="#f7f5f0", bbox_inches="tight")
    plt.close(fig)
    print(OUTPUT)

if __name__ == "__main__":
    main()
