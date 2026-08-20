import json
import math
import numpy as np
from collections import defaultdict, Counter

# Load decoded ground truth
with open("ref/morpheus/cubes_decoded.json", "r") as f:
    data = json.load(f)

FS = data["law"]["hz_datum"]  # 39062.5 Hz
cubes = data["cubes"]

print("================================================================================")
print(f"CORPUS-WIDE STRUCTURAL INVENTORY (Native Fs = {FS} Hz, 289 Cubes, 2,312 Corners)")
print("================================================================================\n")

# 1. Identity & Active State Census
total_corners = 0
active_corners = 0
all_identity_corners = 0

# Track per-lane active/identity counts
lane_active_count = [0] * 7
lane_pole_only = [0] * 7
lane_zero_only = [0] * 7
lane_pole_and_zero = [0] * 7
lane_identity = [0] * 7

# .4 Cube inventory
dot_4_cubes = []
non_dot_4_cubes = []
bitmask_groups = defaultdict(list)

# Section geometry frequency/radius dictionary for recurrence mining
lane_geometries = defaultdict(list)

def is_identity_section(s):
    rp = s["pole"]["r"]
    rz = s["zero"]["r"]
    hp = s["pole"]["hz"]
    hz = s["zero"]["hz"]
    if rp == 0.0 and rz == 0.0:
        return True
    if abs(rp - rz) < 1e-5 and abs(hp - hz) < 1e-2:
        return True
    return False

for c in cubes:
    c_idx = c["index"]
    c_name = c["name"].strip()
    is_dot_4 = ".4" in c_name or c_name.endswith(" 4")
    
    corner_active_mask = 0
    active_in_cube = 0
    
    for ci, co in enumerate(c["corners"]):
        total_corners += 1
        active_sections = 0
        
        for si, s in enumerate(co["sections"]):
            ident = is_identity_section(s)
            rp = s["pole"]["r"]
            rz = s["zero"]["r"]
            
            if ident:
                lane_identity[si] += 1
            else:
                lane_active_count[si] += 1
                active_sections += 1
                
                if rp > 0 and rz == 0:
                    lane_pole_only[si] += 1
                elif rp == 0 and rz > 0:
                    lane_zero_only[si] += 1
                elif rp > 0 and rz > 0:
                    lane_pole_and_zero[si] += 1
                
                # Record geometry key: (lane, round(hz_p), round(r_p, 3), round(hz_z), round(r_z, 3))
                geom_key = (si, round(s["pole"]["hz"], 1), round(rp, 4), round(s["zero"]["hz"], 1), round(rz, 4))
                lane_geometries[geom_key].append((c_idx, c_name, ci))
                
        if active_sections > 0:
            active_corners += 1
            active_in_cube += 1
            corner_active_mask |= (1 << ci)
        else:
            all_identity_corners += 1
            
    mask_str = f"{corner_active_mask:08b}"
    if is_dot_4:
        dot_4_cubes.append((c_idx, c_name, corner_active_mask, mask_str, active_in_cube))
    else:
        non_dot_4_cubes.append((c_idx, c_name, corner_active_mask, mask_str, active_in_cube))
        
    bitmask_groups[mask_str].append((c_idx, c_name, is_dot_4))

print(f"1. GLOBAL CORNER CENSUS:")
print(f"   Total Cubes: {len(cubes)}")
print(f"   Total Corners: {total_corners}")
print(f"   Active Corners: {active_corners} ({active_corners / total_corners * 100:.1f}%)")
print(f"   All-Identity / Null Corners: {all_identity_corners} ({all_identity_corners / total_corners * 100:.1f}%)")
print()

# 2. .4 CUBES ENUMERATION & BITMASK ANALYSIS
print("================================================================================")
print(f"2. .4 CUBES & ACTIVE CORNER BITMASKS ({len(dot_4_cubes)} named '.4' cubes out of 289)")
print("================================================================================")
mask_counter = Counter(m[3] for m in dot_4_cubes)
for mask_str, count in mask_counter.most_common():
    active_count = mask_str.count('1')
    active_indices = [7 - i for i, b in enumerate(mask_str) if b == '1']
    print(f"\nBitmask [{mask_str}] -> {count} '.4' cubes ({active_count} active corners: {sorted(active_indices)}):")
    sample_names = [f"#{c[0]} {c[1]}" for c in dot_4_cubes if c[3] == mask_str][:6]
    print(f"   Examples: {', '.join(sample_names)}")

print("\n--------------------------------------------------------------------------------")
print("ALL CUBES BITMASK DISTRIBUTION (Both .4 and standard cubes):")
print("--------------------------------------------------------------------------------")
all_mask_counter = Counter(f"{c[2]:08b}" for c in dot_4_cubes + non_dot_4_cubes)
for mask_str, count in all_mask_counter.most_common():
    active_count = mask_str.count('1')
    active_indices = [7 - i for i, b in enumerate(mask_str) if b == '1']
    print(f"  Mask [{mask_str}]: {count:3d} cubes | {active_count}/8 corners active: {sorted(active_indices)}")

# 3. LANE-BY-LANE GEOMETRIC FACTOR INVENTORY
print("\n================================================================================")
print("3. PER-LANE FACTOR GEOMETRY (Total corners evaluated = 2,312)")
print("================================================================================")
print("Lane | Active | Identity | Pole-Only | Zero-Only | Pole+Zero (Dipole) | S_k Zero Disabled")
print("-----+--------+----------+-----------+-----------+--------------------+------------------")
for si in range(7):
    s7_zero_stat = "100.0% (Zero R=0)" if si == 6 and lane_zero_only[si] == 0 and lane_pole_and_zero[si] == 0 else "Active"
    print(f" S{si+1}  | {lane_active_count[si]:6d} |  {lane_identity[si]:7d} |   {lane_pole_only[si]:7d} |   {lane_zero_only[si]:7d} |       {lane_pole_and_zero[si]:12d} | {s7_zero_stat}")

# 4. RECURRENT LANE-AWARE POLE-ZERO COMBINATIONS
print("\n================================================================================")
print("4. TOP RECURRENT POLE-ZERO MOTIFS PER LANE (Exact parameter matches across cubes)")
print("================================================================================")
for si in range(7):
    print(f"\n--- STAGE S{si+1} TOP RECURRENT GEOMETRIES ---")
    lane_items = [item for item in lane_geometries.items() if item[0][0] == si]
    lane_items.sort(key=lambda x: len(x[1]), reverse=True)
    
    for (lane, hp, rp, hz, rz), occurrences in lane_items[:5]:
        cube_count = len(set(occ[0] for occ in occurrences))
        kind = "POLE-ONLY" if rz == 0 else "ZERO-ONLY" if rp == 0 else f"DIPOLE (ratio={hz/hp:.2f}, dR={rp-rz:+.3f})"
        print(f"  [{len(occurrences):3d} corners across {cube_count:2d} cubes] Pole: {hp:7.1f} Hz (R={rp:.4f}) | Zero: {hz:7.1f} Hz (R={rz:.4f}) -> {kind}")
        sample_cubes = list(dict.fromkeys(occ[1] for occ in occurrences))[:4]
        print(f"      Cubes: {', '.join(sample_cubes)}")
