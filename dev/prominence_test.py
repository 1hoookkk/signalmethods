"""The shipped prominence measures rise from the LEFT only.  Does that change
which peaks get picked, versus a symmetric two-sided prominence?"""
import json, math, sys
import numpy as np
sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
sys.path.insert(0, r"C:\Users\hooki\trench-native\dev")
import trench_native_research as core
import fit_dvtd_topology as F

grid = np.asarray(core.erb_grid_hz())


def pick(db, count, two_sided):
    curve = np.convolve(db, np.ones(5) / 5.0, mode="same")
    band = (grid >= F.FIT_LO_HZ) & (grid <= F.FIT_HI_HZ)
    found = []
    for i in range(1, len(grid) - 1):
        if not band[i]:
            continue
        if curve[i] >= curve[i - 1] and curve[i] > curve[i + 1]:
            left = curve[max(0, i - 12):i].min(initial=curve[i])
            if two_sided:
                right = curve[i + 1:i + 13].min(initial=curve[i])
                prom = curve[i] - max(left, right)
            else:
                prom = curve[i] - left
            found.append((grid[i], prom))
    found.sort(key=lambda p: -p[1])
    return sorted(hz for hz, _ in found[:count])


rows = json.load(open(sys.argv[1]))
same = diff = 0
examples = []
for r in rows:
    t = np.asarray(r["target"])
    n = 6 - r["cut_bells_seeded"]
    a, b = pick(t, n, False), pick(t, n, True)
    if [round(x) for x in a] == [round(x) for x in b]:
        same += 1
    else:
        diff += 1
        if len(examples) < 6:
            examples.append((r["mouth"], a, b))
print(f"mouths where the two prominence rules pick the SAME peak set: {same}/{same+diff}")
print(f"                                            DIFFERENT       : {diff}/{same+diff}\n")
for m, a, b in examples:
    print(f"  {m}")
    print(f"     left-only : {' '.join(f'{x:6.0f}' for x in a)}")
    print(f"     two-sided : {' '.join(f'{x:6.0f}' for x in b)}")
