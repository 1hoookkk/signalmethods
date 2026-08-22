"""Re-fit the affected mouths with a two-sided prominence, without touching the
shipped script.  Does fixing the selection rule actually improve the fit?"""
import json, pathlib, sys
import numpy as np
sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
sys.path.insert(0, r"C:\Users\hooki\trench-native\dev")
import trench_native_research as core
import fit_dvtd_topology as F

grid = np.asarray(core.erb_grid_hz())


def two_sided(g, db, count, sign=1.0):
    curve = sign * np.convolve(db, np.ones(5) / 5.0, mode="same")
    band = (g >= F.FIT_LO_HZ) & (g <= F.FIT_HI_HZ)
    found = []
    for i in range(1, len(g) - 1):
        if not band[i]:
            continue
        if curve[i] >= curve[i - 1] and curve[i] > curve[i + 1]:
            left = curve[max(0, i - 12):i].min(initial=curve[i])
            right = curve[i + 1:i + 13].min(initial=curve[i])
            found.append((g[i], curve[i], curve[i] - max(left, right)))
    found.sort(key=lambda p: -p[2])
    found = found[:count]
    found.sort(key=lambda p: p[0])
    while len(found) < count:
        found.append((F.FIT_HI_HZ * 0.9, curve[band].mean(), 3.0))
    return found


AFFECTED = ["s1-03-tiere-tense-i", "s1-06-laehmung-tense-ae", "s1-08-guete-tense-y",
            "s1-15-bass-lax-a", "s1-22-ehe-schwa", "s2-08-guete-tense-y"]
old = {r["mouth"]: r for r in json.load(open(sys.argv[1]))}
paths = {p.parent.name: p for p in F.DVTD.glob("subject-*/*/*-vvtf-measured.txt")}

F.pick_extrema = two_sided
print(f'{"mouth":<26}{"left-only":>11}{"two-sided":>11}{"change":>9}   worst dB')
tot_a = tot_b = 0.0
for m in AFFECTED:
    r = F.fit_mouth(paths[m], 44100.0, verbose=False)
    a, b = old[m]["polished_rms_db"], r["polished_rms_db"]
    tot_a += a; tot_b += b
    print(f'{m:<26}{a:11.3f}{b:11.3f}{100*(b-a)/a:8.1f}%   '
          f'{old[m]["polished_max_db"]:.2f} -> {r["polished_max_db"]:.2f}')
print(f'\n{"mean of these six":<26}{tot_a/len(AFFECTED):11.3f}{tot_b/len(AFFECTED):11.3f}'
      f'{100*(tot_b-tot_a)/tot_a:8.1f}%')
