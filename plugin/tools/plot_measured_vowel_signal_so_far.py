#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools import runtime_probe as rp  # noqa: E402
from tools.plot_measured_octave_morph_cascade import (  # noqa: E402
    COMPILER,
    HIGH_ID,
    LOW_ID,
    SAMPLE_RATE,
    build_geometry,
    measured_object,
)

OUT = ROOT / "dev" / "reference" / "ETL_S0003_o_i_signal_so_far_44100.png"
COLORS = ["#70757b", "#4477aa", "#66a061", "#b98b2f", "#b65a53", "#25282b"]
ENDPOINTS = [(0.0, "Morph 0 — measured /o/"), (1.0, "Morph 100 — measured /i/")]

def compile_body() -> bytes:
    geometry = build_geometry(measured_object(LOW_ID), measured_object(HIGH_ID))
    with tempfile.TemporaryDirectory(prefix="trench_measured_vowels_") as tmp:
        tmp_dir = Path(tmp)
        geometry_path = tmp_dir / "measured_vowels.geometry.json"
        body_path = tmp_dir / "measured_vowels.body240"
        geometry_path.write_text(json.dumps(geometry), encoding="utf-8")
        result = subprocess.run(
            [str(COMPILER), str(geometry_path), str(body_path)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        return body_path.read_bytes()

def main() -> None:
    body = compile_body()
    if len(body) != 240:
        raise RuntimeError(f"compiler produced {len(body)} bytes, expected 240")
    rp.verify_word_law(body)

    cumulative_by_endpoint = []
    for morph, _ in ENDPOINTS:
        cumulative_by_endpoint.append(np.cumsum(rp.stage_curves(body, morph, 0.0, SAMPLE_RATE), axis=0))

    all_values = np.concatenate([curves.ravel() for curves in cumulative_by_endpoint])
    y_min = 10.0 * np.floor((float(np.min(all_values)) - 4.0) / 10.0)
    y_max = 10.0 * np.ceil((float(np.max(all_values)) + 4.0) / 10.0)

    fig, axes = plt.subplots(1, 2, figsize=(15, 7.2), sharex=True, sharey=True)
    for ax, (_, title), cumulative in zip(axes, ENDPOINTS, cumulative_by_endpoint):
        for stage in range(6):
            final = stage == 5
            ax.semilogx(
                rp.GRID,
                cumulative[stage],
                color=COLORS[stage],
                linewidth=2.35 if final else 1.15,
                alpha=1.0 if final else 0.88,
                label=f"after S1…S{stage + 1}" if stage else "after S1",
            )
        ax.axhline(0.0, color="#7b8085", linewidth=0.7, alpha=0.6)
        ax.set_xlim(20.0, 20_000.0)
        ax.set_ylim(y_min, y_max)
        ax.set_title(title, loc="left", fontsize=13, weight="bold")
        ax.grid(which="both", alpha=0.23)
        ax.set_xlabel("frequency (Hz)")
        ax.set_ylabel("cumulative magnitude (dB)")

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center", ncol=6, frameon=False, bbox_to_anchor=(0.5, 0.932))
    fig.suptitle("ETL measured /o/ → /i/ — packed signal so far", fontsize=19, weight="bold", y=0.99)
    fig.text(
        0.5,
        0.948,
        "Exact 44.1 kHz packed body · serial accumulation S1 → S1×S2 → … → S1×…×S6 · no level normalization",
        ha="center",
        fontsize=11,
    )
    fig.text(
        0.5,
        0.022,
        "S1–S4 = measured F1–F4 frequency + bandwidth-derived radius · degenerate zeros · SCALE 1 · S5–S6 identity · Q collapsed",
        ha="center",
        fontsize=10,
    )
    fig.tight_layout(rect=(0.025, 0.065, 0.995, 0.895))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUT, dpi=160, facecolor="#f7f5f0", bbox_inches="tight")
    plt.close(fig)

    delta = cumulative_by_endpoint[1] - cumulative_by_endpoint[0]
    summary = {
        "status": "DIAGNOSTIC_ONLY_NOT_A_CANONICAL_FILTER",
        "plot": OUT.name,
        "morph_0": LOW_ID,
        "morph_100": HIGH_ID,
        "stage_cumulative_delta_db": [
            {
                "after_stage": stage + 1,
                "rms_over_log_grid": float(np.sqrt(np.mean(delta[stage] ** 2))),
                "maximum_absolute": float(np.max(np.abs(delta[stage]))),
                "frequency_of_maximum_absolute_hz": float(rp.GRID[np.argmax(np.abs(delta[stage]))]),
            }
            for stage in range(6)
        ],
    }
    OUT.with_suffix(".json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(OUT)
    print(OUT.with_suffix(".json"))

if __name__ == "__main__":
    main()
