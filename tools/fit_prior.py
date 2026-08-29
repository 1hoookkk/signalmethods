import glob
import json
import os

import h5py
import numpy as np
from scipy.signal import find_peaks

FS = 44100.0
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_HPP = os.path.join(REPO, "native", "app", "fitted_shelf.hpp")
OUT_TXT = os.path.join(REPO, "tools", "fit_prior_report.txt")

DVTD = "C:/Users/hooki/trench-authoring/recipes/vocal/dvtd"
TFS = "C:/Users/hooki/trench-x3-clean/recipes/tfs"
SOFA = "C:/Users/hooki/trench-authoring/recipes/hrtf/oldenburg_mmhr/KEMAR_s.sofa"

ABSENT = (22050.0, 1000000000.0, False)

STEP = 1.0 / 24.0
N_GRID = int(round(np.log2(20000.0 / 20.0) / STEP)) + 1
GRID = 20.0 * 2.0 ** (np.arange(N_GRID) * STEP)
W = 2.0 * np.pi * GRID / FS

BAND_SHELF = (GRID >= 40.0) & (GRID <= 16000.0)
BAND_FIT = (GRID >= 100.0) & (GRID <= 10000.0)


def denom_db(hz, bw):
    r = np.exp(-np.pi * bw / FS)
    if r >= 1.0:
        r = 1.0 - 1e-12
    theta = 2.0 * np.pi * hz / FS
    z = np.exp(-1j * W)
    d = 1.0 - 2.0 * r * np.cos(theta) * z + (r * r) * z * z
    return 20.0 * np.log10(np.abs(d) + 1e-300)


def section_db(pole, zero):
    if not pole[2] and not zero[2]:
        return np.zeros_like(GRID)
    out = np.zeros_like(GRID)
    if zero[2]:
        out = out + denom_db(zero[0], zero[1])
    if pole[2]:
        out = out - denom_db(pole[0], pole[1])
    return out


def cascade_db(poles, zeros):
    out = np.zeros_like(GRID)
    for p, z in zip(poles, zeros):
        out = out + section_db(p, z)
    return out


def demeaned_rms(diff, mask):
    d = diff[mask]
    d = d - d.mean()
    return float(np.sqrt(np.mean(d * d)))


def smooth_octave(curve, fraction):
    width = fraction / STEP
    half = max(1, int(round(width / 2.0)))
    kernel = np.ones(2 * half + 1) / (2 * half + 1)
    padded = np.concatenate([np.full(half, curve[0]), curve, np.full(half, curve[-1])])
    return np.convolve(padded, kernel, mode="valid")


def fit_shelf(target):
    broad = smooth_octave(target, 1.0)
    fp_grid = np.geomspace(2000.0, 16000.0, 12)
    fz_grid = np.geomspace(150.0, 1500.0, 12)
    f6_grid = np.geomspace(80.0, 400.0, 12)
    s6_zero = (20000.0, 0.03, True)
    s6_zero_db = denom_db(s6_zero[0], s6_zero[1])
    pole1_db = [denom_db(f, 0.5 * f) for f in fp_grid]
    zero1_db = [denom_db(f, 0.5 * f) for f in fz_grid]
    pole6_db = [denom_db(f, 0.5 * f) for f in f6_grid]
    best = None
    best_err = None
    for i, fp in enumerate(fp_grid):
        for j, fz in enumerate(fz_grid):
            base = zero1_db[j] - pole1_db[i] + s6_zero_db
            for k, f6 in enumerate(f6_grid):
                shelf = base - pole6_db[k]
                err = demeaned_rms(broad - shelf, BAND_SHELF)
                if best_err is None or err < best_err:
                    best_err = err
                    best = (float(fp), float(fz), float(f6))
    fp, fz, f6 = best
    s1_pole = (fp, 0.5 * fp, True)
    s1_zero = (fz, 0.5 * fz, True)
    s6_pole = (f6, 0.5 * f6, True)
    return s1_pole, s1_zero, s6_pole, s6_zero, best_err


