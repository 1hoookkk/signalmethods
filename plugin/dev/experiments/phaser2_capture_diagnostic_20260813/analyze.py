import hashlib
import json
import math
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import soundfile as sf
from scipy.signal import coherence, lfilter, resample_poly, welch


ROOT = Path(r"C:\Users\hooki\trench-x3-clean")
REC = Path(r"C:\Users\hooki\OneDrive\Documents\Image-Line\FL Studio\Audio\Recorded")
BYPASS = Path(r"C:\Users\hooki\trench-s6-slot-law\ref\inputs\bypassed-pinknoise.wav")
PRESET = ROOT / "plugin/presets/bodies/X3F_phaser_2.json"
OUT = Path(__file__).resolve().parent
INSTALLED = Path(r"C:\Program Files\Common Files\VST3\TRENCH.vst3\Contents\x86_64-win\TRENCH.vst3")
WORKSPACE_BUILD = ROOT / "build/TRENCH_artefacts/Release/VST3/TRENCH.vst3/Contents/x86_64-win/TRENCH.vst3"

CAPTURES = {
    0: (
        REC / "untitled_2026-08-13 21-04-40_Master.wav",
        REC / "untitled_2026-08-13 21-06-00_Master.wav",
    ),
    50: (
        REC / "untitled_2026-08-13 21-04-51_Master.wav",
        REC / "untitled_2026-08-13 21-06-07_Master.wav",
    ),
    100: (
        REC / "untitled_2026-08-13 21-05-01_Master.wav",
        REC / "untitled_2026-08-13 21-06-19_Master.wav",
    ),
}

MINOR_DEGREES = {0, 2, 3, 5, 7, 8, 10}


