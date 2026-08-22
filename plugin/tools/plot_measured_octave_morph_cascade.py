#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import LinearSegmentedColormap

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools import runtime_probe as rp  # noqa: E402

DATASET = ROOT / "recipes" / "tables" / "academia" / "etl_mokhtari_tanaka_2000.json"
COMPILER = ROOT / "target" / "release" / "body-from-geometry.exe"
DEFAULT_OUT = ROOT / "dev" / "reference" / "measured_high_octave_morph_cascade_44100.png"
SAMPLE_RATE = 44_100.0
LOW_ID = "ETL_S0003_o_w01_f5"
HIGH_ID = "ETL_S0003_i_w01_f5"
FORMANTS = ("F1", "F2", "F3", "F4")

def measured_object(object_id: str) -> dict:
    data = json.loads(DATASET.read_text(encoding="utf-8"))
    for obj in data["objects"]:
        if obj["object_id"] == object_id:
            return obj
    raise KeyError(object_id)

def formant_map(obj: dict) -> dict[str, dict[str, float]]:
    return {row["mode_or_formant"]: row for row in obj["formants"]}

def pole_radius(bandwidth_hz: float) -> float:
    return math.exp(-math.pi * bandwidth_hz / SAMPLE_RATE)

def active_stage(row: dict[str, float]) -> dict[str, float]:
    return {
        "pole_hz": float(row["frequency_hz"]),
        "pole_r": pole_radius(float(row["bandwidth_hz"])),
        "zero_hz": 0.0,
        "zero_r": 0.0,
        "scale": 1.0,
    }

def identity_stage() -> dict[str, float]:
    return {"pole_hz": 0.0, "pole_r": 0.0, "zero_hz": 0.0, "zero_r": 0.0, "scale": 1.0}

def endpoint_stages(obj: dict) -> list[dict[str, float]]:
    fm = formant_map(obj)
    stages = [active_stage(fm[name]) for name in FORMANTS]
    stages.extend([identity_stage(), identity_stage()])
    return stages

def build_geometry(low: dict, high: dict) -> dict:
    low_stages = endpoint_stages(low)
    high_stages = endpoint_stages(high)
    return {
        "name": "Measured /o/ to /i/ high-octave diagnostic",
        "purpose": "diagnostic_only_not_a_canonical_filter",
        "corners": [low_stages, high_stages, low_stages, high_stages],
    }

def cumulative_db(body: bytes, morph: float) -> np.ndarray:
    curves = np.asarray(rp.stage_curves(body, morph, 0.0, SAMPLE_RATE))
    return np.cumsum(curves, axis=0)

def decoded_endpoint(body: bytes, corner: int) -> list[dict[str, float]]:
    out = []
    for lane, values in enumerate(rp.corner_geometry(body, corner, SAMPLE_RATE), start=1):
        pole_hz, pole_r, zero_hz, zero_r, scale = values
        out.append({
            "lane": lane,
            "pole_hz": pole_hz,
            "pole_r": pole_r,
            "zero_hz": zero_hz,
            "zero_r": zero_r,
            "scale": scale,
        })
    return out

def add_endpoint_band(ax, center: float, bandwidth: float, color: str, label: str) -> None:
    lo = max(20.0, center - bandwidth / 2.0)
    hi = min(20_000.0, center + bandwidth / 2.0)
    ax.axvspan(lo, hi, color=color, alpha=0.13, linewidth=0)
    ax.axvline(center, color=color, alpha=0.75, linewidth=1.1)
    ax.text(center, 0.975, label, color=color, rotation=90, ha="right", va="top",
            transform=ax.get_xaxis_transform(), fontsize=8.5)