def walk_width(curve, index, sign):
    thr = curve[index] - 3.0 * sign
    dist = []
    for step in (-1, 1):
        j = index
        hit = None
        turn = None
        while True:
            nxt = j + step
            if nxt < 0 or nxt >= len(curve):
                turn = j
                break
            if sign > 0:
                if curve[nxt] <= thr:
                    hit = nxt
                    break
                if curve[nxt] > curve[j]:
                    turn = j
                    break
            else:
                if curve[nxt] >= thr:
                    hit = nxt
                    break
                if curve[nxt] < curve[j]:
                    turn = j
                    break
            j = nxt
        if hit is not None:
            dist.append(abs(GRID[hit] - GRID[index]))
        else:
            dist.append(2.0 * abs(GRID[turn] - GRID[index]))
    return dist[0] + dist[1]


def clamp_bw(bw):
    return float(min(20000.0, max(1.0, bw)))


def pick_features(residual):
    curve = smooth_octave(residual, 1.0 / 12.0)
    found = []
    hi, hp = find_peaks(curve, prominence=2.0)
    for k, idx in enumerate(hi):
        found.append((float(hp["prominences"][k]), int(idx), 1))
    lo, lp = find_peaks(-curve, prominence=2.0)
    for k, idx in enumerate(lo):
        found.append((float(lp["prominences"][k]), int(idx), -1))
    found.sort(key=lambda item: -item[0])
    chosen = found[:4]
    chosen.sort(key=lambda item: GRID[item[1]])
    bands = []
    for height, idx, sign in chosen:
        f = float(GRID[idx])
        b = clamp_bw(walk_width(curve, idx, sign))
        scale = 10.0 ** (height / 20.0)
        if sign > 0:
            pole = (f, b, True)
            zero = (f, clamp_bw(b * scale), True)
        else:
            pole = (f, clamp_bw(b * scale), True)
            zero = (f, b, True)
        bands.append((pole, zero, sign))
    return bands


def refine(bands, s1_pole, s1_zero, s6_pole, s6_zero, target):
    poles = [s1_pole] + [b[0] for b in bands] + [ABSENT] * (4 - len(bands)) + [s6_pole]
    zeros = [s1_zero] + [b[1] for b in bands] + [ABSENT] * (4 - len(bands)) + [s6_zero]
    factors = np.geomspace(0.5, 2.0, 9)
    for i, band in enumerate(bands):
        slot = i + 1
        sign = band[2]
        base = zeros[slot][1] if sign > 0 else poles[slot][1]
        best_err = None
        best_bw = base
        for factor in factors:
            bw = clamp_bw(base * factor)
            if sign > 0:
                zeros[slot] = (zeros[slot][0], bw, True)
            else:
                poles[slot] = (poles[slot][0], bw, True)
            err = demeaned_rms(target - cascade_db(poles, zeros), BAND_FIT)
            if best_err is None or err < best_err:
                best_err = err
                best_bw = bw
        if sign > 0:
            zeros[slot] = (zeros[slot][0], best_bw, True)
        else:
            poles[slot] = (poles[slot][0], best_bw, True)
    return poles, zeros


def fit_target(target):
    s1_pole, s1_zero, s6_pole, s6_zero, shelf_err = fit_shelf(target)
    shelf = section_db(s1_pole, s1_zero) + section_db(s6_pole, s6_zero)
    bands = pick_features(target - shelf)
    poles, zeros = refine(bands, s1_pole, s1_zero, s6_pole, s6_zero, target)
    err = demeaned_rms(target - cascade_db(poles, zeros), BAND_FIT)
    return poles, zeros, len(bands), err, shelf_err


def to_grid(freqs, db):
    keep = freqs > 0
    freqs = freqs[keep]
    db = db[keep]
    order = np.argsort(freqs)
    return np.interp(np.log10(GRID), np.log10(freqs[order]), db[order])


