#!/usr/bin/env python3
from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from scipy.optimize import linear_sum_assignment
from scipy.signal import find_peaks

ROOT = Path(__file__).resolve().parents[1]
DVTD = ROOT / "recipes/vocal/dvtd"
FS = 44100.0
BAND = (100.0, 10000.0)
N_FORMANTS = 4
LOG_POINTS = 2048
MIN_SEP_OCT = 1.0 / 3.0
ZERO_ST_ABOVE_POLE = 6.0
MIN_ZERO_CLEARANCE_ST = 2.5
ZERO_SEARCH_SPAN_ST = 8.0
ZERO_PARK_HZ = 19000.0

S1_ANCHOR = {"pole_hz": 7374.0, "pole_r": 0.9642, "zero_hz": 2217.0, "zero_r": 0.9489}
S6_ANCHOR = {"pole_hz": 371.0, "pole_r": 0.9983, "zero_hz": 13732.0, "zero_r": 1.0000}

DEFAULT_CORNERS = [
    ("M0_Q0", "subject-1/s1-16-bett-lax-ae"),
    ("M100_Q0", "subject-1/s1-18-offen-lax-o"),
    ("M0_Q100", "subject-2/s2-16-bett-lax-ae"),
    ("M100_Q100", "subject-2/s2-18-offen-lax-o"),
]

def read_measured(rel: str):
    stem = rel.split("/")[-1]
    path = DVTD / rel / f"{stem}-vvtf-measured.txt"
    d = np.loadtxt(path, skiprows=1)
    f, mag = d[:, 0], d[:, 1]
    keep = (f >= BAND[0]) & (f <= BAND[1])
    f, db = f[keep], 20.0 * np.log10(np.maximum(mag[keep], 1e-12))
    grid = np.geomspace(f[0], f[-1], LOG_POINTS)
    return grid, np.interp(np.log(grid), np.log(f), db)

def extract(f: np.ndarray, db: np.ndarray):
    per_oct = LOG_POINTS / math.log2(BAND[1] / BAND[0])
    idx, props = find_peaks(db, prominence=3.0,
                            distance=max(1, int(per_oct * MIN_SEP_OCT)))
    if len(idx) == 0:
        raise SystemExit("no peaks found")
    order = np.argsort(props["prominences"])[::-1][:N_FORMANTS]
    picked = sorted(int(idx[i]) for i in order)
    out = []
    for i in picked:
        half = db[i] - 3.0
        lo = i
        while lo > 0 and db[lo] > half:
            lo -= 1
        hi = i
        while hi < len(db) - 1 and db[hi] > half:
            hi += 1
        out.append((float(f[i]), float(f[hi] - f[lo]), float(db[i])))
    return out

def track_formants(per_corner: dict, shape_weight: float = 0.0,
                   level_weight: float = 0.0) -> dict:
    labels = list(per_corner)
    anchor = labels[0]
    ref = per_corner[anchor]
    tracked = {anchor: list(ref)}
    for label in labels[1:]:
        cand = per_corner[label]
        n = min(len(ref), len(cand))
        def q_of(pk):
            return pk[0] / pk[1] if pk[1] > 0 else float("inf")

        def shape_cost(i, j):
            qa, qb = q_of(ref[i]), q_of(cand[j])
            if not (math.isfinite(qa) and math.isfinite(qb)) or qa <= 0 or qb <= 0:
                return 0.0
            return abs(12 * math.log2(qb / qa))

        cost = np.array([[abs(12 * math.log2(cand[j][0] / ref[i][0]))
                          + shape_weight * shape_cost(i, j)
                          + level_weight * abs(cand[j][2] - ref[i][2])
                          + 1e-3 * abs(i - j)
                          for j in range(n)] for i in range(n)])
        rows, cols = linear_sum_assignment(cost)
        ordered = [None] * n
        for i, j in zip(rows, cols):
            ordered[i] = cand[j]
        tracked[label] = ordered
    return tracked

def to_coords(hz: float, bw: float):
    theta = 2.0 * math.pi * hz / FS
    r = math.exp(-math.pi * bw / FS)
    return theta, r

