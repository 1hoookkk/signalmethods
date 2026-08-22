"""Fit four sampled corners of a TB-303 emulation into one P2K body.

Each recording is one sustained note.  The harmonic envelope is measured at
the fundamental and its harmonics; the oscillator is taken as a sawtooth, so
the filter's response at harmonic k is the measured level plus 20 log10 k.
The four corner targets sit on the core's ERB grid with its ERB weights — the
perceptual space — and every reported number is scored by the C++ core through
the packed P2K words.  Corners are fitted in sequence, each seeded from the
previous one so row identity persists across the square.
"""
import math
import pathlib
import sys

import numpy as np
from scipy.io import wavfile
from scipy.optimize import least_squares

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "out/build/windows-msvc-release/native/research"))
import trench_native_research as core  # noqa: E402

SR = core.kP2kDatumHz
SECTIONS = 6
WARP_MAX = 60.0
CORNERS = ["m0", "m100", "m0q100", "m100q100"]
WAVS = pathlib.Path(r"C:\Users\hooki\Downloads")
OUT = ROOT / "dev" / "tb303_fit.body240"

GRID = np.asarray(core.erb_grid_hz())
WEIGHT = np.asarray(core.erb_grid_weight())
W = 2.0 * math.pi * GRID / SR
Z1 = np.exp(-1j * W)
Z2 = Z1 * Z1


def harmonic_envelope(path):
    sr, x = wavfile.read(path)
    x = x.astype(float)
    if x.ndim > 1:
        x = x.mean(1)
    n = 32768
    seg = x[len(x) // 4: len(x) // 4 + n] * np.hanning(n)
    spectrum = 20.0 * np.log10(np.abs(np.fft.rfft(seg)) + 1e-12)
    bin_hz = sr / n
    lo, hi = int(40 / bin_hz), int(60 / bin_hz)
    coarse = (lo + np.argmax(spectrum[lo:hi])) * bin_hz
    f0 = max(np.arange(coarse - 1, coarse + 1, 0.02),
             key=lambda c: sum(spectrum[int(round(k * c / bin_hz))] for k in range(1, 20)))
    hz, db = [], []
    for k in range(1, int(18000 / f0)):
        i = int(round(k * f0 / bin_hz))
        j = i - 3 + np.argmax(spectrum[i - 3: i + 4])
        hz.append(k * f0)
        db.append(spectrum[j] + 20.0 * math.log10(k))
    return np.asarray(hz), np.asarray(db)


def target_on_grid(hz, db):
    t = np.interp(GRID, hz, db, left=db[0], right=db[-1])
    ratio = 2.0 ** (1.0 / 6.0)
    out = np.empty_like(t)
    for i, f in enumerate(GRID):
        lo = np.searchsorted(GRID, f / ratio)
        hi = max(np.searchsorted(GRID, f * ratio), lo + 1)
        out[i] = t[lo:hi].mean()
    return out


def radius_of_warp(w):
    return 1.0 - 10.0 ** (-w / 20.0)


def coeffs(log_hz, warp):
    r = radius_of_warp(warp)
    return -2.0 * r * math.cos(2.0 * math.pi * math.exp(log_hz) / SR), r * r


def model_db(x):
    total = np.zeros_like(GRID)
    for i in range(SECTIONS):
        a1, a2 = coeffs(x[4 * i], x[4 * i + 1])
        b1, b2 = coeffs(x[4 * i + 2], x[4 * i + 3])
        total += 20.0 * np.log10(np.maximum(np.abs((1 + b1 * Z1 + b2 * Z2) / (1 + a1 * Z1 + a2 * Z2)), 1e-30))
    return total


LO, HI = math.log(20.0), math.log(0.49 * SR)
BOUNDS = (np.asarray([LO, 0.0, LO, 0.0] * SECTIONS), np.asarray([HI, WARP_MAX, HI, WARP_MAX] * SECTIONS))
SQW = np.sqrt(WEIGHT)


def wstats(target, got):
    d = target - got
    d = d - np.sum(WEIGHT * d) / np.sum(WEIGHT)
    return math.sqrt(float(np.sum(WEIGHT * d * d) / np.sum(WEIGHT))), float(np.abs(d).max())


def fit(target, seed):
    def resid(x):
        d = target - model_db(x)
        d = d - np.sum(WEIGHT * d) / np.sum(WEIGHT)
        return SQW * d
    x0 = np.clip(np.asarray(seed, float), BOUNDS[0] + 1e-6, BOUNDS[1] - 1e-6)
    return least_squares(resid, x0, bounds=BOUNDS, method="trf", x_scale="jac", max_nfev=6000).x


def cold_seed(target):
    band = (GRID > 60) & (GRID < 8000)
    knee = GRID[band][np.argmax(np.gradient(target[band]) < -0.15)]
    hz = max(float(knee), 80.0)
    lh = math.log(hz)
    return [lh, 30.0, LO, 0.0,
            lh, 30.0, LO, 0.0,
            lh, 6.0, lh, 0.0,
            math.log(hz / 2), 0.0, math.log(hz / 2), 6.0,
            lh, 0.0, math.log(0.49 * SR), 60.0,
            lh, 0.0, math.log(0.49 * SR), 60.0]


def pack(x, offset_db):
    rows = []
    for i in range(SECTIONS):
        pm, pr = core.lattice_words_from_root(math.exp(x[4 * i]), radius_of_warp(x[4 * i + 1]))
        zm, zr = core.lattice_words_from_root(math.exp(x[4 * i + 2]), radius_of_warp(x[4 * i + 3]))
        rows.append([zm, zr, pm, pr, 0])
    gain = core.nearest_gain_word(10.0 ** (offset_db / 20.0 / SECTIONS) / 4.0)
    for r in rows:
        r[4] = gain
    return rows


targets = {}
for name in CORNERS:
    hz, db = harmonic_envelope(WAVS / f"{name}.wav")
    targets[name] = target_on_grid(hz, db)

solutions = {}
seed = cold_seed(targets["m0"])
body = []
for name in CORNERS:
    t = targets[name]
    x = fit(t, seed if name == "m0" else solutions["m0"])
    if name != "m0":
        alt = fit(t, seed)
        if wstats(t, model_db(alt))[0] + 0.3 < wstats(t, model_db(x))[0]:
            x = alt
    solutions[name] = x
    rms, worst = wstats(t, model_db(x))
    offset = float(np.sum(WEIGHT * (t - model_db(x))) / np.sum(WEIGHT))
    rows = pack(x, offset)
    packed = np.asarray(core.cascade_db([w for r in rows for w in r], list(GRID), SR))
    prms, pworst = wstats(t, packed)
    body += rows
    print(f"{name:<9} continuous {rms:5.2f} dB rms (worst {worst:5.1f})   packed {prms:5.2f} dB rms (worst {pworst:5.1f})")
    for i in range(SECTIONS):
        print(f"   row {i + 1}: pole {math.exp(x[4 * i]):7.0f} Hz r={radius_of_warp(x[4 * i + 1]):.4f}   "
              f"zero {math.exp(x[4 * i + 2]):7.0f} Hz r={radius_of_warp(x[4 * i + 3]):.4f}")

import struct
OUT.write_bytes(struct.pack("<120H", *[w for r in body for w in r]))
print("wrote", OUT)
