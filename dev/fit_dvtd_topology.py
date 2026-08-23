"""Fit a DVTD measured vocal-tract transfer function with a prescribed cascade
topology: section 1 is a two-pole resonant lowpass (zeros pinned at Nyquist),
sections 2..7 are parametric bells (pole and zero sharing a centre frequency).

Continuous geometry is a candidate generator only.  Every score that is reported
is computed by the native C++ core through the real packed words.
"""

import argparse
import json
import math
import pathlib
import sys

import numpy as np
from scipy.optimize import least_squares

BUILD = pathlib.Path(__file__).resolve().parents[1] / "out/build/windows-msvc-release/native/research"
sys.path.insert(0, str(BUILD))
import trench_native_research as core  # noqa: E402

DVTD = pathlib.Path(r"C:\Users\hooki\trench-authoring\recipes\vocal\dvtd")
SECTIONS = 7
BELLS = SECTIONS - 1
FIT_LO_HZ = 100.0
FIT_HI_HZ = 8000.0
MAX_CUT_BELLS = 3


def read_vvtf(path):
    freq, mag = [], []
    with open(path) as handle:
        next(handle)
        for line in handle:
            parts = line.split()
            if len(parts) < 2:
                continue
            freq.append(float(parts[0]))
            mag.append(float(parts[1]))
    return np.asarray(freq), 20.0 * np.log10(np.maximum(np.asarray(mag), 1e-9))


def on_grid(freq, db, grid):
    return np.interp(grid, freq, db, left=db[0], right=db[-1])


def smooth_third_octave(grid, db):
    """One-third-octave smoothing, the repository's established target treatment."""
    ratio = 2.0 ** (1.0 / 6.0)
    out = np.empty_like(db)
    for index, hz in enumerate(grid):
        lo = np.searchsorted(grid, hz / ratio)
        hi = np.searchsorted(grid, hz * ratio)
        out[index] = db[lo:max(hi, lo + 1)].mean()
    return out


def pick_extrema(grid, db, count, sign=1.0):
    """Local maxima of sign*db, ranked by prominence.  sign=-1 finds valleys."""
    curve = sign * np.convolve(db, np.ones(5) / 5.0, mode="same")
    band = (grid >= FIT_LO_HZ) & (grid <= FIT_HI_HZ)
    found = []
    for i in range(1, len(grid) - 1):
        if not band[i]:
            continue
        if curve[i] >= curve[i - 1] and curve[i] > curve[i + 1]:
            left = curve[max(0, i - 12):i].min(initial=curve[i])
            prominence = curve[i] - left
            found.append((grid[i], curve[i], prominence))
    found.sort(key=lambda p: -p[2])
    found = found[:count]
    found.sort(key=lambda p: p[0])
    while len(found) < count:
        found.append((FIT_HI_HZ * 0.9, curve[band].mean(), 3.0))
    return found


def pick_peaks(grid, db, count):
    return pick_extrema(grid, db, count, 1.0)


def words_from_root(hz, radius, sample_rate_hz):
    theta = 2.0 * math.pi * hz / sample_rate_hz
    p = -2.0 * radius * math.cos(theta)
    q = radius * radius
    d_mag = (p + q + 1.0) / 4.0
    d_rsq = 1.0 - q
    return core.encode_word(min(max(d_mag, 0.0), 1.0)), core.encode_word(min(max(d_rsq, 0.0), 1.0))


def biquad(hz, radius, sample_rate_hz):
    theta = 2.0 * math.pi * hz / sample_rate_hz
    return -2.0 * radius * math.cos(theta), radius * radius


WARP_MAX = 54.0
POLE_WARP_MAX = 46.0


def radius_of_warp(warp):
    return 1.0 - 10.0 ** (-warp / 20.0)


def warp_of_radius(radius):
    return min(-20.0 * math.log10(max(1.0 - radius, 1e-9)), WARP_MAX)


def bounds(sample_rate_hz):
    lo_hz, hi_hz = math.log(60.0), math.log(0.46 * sample_rate_hz)
    low = [lo_hz, 0.0]
    high = [hi_hz, POLE_WARP_MAX]
    for _ in range(BELLS):
        low += [lo_hz, 0.0, 0.0]
        high += [hi_hz, POLE_WARP_MAX, WARP_MAX]
    return np.asarray(low), np.asarray(high)