def make_plot(body: bytes, low: dict, high: dict, out_path: Path) -> None:
    low_f = formant_map(low)
    high_f = formant_map(high)
    f2_low = float(low_f["F2"]["frequency_hz"])
    f2_high = float(high_f["F2"]["frequency_hz"])
    bw_low = float(low_f["F2"]["bandwidth_hz"])
    bw_high = float(high_f["F2"]["bandwidth_hz"])
    semitones = 12.0 * math.log2(f2_high / f2_low)

    plt.style.use("seaborn-v0_8-whitegrid")
    fig = plt.figure(figsize=(14.4, 9.0))
    gs = fig.add_gridspec(2, 2, height_ratios=(1.08, 1.0))
    ax_ride = fig.add_subplot(gs[0, :])
    ax_low = fig.add_subplot(gs[1, 0])
    ax_high = fig.add_subplot(gs[1, 1], sharey=ax_low)

    low_color = "#176f8a"
    high_color = "#d55e32"
    cmap = LinearSegmentedColormap.from_list("morph", [low_color, "#74757a", high_color])
    morphs = np.linspace(0.0, 1.0, 9)
    ride = []
    for i, morph in enumerate(morphs):
        y = rp.response(body, float(morph), 0.0, SAMPLE_RATE)
        ride.append(y)
        is_endpoint = i in (0, len(morphs) - 1)
        ax_ride.semilogx(
            rp.GRID,
            y,
            color=cmap(float(morph)),
            linewidth=2.4 if is_endpoint else 1.05,
            alpha=1.0 if is_endpoint else 0.63,
            label=f"Morph {int(round(morph * 100))}" if is_endpoint else None,
        )
    add_endpoint_band(ax_ride, f2_low, bw_low, low_color, f"M0 F2  {f2_low:.1f} Hz  BW {bw_low:.1f}")
    add_endpoint_band(ax_ride, f2_high, bw_high, high_color, f"M100 F2  {f2_high:.1f} Hz  BW {bw_high:.1f}")
    ax_ride.annotate(
        f"measured F2 travel  {semitones:.2f} st",
        xy=(f2_high, np.max(ride[-1][(rp.GRID > f2_high * 0.94) & (rp.GRID < f2_high * 1.06)])),
        xytext=(math.sqrt(f2_low * f2_high), np.nanmax(ride) + 5.0),
        arrowprops={"arrowstyle": "->", "color": "#444444", "linewidth": 1.0},
        ha="center",
        fontsize=10,
    )

    endpoint_specs = [
        (ax_low, 0.0, low, low_color, "Morph 0 — measured /o/"),
        (ax_high, 1.0, high, high_color, "Morph 100 — measured /i/"),
    ]
    all_cumulative = []
    for ax, morph, obj, color, title in endpoint_specs:
        curves = cumulative_db(body, morph)
        all_cumulative.append(curves)
        for stage_idx in range(4):
            is_final = stage_idx == 3
            ax.semilogx(
                rp.GRID,
                curves[stage_idx],
                color=color if is_final else "#777777",
                linewidth=2.8 if is_final else 1.0,
                alpha=1.0 if is_final else 0.38 + 0.13 * stage_idx,
                label=f"S1…S{stage_idx + 1}" + ("  complete measured cascade" if is_final else ""),
            )
        fm = formant_map(obj)
        for stage_idx, name in enumerate(FORMANTS):
            row = fm[name]
            frequency = float(row["frequency_hz"])
            bandwidth = float(row["bandwidth_hz"])
            ax.axvspan(frequency - bandwidth / 2.0, frequency + bandwidth / 2.0,
                       color=color, alpha=0.045, linewidth=0)
        ax.set_title(title, loc="left", fontsize=12, fontweight="bold")
        ax.set_xlabel("Frequency (Hz)")
        ax.legend(loc="lower left", fontsize=8.5, frameon=False)

    y_all = np.concatenate([np.asarray(ride).ravel(), np.asarray(all_cumulative).ravel()])
    y_lo, y_hi = np.nanpercentile(y_all, [0.2, 99.8])
    pad = max(8.0, 0.08 * (y_hi - y_lo))
    for ax in (ax_ride, ax_low, ax_high):
        ax.set_xlim(20.0, 20_000.0)
        ax.set_ylim(y_lo - pad, y_hi + pad)
        ax.grid(True, which="major", alpha=0.35)
        ax.grid(True, which="minor", alpha=0.13)
    ax_ride.set_ylabel("Cascade magnitude (dB, unnormalised)")
    ax_low.set_ylabel("Signal so far (dB, unnormalised)")
    ax_ride.legend(loc="lower left", frameon=False)

    fig.suptitle("Measured high-octave Morph — exact packed-runtime cascade", fontsize=17, fontweight="bold")
    fig.text(
        0.5,
        0.024,
        "ETL S0003 · word 01 · frame 5 · F1–F4 measured frequency + bandwidth only  |  "
        "r = exp(−π·BW/44100)  |  degenerate zeros  |  SCALE = 1  |  Q rows copied  |  S5–S6 identity",
        ha="center",
        fontsize=9,
        color="#444444",
    )
    fig.subplots_adjust(left=0.066, right=0.988, bottom=0.105, top=0.915, hspace=0.29, wspace=0.09)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out_path, dpi=180, bbox_inches="tight")
    plt.close(fig)

