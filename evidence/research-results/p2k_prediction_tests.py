"""Corpus tests for proposed P2K four-corner construction rules.

Authority: the 33 exact 240-byte files in evidence/factory-data/p2k/bodies.
Corner order is C0=M0/Q0, C1=M100/Q0, C2=M0/Q100, C3=M100/Q100.
"""

from __future__ import annotations

import csv
import itertools
import json
import math
import struct
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[2]
BODIES = ROOT / "evidence" / "factory-data" / "p2k" / "bodies"
OUT = ROOT / "evidence" / "research-results" / "p2k_prediction_tests"
FS = 39062.5
EPS = 1.0e-15


def decode_word(word: int) -> float:
    u = word + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    exponent = (u >> 12) & 0xF
    mantissa = u & 0xFFF
    x = mantissa / 4096.0 if exponent == 0 else (mantissa + 4096.0) / 8192.0
    return math.ldexp(x, exponent - 15)


def pair_geometry(magnitude_word: int, radius_word: int) -> dict:
    magnitude = decode_word(magnitude_word)
    encoded_radius_squared = decode_word(radius_word)
    radius_squared = 1.0 - encoded_radius_squared
    p = 4.0 * magnitude + encoded_radius_squared - 2.0
    discriminant = p * p - 4.0 * radius_squared
    if abs(p) < EPS and abs(radius_squared) < EPS:
        return {"kind": "degenerate", "hz": None, "radius": None}
    if discriminant < 0.0 and radius_squared > 0.0:
        radius = math.sqrt(radius_squared)
        cosine = float(np.clip(-p / (2.0 * radius), -1.0, 1.0))
        hz = math.acos(cosine) / (2.0 * math.pi) * FS
        return {"kind": "conjugate", "hz": hz, "radius": radius}
    root = math.sqrt(max(discriminant, 0.0))
    return {
        "kind": "real",
        "hz": None,
        "radius": None,
        "root_a": (-p + root) / 2.0,
        "root_b": (-p - root) / 2.0,
    }


def read_body(path: Path) -> list[list[dict]]:
    words = struct.unpack("<120H", path.read_bytes())
    corners = []
    for corner in range(4):
        sections = []
        for section in range(6):
            base = (corner * 6 + section) * 5
            raw = words[base : base + 5]
            sections.append(
                {
                    "words": raw,
                    "zero": pair_geometry(raw[0], raw[1]),
                    "pole": pair_geometry(raw[2], raw[3]),
                    "scale": 4.0 * decode_word(raw[4]),
                }
            )
        corners.append(sections)
    return corners


def log2_shift(a: dict, b: dict) -> float | None:
    if a["kind"] != "conjugate" or b["kind"] != "conjugate":
        return None
    if a["hz"] <= 0.0 or b["hz"] <= 0.0:
        return None
    return math.log2(b["hz"] / a["hz"])


def delta_log_radius(a: dict, b: dict) -> float | None:
    if a["kind"] != "conjugate" or b["kind"] != "conjugate":
        return None
    if a["radius"] <= 0.0 or b["radius"] <= 0.0:
        return None
    return math.log(b["radius"]) - math.log(a["radius"])


def best_frequency_permutation(source: list[dict], target: list[dict]) -> tuple[tuple[int, ...], float] | None:
    if any(s["pole"]["kind"] != "conjugate" for s in source + target):
        return None
    a = np.log([s["pole"]["hz"] for s in source])
    b = np.log([s["pole"]["hz"] for s in target])
    best_perm = None
    best_cost = math.inf
    for perm in itertools.permutations(range(6)):
        cost = sum(abs(b[i] - a[perm[i]]) for i in range(6))
        if cost < best_cost:
            best_cost = cost
            best_perm = perm
    return best_perm, best_cost


def section_coefficients(section: dict) -> tuple[float, float, float, float, float]:
    w = section["words"]
    d0, d1, d2, d3, d4 = (decode_word(x) for x in w)
    c0, c1, c2, c3, c4 = 4.0 * d0 + d1, d1, 4.0 * d2 + d3, d3, 4.0 * d4
    return c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3