def unpack(x, sample_rate_hz):
    """Free vector -> (lowpass pole, [(hz, r_pole, r_zero)] * 6)."""
    lp_hz = float(math.exp(x[0]))
    lp_r = radius_of_warp(x[1])
    bells = []
    for i in range(BELLS):
        base = 2 + 3 * i
        bells.append((float(math.exp(x[base])), radius_of_warp(x[base + 1]),
                      radius_of_warp(x[base + 2])))
    return (lp_hz, lp_r), bells, 0.5 * sample_rate_hz


def model_db(x, grid, sample_rate_hz):
    """Continuous response of the prescribed topology, unquantised."""
    (lp_hz, lp_r), bells, _ = unpack(x, sample_rate_hz)
    w = 2.0 * math.pi * grid / sample_rate_hz
    z1 = np.exp(-1j * w)
    z2 = z1 * z1
    total = np.zeros_like(grid)

    a1, a2 = biquad(lp_hz, lp_r, sample_rate_hz)
    num = 1.0 + 2.0 * z1 + 1.0 * z2
    den = 1.0 + a1 * z1 + a2 * z2
    total += 20.0 * np.log10(np.maximum(np.abs(num / den), 1e-30))

    for hz, r_p, r_z in bells:
        b1, b2 = biquad(hz, r_z, sample_rate_hz)
        a1, a2 = biquad(hz, r_p, sample_rate_hz)
        num = 1.0 + b1 * z1 + b2 * z2
        den = 1.0 + a1 * z1 + a2 * z2
        total += 20.0 * np.log10(np.maximum(np.abs(num / den), 1e-30))
    return total


def pack_words(x, sample_rate_hz, target_offset_db):
    (lp_hz, lp_r), bells, _ = unpack(x, sample_rate_hz)
    rows = []
    pole_mag, pole_rsq = words_from_root(lp_hz, lp_r, sample_rate_hz)
    rows.append([0xFFFF, 0x0000, pole_mag, pole_rsq, 0])
    for hz, r_p, r_z in bells:
        zero_mag, zero_rsq = words_from_root(hz, r_z, sample_rate_hz)
        pole_mag, pole_rsq = words_from_root(hz, r_p, sample_rate_hz)
        rows.append([zero_mag, zero_rsq, pole_mag, pole_rsq, 0])

    per_section = 10.0 ** (target_offset_db / 20.0 / SECTIONS)
    gain_word = core.encode_word(min(max(per_section / 4.0, 0.0), 1.0))
    for row in rows:
        row[4] = gain_word
    return [w for row in rows for w in row]


def weighted_stats(resid, weight):
    mean = float(np.sum(weight * resid) / np.sum(weight))
    centred = resid - mean
    rms = math.sqrt(float(np.sum(weight * centred * centred) / np.sum(weight)))
    worst = float(np.max(np.abs(centred[weight > 0.0]))) if np.any(weight > 0.0) else 0.0
    return mean, rms, worst


def polish_words(words, target, grid, weight, sample_rate_hz, rounds=3):
    """Integer coordinate descent on the packed words, scored by the real core."""
    words = list(words)

    def score(candidate):
        got = np.asarray(core.cascade_db(candidate, list(grid), sample_rate_hz))
        _, rms, _ = weighted_stats(target - got, weight)
        return rms

    best = score(words)
    for _ in range(rounds):
        improved = False
        for index in range(len(words)):
            if index % 5 == 4:
                continue
            for step in (256, 64, 16, 4, 1, -1, -4, -16, -64, -256):
                candidate = list(words)
                value = candidate[index] + step
                if not 0 <= value <= 0xFFFF:
                    continue
                candidate[index] = value
                trial = score(candidate)
                if trial < best - 1e-9:
                    words, best, improved = candidate, trial, True
                    break
        if not improved:
            break
    return words, best