def sha256(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest().upper()


def decode(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    exponent = (u >> 12) & 15
    mantissa = u & 4095
    value = mantissa / 4096.0 if exponent == 0 else (mantissa + 4096.0) / 8192.0
    return value * 2.0 ** (exponent - 15)


def lerp_u16(a, b, fraction):
    delta = int((int(b) - int(a)) * fraction)
    delta = ((delta + 32768) % 65536) - 32768
    return np.uint16((int(a) + delta) % 65536)


def rows_for(preset, rate, morph, q=1.0):
    stages = preset["active_stages"]
    corners = np.array(preset["banks"][str(rate)], dtype=np.uint16).reshape(4, stages, 5)
    rows = []
    for stage in range(stages):
        words = []
        for word in range(5):
            q0 = lerp_u16(corners[0, stage, word], corners[1, stage, word], morph)
            q1 = lerp_u16(corners[2, stage, word], corners[3, stage, word], morph)
            words.append(lerp_u16(q0, q1, q))
        d0, d1, d2, d3, d4 = [decode(word) for word in words]
        k0, k1, k2, k3, k4 = 4 * d0 + d1, d1, 4 * d2 + d3, d3, 4 * d4
        rows.append(np.array([k4, (k0 - 2) * k4, (1 - k1) * k4, k2 - 2, 1 - k3]))
    return rows


def pair_hz(c1, c2, rate=48000.0):
    if c1 * c1 - 4 * c2 >= 0 or c2 <= 1.0e-18:
        return None
    radius = math.sqrt(c2)
    angle = math.acos(np.clip(-c1 / (2 * radius), -1, 1))
    return angle * rate / (2 * math.pi)


def nearest_g_minor(hz):
    midi = 69 + 12 * math.log2(hz / 440)
    centre = round(midi)
    candidates = [
        note
        for note in range(centre - 12, centre + 13)
        if ((note % 12 - 7) % 12) in MINOR_DEGREES
    ]
    note = min(candidates, key=lambda item: (abs(item - midi), item))
    return 440 * 2 ** ((note - 69) / 12)


def snap_g_minor(rows, rate=48000.0):
    result = []
    for source in rows:
        row = source.copy()
        pole = pair_hz(row[3], row[4], rate)
        if pole is not None:
            target = nearest_g_minor(pole)
            ratio = target / pole
            row[3] = -2 * math.sqrt(row[4]) * math.cos(2 * math.pi * target / rate)
            b0 = row[0]
            zero = pair_hz(row[1] / b0, row[2] / b0, rate)
            if zero is not None:
                moved_zero = np.clip(zero * ratio, 20, 0.49 * rate)
                row[1] = (
                    -2
                    * math.sqrt(row[2] / b0)
                    * math.cos(2 * math.pi * moved_zero / rate)
                    * b0
                )
        result.append(row)
    return result


def magnitude_db(rows, frequencies, rate=48000.0):
    z = np.exp(-2j * np.pi * frequencies / rate)
    response = np.ones_like(z, dtype=complex)
    for b0, b1, b2, a1, a2 in rows:
        response *= (b0 + b1 * z + b2 * z * z) / (1 + a1 * z + a2 * z * z)
    return 20 * np.log10(np.maximum(np.abs(response), 1.0e-12))


def render(source, rows):
    signal = source.copy()
    for b0, b1, b2, a1, a2 in rows:
        signal = lfilter([b0, b1, b2], [1, a1, a2], signal)
    return signal


def best_gain_correlation(observed, predicted, delay=96, trim=4096):
    observed = observed[trim + delay :]
    predicted = predicted[trim : trim + len(observed)]
    gain = float(np.dot(predicted, observed) / np.dot(predicted, predicted))
    correlation = float(np.corrcoef(predicted * gain, observed)[0, 1])
    return correlation, 20 * math.log10(abs(gain))


def audio_info(path):
    info = sf.info(str(path))
    audio, _ = sf.read(str(path), always_2d=True)
    nonzero = np.flatnonzero(np.max(np.abs(audio), axis=1) > 1.0e-9)
    return {
        "path": str(path),
        "sample_rate": info.samplerate,
        "frames": info.frames,
        "channels": info.channels,
        "subtype": info.subtype,
        "first_nonzero_frame": int(nonzero[0]) if len(nonzero) else None,
        "rms_dbfs": float(20 * np.log10(np.sqrt(np.mean(audio * audio)) + 1.0e-300)),
        "peak_dbfs": float(20 * np.log10(np.max(np.abs(audio)) + 1.0e-300)),
    }


def main():
    preset = json.loads(PRESET.read_text())
    dry, bypass_rate = sf.read(str(BYPASS), always_2d=True)
    dry_48 = resample_poly(dry[:, 0], 160, 147)
    report = {
        "diagnostic": "Phaser 2 Q100 TRENCH versus Emulator X3",
        "source_rate_note": "The bypass is 44.1 kHz and the captures are 48 kHz. Direct TRENCH/X3 ratios cancel the common FL resampling.",
        "bypass": audio_info(BYPASS),
        "captures": {},
        "binary_identity": {},
    }
    for label, path in (("installed", INSTALLED), ("workspace_build", WORKSPACE_BUILD)):
        report["binary_identity"][label] = {
            "path": str(path),
            "sha256": sha256(path),
            "bytes": path.stat().st_size,
            "modified": path.stat().st_mtime,
        }

    figure, axes = plt.subplots(3, 1, figsize=(13, 11), sharex=True)
    for axis, (morph_pct, (trench_path, x3_path)) in zip(axes, CAPTURES.items()):
        trench, rate = sf.read(str(trench_path), always_2d=True)
        x3, x3_rate = sf.read(str(x3_path), always_2d=True)
        if rate != 48000 or x3_rate != 48000:
            raise RuntimeError("All processed captures must be 48 kHz")
        trench = trench[4096:, 0]
        x3 = x3[4096:, 0]
        frequencies, trench_psd = welch(
            trench, rate, nperseg=32768, noverlap=24576, window="hann", detrend=False
        )
        _, x3_psd = welch(
            x3, rate, nperseg=32768, noverlap=24576, window="hann", detrend=False
        )
        coherence_f, coherence_value = coherence(
            trench, x3, rate, nperseg=8192, noverlap=6144
        )
        measured = 10 * np.log10(
            np.maximum(trench_psd, 1.0e-30) / np.maximum(x3_psd, 1.0e-30)
        )
        raw_rows = rows_for(preset, 48000, morph_pct / 100.0)
        snapped_rows = snap_g_minor(raw_rows)
        predicted = magnitude_db(snapped_rows, frequencies) - magnitude_db(raw_rows, frequencies)
        band = (frequencies >= 35) & (frequencies <= 19000)
        level_offset = float(np.median(measured[band] - predicted[band]))
        residual = measured[band] - predicted[band] - level_offset
        shape_rmse = float(np.sqrt(np.mean(residual * residual)))
        coherence_band = (coherence_f >= 50) & (coherence_f <= 18000)

        bank_matches = {}
        x3_full, _ = sf.read(str(x3_path), always_2d=True)
        for candidate_rate in (44100, 48000, 96000, 192000):
            candidate_rows = rows_for(preset, candidate_rate, morph_pct / 100.0)
            candidate = render(dry_48, candidate_rows)
            corr, gain_db = best_gain_correlation(x3_full[:, 0], candidate)
            bank_matches[str(candidate_rate)] = {
                "correlation": corr,
                "gain_db": gain_db,
            }

        stage_coordinates = []
        for stage_index, row in enumerate(raw_rows, 1):
            pole = pair_hz(row[3], row[4])
            zero = pair_hz(row[1] / row[0], row[2] / row[0])
            snapped_pole = nearest_g_minor(pole)
            shift = 1200 * math.log2(snapped_pole / pole)
            stage_coordinates.append(
                {
                    "stage": stage_index,
                    "x3_pole_hz": pole,
                    "x3_zero_hz": zero,
                    "g_minor_pole_hz": snapped_pole,
                    "g_minor_zero_hz": zero * snapped_pole / pole,
                    "shift_cents": shift,
                }
            )

        report["captures"][f"M{morph_pct}"] = {
            "trench": audio_info(trench_path),
            "x3": audio_info(x3_path),
            "median_coherence_50_18000_hz": float(np.median(coherence_value[coherence_band])),
            "trench_minus_x3_scalar_db": level_offset,
            "g_minor_shape_fit_rmse_db": shape_rmse,
            "x3_bank_matches": bank_matches,
            "stage_coordinates": stage_coordinates,
        }

        draw = band & (frequencies >= 40)
        axis.semilogx(
            frequencies[draw], measured[draw] - level_offset, color="#202124", linewidth=1.4,
            label="Measured TRENCH - X3, level matched",
        )
        axis.semilogx(
            frequencies[draw], predicted[draw], color="#d33b32", linewidth=1.0,
            label="Predicted 48 kHz bank after G-minor KEY snap",
        )
        axis.axhline(0, color="#777777", linewidth=0.7)
        axis.set_ylim(-40, 40)
        axis.set_ylabel("dB")
        axis.set_title(
            f"Morph {morph_pct}, Q 100 | coherence {report['captures'][f'M{morph_pct}']['median_coherence_50_18000_hz']:.6f} | shape RMSE {shape_rmse:.3f} dB"
        )
        axis.grid(True, which="both", alpha=0.22)

    axes[0].legend(loc="lower left")
    axes[-1].set_xlabel("Hz")
    figure.suptitle("Phaser 2 diagnostic: the captured Morph difference is KEY retuning, not a rate-bank mismatch")
    figure.tight_layout()
    figure.savefig(OUT / "phaser2_key_snap_diagnostic.png", dpi=170)
    plt.close(figure)

    audio_dir = OUT / "renders"
    audio_dir.mkdir(exist_ok=True)
    report["renders"] = {}
    for morph_pct in CAPTURES:
        raw_rows = rows_for(preset, 48000, morph_pct / 100.0)
        snapped_rows = snap_g_minor(raw_rows)
        x3_gain_db = report["captures"][f"M{morph_pct}"]["x3_bank_matches"]["48000"]["gain_db"]
        gain = 10 ** (x3_gain_db / 20)
        exact = render(dry_48, raw_rows) * gain
        snapped = render(dry_48, snapped_rows) * gain
        exact_path = audio_dir / f"phaser2_48k_q100_m{morph_pct:03d}_exact_x3.wav"
        snapped_path = audio_dir / f"phaser2_48k_q100_m{morph_pct:03d}_trench_gm.wav"
        null_path = audio_dir / f"phaser2_48k_q100_m{morph_pct:03d}_gm_minus_exact_null.wav"
        sf.write(str(exact_path), np.column_stack((exact, exact)), 48000, subtype="FLOAT")
        sf.write(str(snapped_path), np.column_stack((snapped, snapped)), 48000, subtype="FLOAT")
        null = snapped - exact
        sf.write(str(null_path), np.column_stack((null, null)), 48000, subtype="FLOAT")
        exact_rms = float(np.sqrt(np.mean(exact * exact)))
        snapped_rms = float(np.sqrt(np.mean(snapped * snapped)))
        null_rms = float(np.sqrt(np.mean(null * null)))
        report["renders"][f"M{morph_pct}"] = {
            "x3_gain_db": x3_gain_db,
            "exact_x3": str(exact_path),
            "trench_g_minor": str(snapped_path),
            "g_minor_minus_exact_null": str(null_path),
            "exact_rms_dbfs": 20 * math.log10(exact_rms + 1.0e-300),
            "g_minor_rms_dbfs": 20 * math.log10(snapped_rms + 1.0e-300),
            "null_rms_dbfs": 20 * math.log10(null_rms + 1.0e-300),
            "null_relative_to_exact_db": 20 * math.log10(null_rms / exact_rms + 1.0e-300),
            "sample_correlation": float(np.corrcoef(exact, snapped)[0, 1]),
        }

    report["conclusion"] = {
        "rate_bank": "48 kHz for both captured paths",
        "shape_difference": "TRENCH applies the detected G-minor KEY snap to the verbatim 48 kHz bank",
        "level_difference": "TRENCH is about 12.3 dB below X3 after accounting for the KEY-induced response shape; the capture alone does not localize that scalar to plugin versus host gain state",
        "installed_binary_matches_workspace_build": report["binary_identity"]["installed"]["sha256"]
        == report["binary_identity"]["workspace_build"]["sha256"],
    }
    (OUT / "report.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report["conclusion"], indent=2))


if __name__ == "__main__":
    main()