def load_mouths():
    out = []
    for subject in (1, 2):
        prefix = "s%d-" % subject
        group = "MOUTHS S%d" % subject
        pattern = os.path.join(DVTD, "subject-%d" % subject, "*", "*-vvtf-measured.txt")
        for path in sorted(glob.glob(pattern)):
            folder = os.path.basename(os.path.dirname(path))
            name = folder[len(prefix) + 3:].replace("-", " ")
            data = np.loadtxt(path, skiprows=1)
            db = 20.0 * np.log10(np.maximum(data[:, 1], 1e-12))
            out.append((group, name, to_grid(data[:, 0], db)))
    return out


def load_tfs():
    out = []
    for path in sorted(glob.glob(os.path.join(TFS, "*.tf.json"))):
        stem = os.path.basename(path)[:-len(".tf.json")]
        with open(path) as handle:
            data = json.load(handle)
        if stem.startswith("uiowa_"):
            group = "BODIES"
            name = stem[len("uiowa_"):].replace("_", " ")
        else:
            group = "OBJECTS"
            name = stem.replace("_", " ")
        curve = to_grid(np.array(data["freqs_hz"], float), np.array(data["mag_db"], float))
        out.append((group, name, curve))
    out.sort(key=lambda item: (item[0] != "BODIES", item[1]))
    return out


def load_hrtf():
    out = []
    with h5py.File(SOFA, "r") as handle:
        ir = handle["Data.IR"]
        pos = handle["SourcePosition"][:]
        fs = float(handle["Data.SamplingRate"][0])
        az = np.minimum(np.abs(pos[:, 0]), np.abs(360.0 - pos[:, 0]))
        candidates = np.where(az < 1e-6)[0]
        freqs = np.fft.rfftfreq(8192, 1.0 / fs)
        for el in (-60, -40, -20, 0, 20, 40, 60):
            row = candidates[np.argmin(np.abs(pos[candidates, 1] - el))]
            h = np.array(ir[row, 0, :], float)
            mag = np.abs(np.fft.rfft(h, 8192))
            db = 20.0 * np.log10(np.maximum(mag, 1e-12))
            curve = smooth_octave(to_grid(freqs, db), 1.0 / 6.0)
            label = "KEMAR el %+d" % el if el != 0 else "KEMAR el 0"
            out.append(("HRTF", label, curve))
    return out


def fmt(pole):
    return "{%.2f, %.2f, %s}" % (pole[0], pole[1], "true" if pole[2] else "false")


def main():
    entries = load_mouths() + load_tfs() + load_hrtf()
    rows = []
    lines = []
    for group, name, target in entries:
        poles, zeros, used, err, shelf_err = fit_target(target)
        rows.append((group, name, poles, zeros))
        lines.append("%-10s %-24s bands %d  rms %.2f dB  shelf %.2f dB" % (group, name, used, err, shelf_err))
        print(lines[-1])

    with open(OUT_TXT, "w") as handle:
        handle.write("\n".join(lines) + "\n")

    body = []
    for group, name, poles, zeros in rows:
        p = ", ".join(fmt(x) for x in poles)
        z = ", ".join(fmt(x) for x in zeros)
        body.append('    {"%s", "%s", {{%s}}, {{%s}}},' % (group, name, p, z))

    text = "#pragma once\n\n#include \"template_shelf.hpp\"\n\n#include <array>\n\n"
    text += "namespace trench::app {\n\n"
    text += "struct FittedEntry {\n  const char* group;\n  const char* name;\n"
    text += "  std::array<TemplatePole, 6> poles;\n  std::array<TemplatePole, 6> zeros;\n};\n\n"
    text += "inline const std::array<FittedEntry, %d> kFittedShelf{{\n" % len(rows)
    text += "\n".join(body)
    text += "\n}};\n\n}\n"
    with open(OUT_HPP, "w") as handle:
        handle.write(text)


main()
