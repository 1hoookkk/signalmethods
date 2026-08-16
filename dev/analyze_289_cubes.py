import os, csv
from collections import defaultdict, Counter
import numpy as np
import json

path = r'C:\Users\hooki\trench-filter-list\ref\p2k_variants\combined\cubes_raw_bytes.csv'

# Store all cubes: cube_id -> corner_id -> list of 7 stage dicts
cubes = defaultdict(lambda: defaultdict(list))
with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    r = csv.reader(f)
    for row in r:
        if not row or row[0].startswith('#') or row[0] == 'cube':
            continue
        c_num, corner, r0, r1, stage, pk1, pk2, zk1, zk2 = row
        cubes[int(c_num)][corner].append({
            'stage': int(stage),
            'r0': int(r0), 'r1': int(r1),
            'pk1': int(pk1), 'pk2': int(pk2),
            'zk1': int(zk1), 'zk2': int(zk2)
        })

print(f"Loaded {len(cubes)} cubes.")

# 1. Complete Section Byte Recurrence: (pk1, pk2, zk1, zk2)
# Exclude passthrough / identity: let's see what identity is
sec_counts = defaultdict(list)
zero_counts = defaultdict(list)
pole_counts = defaultdict(list)
corner_counts = defaultdict(list)
all_corners = {}

for cid, corners in cubes.items():
    for c_id, stages in corners.items():
        ckey = (cid, c_id)
        # 7-stage corner signature
        corner_sig = tuple((s['pk1'], s['pk2'], s['zk1'], s['zk2']) for s in stages)
        corner_counts[corner_sig].append(ckey)
        all_corners[ckey] = stages
        
        for s in stages:
            s_idx = s['stage']
            sec_tup = (s['pk1'], s['pk2'], s['zk1'], s['zk2'])
            z_tup = (s['zk1'], s['zk2'])
            p_tup = (s['pk1'], s['pk2'])
            
            loc = (cid, c_id, s_idx)
            sec_counts[sec_tup].append(loc)
            zero_counts[z_tup].append(loc)
            pole_counts[p_tup].append(loc)

print(f"Total section states: {289 * 8 * 7} = 16,184")
print(f"Unique 4-byte section states: {len(sec_counts)}")
print(f"Unique zero states: {len(zero_counts)}")
print(f"Unique pole states: {len(pole_counts)}")
print(f"Unique 7-stage full corners: {len(corner_counts)}")
print(f"Shared full corners (>1 endpoint): {sum(1 for v in corner_counts.values() if len(v) > 1)}")

# Top recurring complete sections
sorted_sec = sorted(sec_counts.items(), key=lambda x: len(x[1]), reverse=True)
print("\n=== TOP 15 RECURRING COMPLETE 4-BYTE SECTIONS (289 CUBES) ===")
for i, (sec, locs) in enumerate(sorted_sec[:15]):
    cubes_set = set(l[0] for l in locs)
    slots = Counter(l[2] + 1 for l in locs)
    corners = Counter(l[1] for l in locs)
    print(f"#{i+1}: {len(locs)} occ across {len(cubes_set)} cubes | Slots: {dict(slots)} | [P1={sec[0]}, P2={sec[1]}, Z1={sec[2]}, Z2={sec[3]}]")

# Top recurring zero states
sorted_zeros = sorted(zero_counts.items(), key=lambda x: len(x[1]), reverse=True)
print("\n=== TOP 15 RECURRING ZERO STATES (289 CUBES) ===")
for i, (z, locs) in enumerate(sorted_zeros[:15]):
    cubes_set = set(l[0] for l in locs)
    slots = Counter(l[2] + 1 for l in locs)
    print(f"Zero #{i+1}: {len(locs)} occ across {len(cubes_set)} cubes | Slots: {dict(slots)} | [Z1={z[0]}, Z2={z[1]}]")

# Top recurring pole states
sorted_poles = sorted(pole_counts.items(), key=lambda x: len(x[1]), reverse=True)
print("\n=== TOP 15 RECURRING POLE STATES (289 CUBES) ===")
for i, (p, locs) in enumerate(sorted_poles[:15]):
    cubes_set = set(l[0] for l in locs)
    slots = Counter(l[2] + 1 for l in locs)
    print(f"Pole #{i+1}: {len(locs)} occ across {len(cubes_set)} cubes | Slots: {dict(slots)} | [P1={p[0]}, P2={p[1]}]")

# Shared full corners
shared_corners = [(k, v) for k, v in corner_counts.items() if len(v) > 1]
shared_corners.sort(key=lambda x: len(x[1]), reverse=True)
print("\n=== TOP SHARED FULL 7-STAGE CORNERS (289 CUBES) ===")
for i, (csig, locs) in enumerate(shared_corners[:10]):
    print(f"Corner #{i+1}: {len(locs)} endpoints | Locs: {locs[:6]}...")

# 6 Shared + 1 Replaced Section among corners
shared_6 = []
ckeys = list(all_corners.keys())
# sample search across cubes
print("\nSearching for 6-shared + 1-replaced corner pairs across 289 cubes...")
for i in range(0, min(1000, len(ckeys)), 5):
    k1 = ckeys[i]
    c1 = all_corners[k1]
    for j in range(i+1, min(1000, len(ckeys)), 5):
        k2 = ckeys[j]
        c2 = all_corners[k2]
        matches = sum(1 for s in range(7) if (c1[s]['pk1'], c1[s]['pk2'], c1[s]['zk1'], c1[s]['zk2']) == (c2[s]['pk1'], c2[s]['pk2'], c2[s]['zk1'], c2[s]['zk2']))
        if matches == 6:
            diff_slot = [s for s in range(7) if (c1[s]['pk1'], c1[s]['pk2'], c1[s]['zk1'], c1[s]['zk2']) != (c2[s]['pk1'], c2[s]['pk2'], c2[s]['zk1'], c2[s]['zk2'])][0]
            shared_6.append((k1, k2, diff_slot))

print(f"Found {len(shared_6)} 6-shared + 1-replaced corner pairs in sampled search.")
for pair in shared_6[:8]:
    print(f"  Cube {pair[0][0]}.{pair[0][1]} <-> Cube {pair[1][0]}.{pair[1][1]} (Differs only at S{pair[2]+1})")