def write_manifest(body: bytes, low: dict, high: dict, geometry: dict, out_path: Path) -> None:
    def measured_rows(obj: dict) -> list[dict[str, float | str]]:
        fm = formant_map(obj)
        return [{
            "lane": i + 1,
            "measurement": name,
            "frequency_hz": float(fm[name]["frequency_hz"]),
            "bandwidth_hz": float(fm[name]["bandwidth_hz"]),
            "derived_pole_r": pole_radius(float(fm[name]["bandwidth_hz"])),
        } for i, name in enumerate(FORMANTS)]

    f2_low = formant_map(low)["F2"]["frequency_hz"]
    f2_high = formant_map(high)["F2"]["frequency_hz"]
    manifest = {
        "status": "DIAGNOSTIC_ONLY_NOT_A_CANONICAL_FILTER",
        "plot": out_path.name,
        "sample_rate_hz": SAMPLE_RATE,
        "dataset": str(DATASET.relative_to(ROOT)).replace("\\", "/"),
        "source_note": "Mokhtari & Tanaka 2000 ETL vowel measurements",
        "morph_low": {"object_id": low["object_id"], "vowel": "/o/", "rows": measured_rows(low)},
        "morph_high": {"object_id": high["object_id"], "vowel": "/i/", "rows": measured_rows(high)},
        "f2_travel_semitones": 12.0 * math.log2(float(f2_high) / float(f2_low)),
        "authoring_assumptions": [
            "S1-S4 correspond to measured F1-F4",
            "pole radius derived as exp(-pi * bandwidth_hz / 44100)",
            "zeros are degenerate",
            "each section SCALE is 1",
            "Q100 is a byte-equivalent target copy of Q0",
            "S5-S6 are identity sections",
            "no level normalisation is applied",
        ],
        "packed_body_sha256": hashlib.sha256(body).hexdigest(),
        "packed_word_law": "verified_against_trench_core",
        "requested_geometry": geometry,
        "decoded_m0": decoded_endpoint(body, 0),
        "decoded_m100": decoded_endpoint(body, 1),
    }
    out_path.with_suffix(".json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    if not COMPILER.exists():
        raise SystemExit(f"missing compiler: {COMPILER}")
    low = measured_object(LOW_ID)
    high = measured_object(HIGH_ID)
    geometry = build_geometry(low, high)
    with tempfile.TemporaryDirectory(prefix="trench_measured_octave_") as tmp:
        tmp_dir = Path(tmp)
        geometry_path = tmp_dir / "measured_octave.geometry.json"
        body_path = tmp_dir / "measured_octave.body240"
        geometry_path.write_text(json.dumps(geometry, indent=2), encoding="utf-8")
        result = subprocess.run(
            [str(COMPILER), str(geometry_path), str(body_path)],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        if result.returncode:
            raise SystemExit(result.stdout + result.stderr)
        body = body_path.read_bytes()
    if len(body) != 240:
        raise SystemExit(f"compiler produced {len(body)} bytes, expected 240")
    rp.verify_word_law(body)
    make_plot(body, low, high, args.out)
    write_manifest(body, low, high, geometry, args.out)
    print(args.out)
    print(args.out.with_suffix(".json"))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
