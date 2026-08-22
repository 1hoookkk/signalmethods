"""Extract and deliberately exaggerate the TB-303 corner resonances.

The four inputs are renders of the same ARX-friendly, saw-like harmonic comb.
The common excitation cancels in each Q100/Q0 ratio.  This tool finds the
dominant Q contribution at M0 and M100, then scales the complete measured Q
delta by an explicit factor without moving its frequency or inventing lanes.

It writes research targets only.  It does not assign P2K sections or pack a
body; the ordered TB-OrNot-TB corners remain the fitter seeds.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from scipy import signal
from scipy.io import wavfile


SR_DATUM = 44_100.0
P2K_POINTS = 512
P2K_LOW_HZ = 20.0
P2K_HIGH_HZ = 0.499 * SR_DATUM
WELCH_SEGMENT = 16_384
WELCH_OVERLAP = 12_288


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def load_mono(path: Path) -> tuple[int, np.ndarray]:
    sample_rate, samples = wavfile.read(path)
    samples = np.asarray(samples, dtype=np.float64)
    if samples.ndim == 2:
        samples = samples.mean(axis=1)
    return int(sample_rate), samples


def welch_db(samples: np.ndarray, sample_rate: int) -> tuple[np.ndarray, np.ndarray]:
    hz, power = signal.welch(
        samples,
        fs=sample_rate,
        window="hann",
        nperseg=WELCH_SEGMENT,
        noverlap=WELCH_OVERLAP,
        scaling="spectrum",
    )
    return hz, 10.0 * np.log10(np.maximum(power, 1.0e-30))


def estimate_comb_spacing(hz: np.ndarray, db: np.ndarray) -> float:
    """Find the dense probe spacing, avoiding the old third-line F0 error."""
    magnitude = np.power(10.0, db / 20.0)
    candidates = np.linspace(15.5, 17.0, 1501)
    scores = np.empty(candidates.size)
    for index, spacing in enumerate(candidates):
        harmonics = np.arange(2, int(5000.0 / spacing) + 1)
        lines = harmonics * spacing
        on = np.interp(lines, hz, magnitude)
        off_a = np.interp(lines - 0.45 * spacing, hz, magnitude)
        off_b = np.interp(lines + 0.45 * spacing, hz, magnitude)
        scores[index] = np.median(
            20.0 * np.log10(np.maximum(on, 1.0e-30))
            - 10.0 * np.log10(np.maximum(off_a * off_b, 1.0e-60))
        )
    return float(candidates[int(np.argmax(scores))])


def peak_hold_envelope(
    hz: np.ndarray, db: np.ndarray, grid: np.ndarray, comb_spacing: float
) -> np.ndarray:
    held = np.empty(grid.size)
    half_width = 0.52 * comb_spacing
    for index, center in enumerate(grid):
        selected = (hz >= center - half_width) & (hz <= center + half_width)
        held[index] = db[selected].max() if selected.any() else np.interp(center, hz, db)

    # The probe is saw-like: harmonic amplitude falls approximately as 1/k.
    compensated = held + 20.0 * np.log10(np.maximum(grid / comb_spacing, 1.0))
    reference = np.median(compensated[(grid >= 40.0) & (grid <= 100.0)])
    return compensated - reference


def joint_reliable_band(
    hz: np.ndarray,
    q0_db: np.ndarray,
    q100_db: np.ndarray,
    floor_db: float,
    high_hz: float,
) -> tuple[float, float]:
    reliable = (
        (hz >= 30.0)
        & (hz <= high_hz)
        & (q0_db > q0_db.max() - floor_db)
        & (q100_db > q100_db.max() - floor_db)
    )
    indices = np.flatnonzero(reliable)
    if indices.size == 0:
        raise RuntimeError("no jointly reliable Q0/Q100 band")
    return float(hz[indices[0]]), float(hz[indices[-1]])


def dominant_resonance(
    grid: np.ndarray,
    q0_target: np.ndarray,
    q100_target: np.ndarray,
    reliable_band: tuple[float, float],
) -> tuple[float, float]:
    selected = (
        (grid >= max(40.0, reliable_band[0]))
        & (grid <= reliable_band[1])
    )
    indices = np.flatnonzero(selected)
    if indices.size == 0:
        raise RuntimeError("no target points in jointly reliable band")
    peak_index = int(indices[int(np.argmax(q100_target[indices]))])
    return (
        float(grid[peak_index]),
        float(q100_target[peak_index] - q0_target[peak_index]),
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("m0_q0", type=Path)
    parser.add_argument("m100_q0", type=Path)
    parser.add_argument("m0_q100", type=Path)
    parser.add_argument("m100_q100", type=Path)
    parser.add_argument("--factor", type=float, default=1.5)
    parser.add_argument("--floor-db", type=float, default=75.0)
    parser.add_argument("--comb-spacing", type=float)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.factor < 1.0:
        raise ValueError("--factor must be at least 1.0")

    labels = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
    paths = [args.m0_q0, args.m100_q0, args.m0_q100, args.m100_q100]
    spectra: list[np.ndarray] = []
    sample_rate = None
    frequency = None
    for path in paths:
        current_rate, samples = load_mono(path)
        if sample_rate is None:
            sample_rate = current_rate
        if current_rate != sample_rate:
            raise ValueError(f"sample-rate mismatch: {path} is {current_rate} Hz")
        frequency, db = welch_db(samples, current_rate)
        spectra.append(db)
    assert frequency is not None and sample_rate is not None
    if sample_rate != int(SR_DATUM):
        raise ValueError(f"P2K targets require 44100 Hz; inputs are {sample_rate} Hz")

    spacing = (
        float(args.comb_spacing)
        if args.comb_spacing is not None
        else estimate_comb_spacing(frequency, spectra[1])
    )
    grid = np.geomspace(P2K_LOW_HZ, P2K_HIGH_HZ, P2K_POINTS)
    measured = np.vstack(
        [peak_hold_envelope(frequency, curve, grid, spacing) for curve in spectra]
    )
    pushed = measured.copy()
    pushed[2] = measured[0] + args.factor * (measured[2] - measured[0])
    pushed[3] = measured[1] + args.factor * (measured[3] - measured[1])

    # The low-M render is intentionally dark, so its joint reliable band ends
    # much earlier than the high-M pair.  Search only where both captures speak.
    low_band = joint_reliable_band(
        frequency, spectra[0], spectra[2], args.floor_db, 1000.0
    )
    high_band = joint_reliable_band(
        frequency, spectra[1], spectra[3], args.floor_db, 5000.0
    )
    low = dominant_resonance(grid, measured[0], measured[2], low_band)
    high = dominant_resonance(grid, measured[1], measured[3], high_band)

    args.output.mkdir(parents=True, exist_ok=True)
    payload = {
        "schema": "trench.tb303-fundamentals.v1",
        "corner_order": labels,
        "sample_rate_hz": sample_rate,
        "probe_comb_spacing_hz": spacing,
        "exaggeration": {
            "factor": args.factor,
            "law": "Q100_pushed = Q0 + factor * (Q100_measured - Q0)",
            "q0_corners_changed": False,
            "center_frequencies_moved": False,
        },
        "fundamental_resonances": {
            "M0": {
                "frequency_hz": low[0],
                "measured_q_delta_db": low[1],
                "pushed_q_delta_db": args.factor * low[1],
                "reliable_band_hz": list(low_band),
            },
            "M100": {
                "frequency_hz": high[0],
                "measured_q_delta_db": high[1],
                "pushed_q_delta_db": args.factor * high[1],
                "reliable_band_hz": list(high_band),
            },
        },
        "sources": [
            {"corner": label, "path": str(path.resolve()), "sha256": sha256(path)}
            for label, path in zip(labels, paths)
        ],
        "grid_hz": grid.tolist(),
        "measured_target_db": {
            label: measured[index].tolist() for index, label in enumerate(labels)
        },
        "pushed_target_db": {
            label: pushed[index].tolist() for index, label in enumerate(labels)
        },
    }
    json_path = args.output / "tb303-fundamentals-1p5.json"
    json_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    figure, axes = plt.subplots(2, 2, figsize=(12, 7), sharex=True, sharey=True)
    for index, (axis, label) in enumerate(zip(axes.flat, labels)):
        axis.semilogx(grid, measured[index], color="#768089", linewidth=1.4, label="measured")
        axis.semilogx(grid, pushed[index], color="#e76837", linewidth=1.8, label="1.5x Q character")
        if index == 2:
            axis.axvline(low[0], color="#e0b04b", linestyle="--", linewidth=1.0)
        if index == 3:
            axis.axvline(high[0], color="#e0b04b", linestyle="--", linewidth=1.0)
        axis.set_title(label)
        axis.grid(True, which="both", alpha=0.18)
    axes[0, 0].legend(loc="lower left")
    figure.supxlabel("Frequency (Hz)")
    figure.supylabel("Probe-compensated target (dB, relative)")
    figure.suptitle("TB-303 fundamentals — measured and explicit 1.5x Q emphasis")
    figure.tight_layout()
    figure.savefig(args.output / "tb303-fundamentals-1p5.png", dpi=160)
    plt.close(figure)

    print(f"probe comb spacing: {spacing:.4f} Hz")
    print(
        f"M0 fundamental: {low[0]:.1f} Hz, Q delta {low[1]:+.2f} -> "
        f"{args.factor * low[1]:+.2f} dB"
    )
    print(
        f"M100 fundamental: {high[0]:.1f} Hz, Q delta {high[1]:+.2f} -> "
        f"{args.factor * high[1]:+.2f} dB"
    )
    print(json_path)


if __name__ == "__main__":
    main()