def fit_mouth(path, sample_rate_hz, verbose=True):
    grid = np.asarray(core.erb_grid_hz())
    weight = np.asarray(core.erb_grid_weight())
    band = (grid >= FIT_LO_HZ) & (grid <= FIT_HI_HZ)
    weight = np.where(band, weight, 0.0)

    freq, db = read_vvtf(path)
    target = smooth_third_octave(grid, on_grid(freq, db, grid))

    lo, hi = bounds(sample_rate_hz)
    sqrt_w = np.sqrt(weight)

    def residual(x):
        got = model_db(x, grid, sample_rate_hz)
        delta = target - got
        delta = delta - np.sum(weight * delta) / np.sum(weight)
        return sqrt_w * delta

    best_solution, best_cost, best_split = None, math.inf, None
    for cuts in range(0, MAX_CUT_BELLS + 1):
        peaks = pick_extrema(grid, target, BELLS - cuts, 1.0)
        valleys = pick_extrema(grid, target, cuts, -1.0) if cuts else []
        for pole_warp in (18.0, 30.0):
            anchor = peaks[-1][0] if peaks else FIT_HI_HZ * 0.5
            seed = [math.log(min(max(anchor * 1.5, 1200.0), 0.4 * sample_rate_hz)), 8.0]
            for hz, _, prominence in peaks:
                boost = min(max(prominence, 1.0), 26.0)
                seed += [math.log(hz), pole_warp, max(pole_warp - boost, 0.0)]
            for hz, _, depth in valleys:
                # a cut is the same bell inverted: the zero sits nearer the circle
                notch = min(max(depth, 2.0), 30.0)
                pole = max(pole_warp - notch, 0.0)
                seed += [math.log(hz), pole, min(pole + notch, WARP_MAX)]
            seed = np.clip(np.asarray(seed), lo + 1e-6, hi - 1e-6)
            try:
                candidate = least_squares(residual, seed, bounds=(lo, hi), method="trf",
                                          x_scale="jac", max_nfev=4000)
            except ValueError:
                continue
            if candidate.cost < best_cost:
                best_solution, best_cost, best_split = candidate, candidate.cost, cuts
    solution = best_solution

    continuous = model_db(solution.x, grid, sample_rate_hz)
    offset, cont_rms, _ = weighted_stats(target - continuous, weight)

    words = pack_words(solution.x, sample_rate_hz, offset)
    packed = np.asarray(core.cascade_db(words, list(grid), sample_rate_hz))
    _, packed_rms, packed_max = weighted_stats(target - packed, weight)

    words, polished_rms = polish_words(words, target, grid, weight, sample_rate_hz)
    polished = np.asarray(core.cascade_db(words, list(grid), sample_rate_hz))
    _, _, polished_max = weighted_stats(target - polished, weight)

    if verbose:
        print(f"{path.parent.name:28s} continuous {cont_rms:6.3f}  "
              f"packed {packed_rms:6.3f}  polished {polished_rms:6.3f} dB rms "
              f"(worst {polished_max:5.2f})  cuts seeded {best_split}")
    return {
        "mouth": path.parent.name,
        "cut_bells_seeded": best_split,
        "continuous_rms_db": cont_rms,
        "packed_rms_db": packed_rms,
        "polished_rms_db": polished_rms,
        "polished_max_db": polished_max,
        "words": words,
        "target": target.tolist(),
        "response": polished.tolist(),
    }


def main():
    global SECTIONS, BELLS
    parser = argparse.ArgumentParser()
    parser.add_argument("--mouth", default="s1-01-bahn-tense-a")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--sample-rate", type=float, default=44100.0)
    parser.add_argument("--out", default=None)
    parser.add_argument("--sections", type=int, default=SECTIONS)
    args = parser.parse_args()
    SECTIONS = args.sections
    BELLS = SECTIONS - 1

    paths = sorted(DVTD.glob("subject-*/*/*-vvtf-measured.txt"))
    if not args.all:
        paths = [p for p in paths if p.parent.name == args.mouth]
    if not paths:
        raise SystemExit("no DVTD curves matched")

    results = [fit_mouth(path, args.sample_rate) for path in paths]
    if args.out:
        pathlib.Path(args.out).write_text(json.dumps(results, indent=1))
        print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