def cascade_level_db(sections: list[dict], mode: str) -> float:
    if mode == "dc":
        omega = np.array([0.0])
    elif mode == "white_noise_rms":
        omega = np.linspace(0.0, math.pi, 8193)
    else:
        raise ValueError(mode)
    z1 = np.exp(-1j * omega)
    z2 = z1 * z1
    response = np.ones_like(z1, dtype=np.complex128)
    for section in sections:
        b0, b1, b2, a1, a2 = section_coefficients(section)
        response *= (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)
    if mode == "dc":
        amplitude = abs(response[0])
    else:
        amplitude = math.sqrt(float(np.mean(np.abs(response) ** 2)))
    return 20.0 * math.log10(max(amplitude, 1.0e-300))


def pearson(x: list[float], y: list[float]) -> float:
    if len(x) < 2 or np.std(x) == 0.0 or np.std(y) == 0.0:
        return math.nan
    return float(np.corrcoef(x, y)[0, 1])


def sign(value: float, tolerance: float = 1.0e-12) -> int:
    return 1 if value > tolerance else -1 if value < -tolerance else 0


def main() -> None:
    paths = sorted(BODIES.glob("P2k_0*.bin"))
    if len(paths) != 33 or any(p.stat().st_size != 240 for p in paths):
        raise RuntimeError("expected exactly 33 factory P2K bodies of 240 bytes")

    bodies = [(p.stem, read_body(p)) for p in paths]
    raw_rows = []

    # Prediction 1: minimum-total-log-frequency bijection, C2 targets to C0 sources.
    p1_bodies = []
    identity_hits = 0
    eligible_sections = 0
    independent_hits = 0
    independent_total = 0
    tie_inclusive_hits = 0
    identity_permutation_bodies = 0
    for name, corners in bodies:
        result = best_frequency_permutation(corners[0], corners[2])
        if result is None:
            p1_bodies.append({"body": name, "eligible": False})
            continue
        perm, cost = result
        hits = sum(i == perm[i] for i in range(6))
        identity_permutation_bodies += hits == 6
        identity_hits += hits
        eligible_sections += 6
        src_log = np.log([s["pole"]["hz"] for s in corners[0]])
        independent = [int(np.argmin(np.abs(src_log - math.log(s["pole"]["hz"])))) for s in corners[2]]
        ihits = sum(i == independent[i] for i in range(6))
        tie_hits = int(sum(
            abs(src_log[i] - math.log(corners[2][i]["pole"]["hz"]))
            <= np.min(np.abs(src_log - math.log(corners[2][i]["pole"]["hz"]))) + 1.0e-12
            for i in range(6)
        ))
        independent_hits += ihits
        independent_total += 6
        tie_inclusive_hits += tie_hits
        p1_bodies.append(
            {"body": name, "eligible": True, "permutation_c2_to_c0": list(perm), "identity_hits": hits, "cost_log": cost,
             "independent_nearest": independent, "independent_identity_hits": ihits,
             "own_slot_is_tied_or_unique_nearest_hits": tie_hits}
        )

    rng = np.random.default_rng(20260902)
    trials = 100_000
    random_rates = np.empty(trials)
    eligible_body_count = eligible_sections // 6
    for trial in range(trials):
        hits = 0
        for _ in range(eligible_body_count):
            perm = rng.permutation(6)
            hits += int(np.sum(perm == np.arange(6)))
        random_rates[trial] = hits / max(eligible_sections, 1)

    # Prediction 2: first five physical sections, C2-C0.
    p2_bodies = []
    within_ss = 0.0
    within_n = 0
    body_means = []
    p2_valid_stage_count = 0
    for name, corners in bodies:
        deltas = [delta_log_radius(corners[0][s]["pole"], corners[2][s]["pole"]) for s in range(5)]
        valid = [d for d in deltas if d is not None]
        p2_valid_stage_count += len(valid)
        signs = [sign(d) if d is not None else None for d in deltas]
        counts = {str(k): signs.count(k) for k in (-1, 0, 1)}
        dominant_sign = max((-1, 0, 1), key=lambda k: counts[str(k)])
        consistent = counts[str(dominant_sign)] >= 4
        mean = float(np.mean(valid)) if valid else math.nan
        if valid:
            within_ss += sum((d - mean) ** 2 for d in valid)
            within_n += len(valid)
            body_means.append(mean)
        p2_bodies.append({"body": name, "delta_log_r_s1_s5": deltas, "signs": signs, "dominant_sign": dominant_sign,
                          "dominant_count": counts[str(dominant_sign)], "at_least_4_of_5": consistent, "mean": mean})

    within_variance = within_ss / within_n
    between_variance = float(np.var(body_means))

    # Prediction 3: horizontal C1-C0, all six sections. Regression uses paired live conjugate zeros/poles.
    p3_bodies = []
    pole_shifts = []
    zero_shifts = []
    for name, corners in bodies:
        shifts = [log2_shift(corners[0][s]["pole"], corners[1][s]["pole"]) for s in range(6)]
        shift_signs = [sign(d) if d is not None else None for d in shifts]
        nonzero = [v for v in shift_signs if v not in (None, 0)]
        valid_signs = [v for v in shift_signs if v is not None]
        unanimous_nonzero = len(nonzero) >= 4 and len(set(nonzero)) == 1
        dominant_nonzero = max(nonzero.count(-1), nonzero.count(1)) if nonzero else 0
        dominant_exact = max(valid_signs.count(-1), valid_signs.count(0), valid_signs.count(1)) if valid_signs else 0
        for s in range(6):
            pd = shifts[s]
            zd = log2_shift(corners[0][s]["zero"], corners[1][s]["zero"])
            if pd is not None and zd is not None:
                pole_shifts.append(pd)
                zero_shifts.append(zd)
        p3_bodies.append({"body": name, "pole_log2_hz_shifts": shifts, "signs": shift_signs,
                          "unanimous_nonzero_direction": unanimous_nonzero,
                          "at_least_5_of_6_nonzero_direction": dominant_nonzero >= 5,
                          "at_least_5_of_6_exact_sign": dominant_exact >= 5})

    x = np.asarray(pole_shifts)
    y = np.asarray(zero_shifts)
    slope, intercept = np.polyfit(x, y, 1)
    through_origin_slope = float(np.dot(x, y) / np.dot(x, x))

    # Prediction 5: preserve raw words; compare decoded DC and white-noise RMS levels.
    p5_bodies = []
    raw_gain_deltas = []
    decoded_log_gain_deltas = []
    radius_deltas = []
    for name, corners in bodies:
        dc = [cascade_level_db(corner, "dc") for corner in corners]
        white = [cascade_level_db(corner, "white_noise_rms") for corner in corners]
        raw_gain_words = [[int(section["words"][4]) for section in corner] for corner in corners]
        p5_bodies.append({"body": name, "raw_gain_words": raw_gain_words, "dc_level_db": dc,
                          "dc_range_db": max(dc) - min(dc), "white_noise_rms_db": white,
                          "white_noise_rms_range_db": max(white) - min(white)})
        for low_corner, high_corner in ((0, 2), (1, 3)):
            for s in range(6):
                rd = delta_log_radius(corners[low_corner][s]["pole"], corners[high_corner][s]["pole"])
                if rd is None:
                    continue
                low_word = int(corners[low_corner][s]["words"][4])
                high_word = int(corners[high_corner][s]["words"][4])
                raw_gain_deltas.append(high_word - low_word)
                decoded_log_gain_deltas.append(math.log(corners[high_corner][s]["scale"] / corners[low_corner][s]["scale"]))
                radius_deltas.append(rd)
                raw_rows.append({"body": name, "low_corner": low_corner, "high_corner": high_corner, "section": s + 1,
                                 "gain_word_low": low_word, "gain_word_high": high_word,
                                 "delta_gain_word": high_word - low_word,
                                 "delta_log_gain": decoded_log_gain_deltas[-1], "delta_log_pole_radius": rd})

    result = {
        "corpus": {"body_count": len(bodies), "corners_per_body": 4, "sections_per_corner": 6,
                   "sample_rate_hz": FS, "corner_order": ["C0=M0/Q0", "C1=M100/Q0", "C2=M0/Q100", "C3=M100/Q100"]},
        "prediction_1": {
            "method": "minimum-total-absolute-log-frequency bijection from C2 sections to C0 sections",
            "eligible_bodies": eligible_body_count, "identity_hits": identity_hits, "eligible_sections": eligible_sections,
            "identity_rate": identity_hits / max(eligible_sections, 1),
            "identity_permutation_bodies": identity_permutation_bodies,
            "independent_nearest_identity_rate": independent_hits / max(independent_total, 1),
            "tie_inclusive_own_slot_nearest_rate": tie_inclusive_hits / max(independent_total, 1),
            "random_permutation_expected_rate": 1.0 / 6.0,
            "random_permutation_mc_mean": float(np.mean(random_rates)),
            "random_permutation_mc_95_interval": [float(np.quantile(random_rates, 0.025)), float(np.quantile(random_rates, 0.975))],
            "bodies": p1_bodies,
        },
        "prediction_2": {
            "method": "C2-C0 delta natural-log pole radius, physical sections S1-S5",
            "valid_conjugate_stage_pairs": p2_valid_stage_count,
            "bodies_at_least_4_of_5_same_sign": sum(b["at_least_4_of_5"] for b in p2_bodies),
            "positive_dominant_bodies_at_least_4_of_5": sum(b["at_least_4_of_5"] and b["dominant_sign"] == 1 for b in p2_bodies),
            "negative_dominant_bodies_at_least_4_of_5": sum(b["at_least_4_of_5"] and b["dominant_sign"] == -1 for b in p2_bodies),
            "zero_dominant_bodies_at_least_4_of_5": sum(b["at_least_4_of_5"] and b["dominant_sign"] == 0 for b in p2_bodies),
            "within_body_variance": within_variance, "between_body_variance_of_means": between_variance,
            "between_to_within_ratio": between_variance / within_variance if within_variance else math.inf,
            "bodies": p2_bodies,
        },
        "prediction_3": {
            "method": "C1-C0 log2-frequency shifts; pooled same-section live-conjugate zero/pole pairs",
            "bodies_unanimous_nonzero_pole_direction": sum(b["unanimous_nonzero_direction"] for b in p3_bodies),
            "bodies_at_least_5_of_6_nonzero_pole_direction": sum(b["at_least_5_of_6_nonzero_direction"] for b in p3_bodies),
            "bodies_at_least_5_of_6_exact_sign": sum(b["at_least_5_of_6_exact_sign"] for b in p3_bodies),
            "regression_pair_count": len(x), "ols_slope": float(slope), "ols_intercept_octaves": float(intercept),
            "through_origin_slope": through_origin_slope, "pearson_r": pearson(pole_shifts, zero_shifts),
            "bodies": p3_bodies,
        },
        "prediction_5": {
            "method": "raw fifth words retained; decoded cascade DC and linear-frequency white-noise RMS levels",
            "bodies_dc_corner_range_le_1db": sum(b["dc_range_db"] <= 1.0 for b in p5_bodies),
            "bodies_white_noise_corner_range_le_1db": sum(b["white_noise_rms_range_db"] <= 1.0 for b in p5_bodies),
            "median_dc_corner_range_db": float(np.median([b["dc_range_db"] for b in p5_bodies])),
            "max_dc_corner_range_db": max(b["dc_range_db"] for b in p5_bodies),
            "median_white_noise_corner_range_db": float(np.median([b["white_noise_rms_range_db"] for b in p5_bodies])),
            "gain_word_delta_vs_delta_log_radius_pearson_r": pearson(raw_gain_deltas, radius_deltas),
            "decoded_log_gain_delta_vs_delta_log_radius_pearson_r": pearson(decoded_log_gain_deltas, radius_deltas),
            "positive_radius_pair_count": sum(d > 0.0 for d in radius_deltas),
            "gain_word_delta_vs_positive_delta_log_radius_pearson_r": pearson(
                [g for g, d in zip(raw_gain_deltas, radius_deltas) if d > 0.0],
                [d for d in radius_deltas if d > 0.0],
            ),
            "decoded_log_gain_delta_vs_positive_delta_log_radius_pearson_r": pearson(
                [g for g, d in zip(decoded_log_gain_deltas, radius_deltas) if d > 0.0],
                [d for d in radius_deltas if d > 0.0],
            ),
            "pair_count": len(radius_deltas), "bodies": p5_bodies,
        },
    }

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    with OUT.with_suffix(".csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(raw_rows[0]))
        writer.writeheader()
        writer.writerows(raw_rows)

    p1 = result["prediction_1"]
    p2 = result["prediction_2"]
    p3 = result["prediction_3"]
    p5 = result["prediction_5"]
    report = f"""# P2K prediction tests

Corpus: {len(bodies)} exact factory `.bin` bodies, 4 corners, 6 sections, 5 little-endian u16 words per section. Decode datum: {FS:g} Hz. Corner order: C0=M0/Q0, C1=M100/Q0, C2=M0/Q100, C3=M100/Q100.

## Prediction 1 — slot identity across rows

- Minimum-cost log-frequency bijection is identity for {p1['identity_hits']}/{p1['eligible_sections']} sections ({100*p1['identity_rate']:.1f}%) across {p1['eligible_bodies']} eligible bodies.
- The entire optimal permutation is identity in {p1['identity_permutation_bodies']}/{p1['eligible_bodies']} eligible bodies.
- Independent nearest-neighbour identity rate: {100*p1['independent_nearest_identity_rate']:.1f}%; allowing exact-frequency ties, the own slot is a nearest match for {100*p1['tie_inclusive_own_slot_nearest_rate']:.1f}%.
- Random-permutation baseline: expected {100*p1['random_permutation_expected_rate']:.1f}%; 100,000-trial Monte Carlo mean {100*p1['random_permutation_mc_mean']:.1f}%, 95% interval {100*p1['random_permutation_mc_95_interval'][0]:.1f}–{100*p1['random_permutation_mc_95_interval'][1]:.1f}%.
- Threshold tested: >=80%.

## Prediction 2 — sharpening as a body-level operation

- {p2['bodies_at_least_4_of_5_same_sign']}/{len(bodies)} bodies have >=4/5 S1–S5 `delta ln(r)` values with the same exact sign: {p2['positive_dominant_bodies_at_least_4_of_5']} positive, {p2['negative_dominant_bodies_at_least_4_of_5']} negative, {p2['zero_dominant_bodies_at_least_4_of_5']} zero-dominant. There are {p2['valid_conjugate_stage_pairs']}/{len(bodies)*5} valid conjugate stage pairs.
- Pooled within-body variance: {p2['within_body_variance']:.8g}.
- Between-body variance of body means: {p2['between_body_variance_of_means']:.8g} (between/within={p2['between_to_within_ratio']:.3f}).

## Prediction 3 — coherent frame translation

- C1-C0 pole motion is unanimous among at least four nonzero live shifts in {p3['bodies_unanimous_nonzero_pole_direction']}/{len(bodies)} bodies; >=5/6 same nonzero direction in {p3['bodies_at_least_5_of_6_nonzero_pole_direction']}/{len(bodies)}, and >=5/6 the same exact sign (including zero) in {p3['bodies_at_least_5_of_6_exact_sign']}/{len(bodies)}.
- Same-section zero-shift on pole-shift regression ({p3['regression_pair_count']} live conjugate pairs): slope={p3['ols_slope']:.4f}, intercept={p3['ols_intercept_octaves']:.4f} octaves, Pearson r={p3['pearson_r']:.4f}. Through-origin slope={p3['through_origin_slope']:.4f}.
- Prediction tested: slope about 0.6 and r>0.5.

## Prediction 5 — gain-word level compensation

- Raw fifth words for every corner/section are retained in the JSON. DC corner range is <=1 dB in {p5['bodies_dc_corner_range_le_1db']}/{len(bodies)} bodies; median range={p5['median_dc_corner_range_db']:.3f} dB, max={p5['max_dc_corner_range_db']:.3f} dB.
- As a spectral-level sensitivity check, linear-frequency white-noise RMS range is <=1 dB in {p5['bodies_white_noise_corner_range_le_1db']}/{len(bodies)} bodies; median range={p5['median_white_noise_corner_range_db']:.3f} dB.
- Across both row transitions C2-C0 and C3-C1 ({p5['pair_count']} section pairs), raw gain-word delta vs `delta ln(r)` Pearson r={p5['gain_word_delta_vs_delta_log_radius_pearson_r']:.4f}; decoded `delta ln(scale)` vs `delta ln(r)` r={p5['decoded_log_gain_delta_vs_delta_log_radius_pearson_r']:.4f}.
- Restricting to the {p5['positive_radius_pair_count']} actual radius increases, those correlations are {p5['gain_word_delta_vs_positive_delta_log_radius_pearson_r']:.4f} raw and {p5['decoded_log_gain_delta_vs_positive_delta_log_radius_pearson_r']:.4f} decoded.
"""
    OUT.with_suffix(".md").write_text(report, encoding="utf-8")
    print(report)


if __name__ == "__main__":
    main()