def place_zero(pole_hz: float, other_poles: list[float]) -> tuple[float, str]:
    want = pole_hz * 2 ** (ZERO_ST_ABOVE_POLE / 12.0)

    def clear(hz: float) -> bool:
        return all(abs(12 * math.log2(hz / p)) >= MIN_ZERO_CLEARANCE_ST
                   for p in other_poles if p > 0)

    if clear(want):
        return want, f"+{ZERO_ST_ABOVE_POLE:.0f} st"
    step = 0.25
    off = step
    while off <= ZERO_SEARCH_SPAN_ST:
        for cand in (want * 2 ** (off / 12.0), want * 2 ** (-off / 12.0)):
            if cand > pole_hz and cand < FS * 0.49 and clear(cand):
                moved = 12 * math.log2(cand / pole_hz)
                return cand, f"+{moved:.1f} st (moved clear)"
        off += step
    return ZERO_PARK_HZ, "parked (no clear carve)"

def grammar(formants):
    voice_poles = [hz for hz, _bw, _lvl in formants]
    all_poles = [S1_ANCHOR["pole_hz"]] + voice_poles + [S6_ANCHOR["pole_hz"]]

    lanes = [dict(slot=1, role="S1 air anchor (static)",
                  pole_hz=S1_ANCHOR["pole_hz"], pole_r=S1_ANCHOR["pole_r"],
                  zero_hz=S1_ANCHOR["zero_hz"], zero_r=S1_ANCHOR["zero_r"],
                  zero_note="measured anchor")]
    for n, (hz, bw, _lvl) in enumerate(formants):
        _theta, r = to_coords(hz, bw)
        others = [p for p in all_poles if p != hz]
        z_hz, note = place_zero(hz, others)
        lanes.append(dict(slot=2 + n, role=f"S{2+n} voice F{n+1}",
                          pole_hz=hz, pole_r=r,
                          zero_hz=z_hz, zero_r=r, zero_note=note))
    if len(lanes) != 5:
        raise SystemExit(f"expected 4 formants for S2-S5, got {len(lanes)-1}")
    lanes.append(dict(slot=6, role="S6 chest anchor (static)",
                      pole_hz=S6_ANCHOR["pole_hz"], pole_r=S6_ANCHOR["pole_r"],
                      zero_hz=S6_ANCHOR["zero_hz"], zero_r=S6_ANCHOR["zero_r"],
                      zero_note="measured anchor, unit zero"))
    return lanes

def main(argv):
    corners = DEFAULT_CORNERS
    if len(argv) > 1:
        corners = [(f"C{i}", a) for i, a in enumerate(argv[1:])]
    print(f"fs = {FS:.0f} Hz   band {BAND[0]:.0f}-{BAND[1]:.0f} Hz   "
          f"r = exp(-pi*B/fs)   zeros +{ZERO_ST_ABOVE_POLE:.0f} st\n")
    raw = {}
    for label, rel in corners:
        f, db = read_measured(rel)
        raw[label] = extract(f, db)
        print(f"{label}   {rel}")
        print(f"  {'peak Hz':>9}{'-3dB BW':>10}{'level dB':>10}")
        for hz, bw, lvl in raw[label]:
            print(f"  {hz:>9.1f}{bw:>10.1f}{lvl:>10.1f}")
        print()

    tracked = track_formants(raw)
    print("FORMANT TRACKS (Hungarian on the raw peaks, before any filter math)")
    print(f"  {'track':<8}{'lane':<6}" + "".join(f"{l:>13}" for l in tracked))
    for t in range(N_FORMANTS):
        row = "".join(f"{tracked[l][t][0]:>13.1f}" for l in tracked)
        span = max(tracked[l][t][0] for l in tracked) / min(tracked[l][t][0] for l in tracked)
        print(f"  T{t+1:<7}S{t+2:<5}" + row + f"   travel {12*math.log2(span):5.1f} st")
    print()

    table = {label: grammar(tracked[label]) for label in tracked}

    print("FINAL (f, r) COORDINATE ARRAY — 4 corners x 6 lanes")
    print(f"{'corner':<11}{'lane':<6}{'pole_hz':>10}{'pole_r':>9}"
          f"{'zero_hz':>10}{'zero_r':>9}   {'carve':<22}role")
    for label, lanes in table.items():
        for ln in lanes:
            print(f"{label:<11}S{ln['slot']:<5}{ln['pole_hz']:>10.2f}{ln['pole_r']:>9.5f}"
                  f"{ln['zero_hz']:>10.2f}{ln['zero_r']:>9.5f}   "
                  f"{ln['zero_note']:<22}{ln['role']}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
