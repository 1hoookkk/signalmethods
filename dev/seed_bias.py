"""Does the peak-picking seed bind the answer, or does the optimiser leave it?"""
import json, math, pathlib, sys
import numpy as np
sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
sys.path.insert(0, r"C:\Users\hooki\trench-native\dev")
import trench_native_research as core
import fit_dvtd_topology as F

SR = 44100.0
grid = np.asarray(core.erb_grid_hz())
rows = json.load(open(sys.argv[1]))

moved, inband, outband, total = [], 0, 0, 0
for r in rows:
    target = np.asarray(r["target"])
    seeded = [hz for hz, _, _ in F.pick_extrema(grid, target, 6 - r["cut_bells_seeded"], 1.0)]
    seeded += [hz for hz, _, _ in F.pick_extrema(grid, target, r["cut_bells_seeded"], -1.0)]
    got = []
    for s in range(1, 7):
        g = core.geometry(r["words"][s * 5:(s + 1) * 5], SR)
        if g["pole"]["kind"] == "conjugate":
            got.append(g["pole"]["hz"])
    for hz in got:
        total += 1
        if F.FIT_LO_HZ <= hz <= F.FIT_HI_HZ: inband += 1
        else: outband += 1
    for a, b in zip(sorted(seeded), sorted(got)):
        if a > 0 and b > 0:
            moved.append(abs(math.log2(b / a)) * 12.0)

m = np.asarray(moved)
print(f"seeded-to-final pole movement, {len(m)} pairs, in semitones")
for q in (10, 25, 50, 75, 90, 99):
    print(f"   p{q:<3} {np.percentile(m, q):7.2f} st")
print(f"   mean {m.mean():.2f}   share moving under 1 st: {100*(m<1).mean():.0f}%"
      f"   over 6 st: {100*(m>6).mean():.0f}%")
print(f"\nfitted pole placement vs the 100-8000 Hz fit band:")
print(f"   inside  {inband:4d}  ({100*inband/total:.0f}%)")
print(f"   outside {outband:4d}  ({100*outband/total:.0f}%)   <- seeds can never start here")
lp = [core.geometry(r["words"][0:5], SR) for r in rows]
lph = [g["pole"]["hz"] for g in lp if g["pole"]["kind"] == "conjugate"]
print(f"\nlowpass pole: n={len(lph)} conjugate, median {np.median(lph):.0f} Hz, "
      f"range {min(lph):.0f}-{max(lph):.0f} Hz")
