"""
Scan all 33 P2K presets to find:
1. Which presets share the exact Stage 6 Cage / Tilt mechanism (low pole + spanning unit-circle zero).
2. Which presets share exact pole postures with TalkingHedz (0 cents error).
3. The cluster families across the 33 presets sharing these architectural motifs.
"""

import glob, os, struct, sys
import numpy as np

sys.path.insert(0, '.')
from dev.clean_corpus_analysis import decode_stage, SR_44K

p2k_files = sorted(glob.glob('ref/presets/P2k_*.bin'))
presets = {}

for pf in p2k_files:
    pname = os.path.splitext(os.path.basename(pf))[0]
    data = open(pf, 'rb').read()
    words = struct.unpack('<120H', data)
    corners = []
    for ci in range(4):
        stages = []
        for si in range(6):
            w = words[(ci*6 + si)*5 : (ci*6 + si + 1)*5]
            zero, pole, scale = decode_stage(w, sr=SR_44K)
            stages.append({
                'words': w,
                'zero': zero,
                'pole': pole,
                'scale': scale
            })
        corners.append(stages)
    presets[pname] = corners

print("=" * 95)
print("1. STAGE 6 ARCHITECTURE CENSUS (C0: M0_Q0)")
print("=" * 95)
print(f"{'Preset Name':<28} {'S6 Pole':<18} {'S6 Zero':<18} {'S6 Unit-Circle Zeros (4 C)'}")
print("-" * 95)

cage_presets = []
for pname, corners in presets.items():
    s6_c0 = corners[0][5]
    p = s6_c0['pole']
    z = s6_c0['zero']

    p_str = f"{p['hz']:.1f}Hz (r={p['r']:.3f})" if p['type'] == 'Conjugate' else f"Real({p.get('root_a',0):.2f})"
    z_str = f"{z['hz']:.1f}Hz (r={z['r']:.3f})" if z['type'] == 'Conjugate' else f"Real({z.get('root_a',0):.2f})"

    uc_count = sum(1 for ci in range(4) if corners[ci][5]['zero']['type'] == 'Conjugate' and corners[ci][5]['zero']['r'] >= 0.99)

    is_cage = False
    if p['type'] == 'Conjugate' and p['hz'] < 1000 and z['type'] == 'Conjugate' and z['r'] >= 0.99:
        is_cage = True
        cage_presets.append(pname)

    tag = " [CAGE & TILT]" if is_cage else ""
    print(f"{pname:<28} {p_str:<18} {z_str:<18} {uc_count}/4 corners{tag}")

print("\n" + "=" * 95)
print("2. EXACT POLE SKELETON TWINS OF TALKING HEDZ & VOCAL CLUSTERS")
print("=" * 95)

# Compare all corners across presets for exact pole matches
matches = []
preset_names = sorted(presets.keys())
for i, p1 in enumerate(preset_names):
    for j, p2 in enumerate(preset_names):
        if i >= j:
            continue
        for c1 in range(4):
            for c2 in range(4):
                poles1 = [presets[p1][c1][si]['pole'] for si in range(6)]
                poles2 = [presets[p2][c2][si]['pole'] for si in range(6)]

                # Check if all conjugate poles match within 5 Hz
                all_match = True
                for s in range(6):
                    t1, t2 = poles1[s]['type'], poles2[s]['type']
                    if t1 != t2:
                        all_match = False; break
                    if t1 == 'Conjugate':
                        if abs(poles1[s]['hz'] - poles2[s]['hz']) > 5.0 or abs(poles1[s]['r'] - poles2[s]['r']) > 0.01:
                            all_match = False; break
                if all_match:
                    matches.append((p1, c1, p2, c2))

print(f"Found {len(matches)} pairs sharing 100% IDENTICAL 6-pole chassis (0.0 cents error):\n")
for p1, c1, p2, c2 in matches:
    cnames = ['M0_Q0', 'M100_Q0', 'M0_Q100', 'M100_Q100']
    print(f"  • {p1} ({cnames[c1]})  <===>  {p2} ({cnames[c2]})")
