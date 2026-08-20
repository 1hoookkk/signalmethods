import json, glob, math
import numpy as np
from collections import defaultdict, Counter

# Load Morpheus and P2K data
with open("ref/morpheus/cubes_decoded.json", "r", encoding="utf-8") as f:
    morph = json.load(f)

p2k_files = sorted(glob.glob("recipes/architectures/P2k_*.json"))
p2k_presets = [json.load(open(f, "r", encoding="utf-8")) for f in p2k_files]

print("=== 1. AUDITING P2K CORNER RELATIONSHIPS (33 Presets, 4 Corners Each) ===")

q_hz_diffs = []
q_rp_diffs = []
q_rz_diffs = []
q_scale_ratios = []

m_hz_diffs = []
m_rp_diffs = []
m_rz_diffs = []

for p in p2k_presets:
    pname = p["name"]
    corners = {c: [] for c in ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]}
    scales = {}
    
    for s in p["sections"]:
        for cname in corners.keys():
            g = s["corners"][cname]
            p_data = g["pole"]
            z_data = g["zero"]
            scale = g.get("scale", 1.0)
            scales[cname] = scale
            
            phz = p_data.get("hz", 0.0) if "hz" in p_data else 0.0
            pr  = p_data.get("r", 0.0) if "r" in p_data else 0.0
            zhz = z_data.get("hz", 0.0) if "hz" in z_data else 0.0
            zr  = z_data.get("r", 0.0) if "r" in z_data else 0.0
            corners[cname].append((phz, pr, zhz, zr))
            
    # Compare Q movement: M0_Q0 -> M0_Q100, M100_Q0 -> M100_Q100
    for c_low, c_high in [("M0_Q0", "M0_Q100"), ("M100_Q0", "M100_Q100")]:
        for si in range(6):
            p1 = corners[c_low][si]
            p2 = corners[c_high][si]
            
            if p1[1] > 0.45 or p2[1] > 0.45: # active pole
                q_hz_diffs.append(abs(p2[0] - p1[0]))
                q_rp_diffs.append(p2[1] - p1[1])
            if p1[3] > 0.45 or p2[3] > 0.45: # active zero
                q_rz_diffs.append(p2[3] - p1[3])
                
    # Compare Morph movement: M0_Q0 -> M100_Q0
    for si in range(6):
        p1 = corners["M0_Q0"][si]
        p2 = corners["M100_Q0"][si]
        if p1[1] > 0.45 or p2[1] > 0.45:
            m_hz_diffs.append(abs(p2[0] - p1[0]))
            m_rp_diffs.append(abs(p2[1] - p1[1]))

print(f"P2K Q Axis Analysis (along Q=0 -> Q=100):")
print(f"  Pole Freq Change (|Delta Hz|): median = {np.median(q_hz_diffs):.1f} Hz, mean = {np.mean(q_hz_diffs):.1f} Hz, p90 = {np.percentile(q_hz_diffs, 90):.1f} Hz")
print(f"  Poles with EXACT ZERO Hz change: {sum(1 for d in q_hz_diffs if d < 0.1)} / {len(q_hz_diffs)} ({sum(1 for d in q_hz_diffs if d < 0.1)/len(q_hz_diffs)*100:.1f}%)")
print(f"  Pole Radius Change (Delta Rp):  median = +{np.median(q_rp_diffs):.4f}, mean = +{np.mean(q_rp_diffs):.4f}, p90 = +{np.percentile(q_rp_diffs, 90):.4f}")
print(f"  Zero Radius Change (Delta Rz):  median = {np.median(q_rz_diffs):.4f}, mean = {np.mean(q_rz_diffs):.4f}")

print(f"\nP2K Morph Axis Analysis (along M=0 -> M=100):")
print(f"  Pole Freq Change (|Delta Hz|): median = {np.median(m_hz_diffs):.1f} Hz, mean = {np.mean(m_hz_diffs):.1f} Hz, max = {np.max(m_hz_diffs):.1f} Hz")
print(f"  Pole Radius Change (|Delta Rp|): median = {np.median(m_rp_diffs):.4f}")

print("\n=== 2. AUDITING MORPHEUS 3D CUBE TRANSFORM AXIS (289 Cubes, 8 Corners Each) ===")

morph_t_categories = Counter()

for c in morph["cubes"]:
    cname = c["name"]
    corners = c["corners"] # 8 corners: C0..C7
    
    # Check if Z0 plane (C0..C3) == Z1 plane (C4..C7)
    plane_identical = True
    zero_dissolved = 0
    freq_transposed = 0
    total_active_stages = 0
    
    for ci in range(4):
        c_z0 = corners[ci]
        c_z1 = corners[ci + 4]
        
        for si in range(7):
            s0 = c_z0["sections"][si]
            s1 = c_z1["sections"][si]
            w0 = s0["raw"]
            w1 = s1["raw"]
            
            if w0 != w1:
                plane_identical = False
                
            p0 = s0["pole"]
            p1 = s1["pole"]
            z0 = s0["zero"]
            z1 = s1["zero"]
            
            if z0["r"] > 0.5 and z1["r"] < 0.2:
                zero_dissolved += 1
            if p0["r"] > 0.5 and p1["r"] > 0.5:
                total_active_stages += 1
                if abs(p1["hz"] - p0["hz"]) > 50.0:
                    freq_transposed += 1
                    
    if plane_identical:
        morph_t_categories["1. 2D Plane Only (Z0 == Z1, e.g. .4 cubes)"] += 1
    elif zero_dissolved >= 2:
        morph_t_categories["2. Zero Dissolution (Z1 removes zeroes / notches)"] += 1
    elif freq_transposed >= 4:
        morph_t_categories["3. Frequency / Vowel Morph along Transform"] += 1
    else:
        morph_t_categories["4. Targeted 1-Stage Mutation / Subtle Tweak"] += 1

print(f"Morpheus Transform (Z0 -> Z1) Archetype Breakdown across {len(morph['cubes'])} cubes:")
for cat, count in morph_t_categories.most_common():
    pct = count / len(morph["cubes"]) * 100.0
    print(f"  {cat:60s}: {count:3d} cubes ({pct:5.1f}%)")
