import json, os, glob
from collections import defaultdict

# 1. Mine P2K architecture files for exact raw word partial overlaps
p2k_files = sorted(glob.glob("recipes/architectures/P2k_*.json"))
print(f"Loaded {len(p2k_files)} P2K architecture files.")

# Extract each corner as a set of (stage_idx, pole_hz, pole_r, zero_hz, zero_r, is_real)
p2k_corners = []
for p in p2k_files:
    with open(p, "r", encoding="utf-8") as f:
        d = json.load(f)
    pname = d["name"]
    for cname in ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]:
        stages = []
        for s in d["sections"]:
            g = s["corners"][cname]
            p_data = g["pole"]
            z_data = g["zero"]
            
            p_key = ("real", tuple(p_data["pair"])) if "pair" in p_data else ("conj", round(p_data["hz"], 1), round(p_data["r"], 4))
            z_key = ("real", tuple(z_data["pair"])) if "pair" in z_data else ("conj", round(z_data["hz"], 1), round(z_data["r"], 4))
            stages.append((p_key, z_key))
            
        p2k_corners.append({
            "preset": pname,
            "corner": cname,
            "stages": stages,
            "poles": [s[0] for s in stages],
            "zeros": [s[1] for s in stages]
        })

print(f"Extracted {len(p2k_corners)} P2K corners.")

# Pairwise comparison across all P2K corners
overlap_counts = defaultdict(int)
high_overlaps = []

for i in range(len(p2k_corners)):
    for j in range(i + 1, len(p2k_corners)):
        c1 = p2k_corners[i]
        c2 = p2k_corners[j]
        
        # Check order-independent matching
        poles1 = list(c1["poles"])
        poles2 = list(c2["poles"])
        zeros1 = list(c1["zeros"])
        zeros2 = list(c2["zeros"])
        
        # Matched poles
        matched_poles = 0
        p2_temp = list(poles2)
        for p in poles1:
            if p in p2_temp:
                matched_poles += 1
                p2_temp.remove(p)
                
        # Matched zeros
        matched_zeros = 0
        z2_temp = list(zeros2)
        for z in zeros1:
            if z in z2_temp:
                matched_zeros += 1
                z2_temp.remove(z)
                
        # Lane-preserved matches
        lane_matched_poles = sum(1 for k in range(6) if c1["poles"][k] == c2["poles"][k])
        lane_matched_zeros = sum(1 for k in range(6) if c1["zeros"][k] == c2["zeros"][k])
        
        total_matched = matched_poles + matched_zeros
        overlap_counts[total_matched] += 1
        
        if c1["preset"] != c2["preset"] and total_matched >= 9:
            high_overlaps.append({
                "c1": f"{c1['preset']} {c1['corner']}",
                "c2": f"{c2['preset']} {c2['corner']}",
                "matched_poles": matched_poles,
                "matched_zeros": matched_zeros,
                "lane_poles": lane_matched_poles,
                "lane_zeros": lane_matched_zeros,
                "total": total_matched
            })

print("\n=== P2K PAIRWISE ROOT OVERLAP DISTRIBUTION (12 total roots: 6 poles + 6 zeros) ===")
for k in sorted(overlap_counts.keys(), reverse=True):
    print(f"  Matched roots {k:2d} / 12 : {overlap_counts[k]:5d} corner pairs")

print(f"\n=== CROSS-PRESET HIGH OVERLAPS (>= 9 / 12 roots matched) ({len(high_overlaps)} pairs) ===")
for item in sorted(high_overlaps, key=lambda x: x["total"], reverse=True)[:20]:
    print(f"  {item['c1']:30s} <-> {item['c2']:30s} | Poles: {item['matched_poles']}/6 (lane:{item['lane_poles']}) | Zeros: {item['matched_zeros']}/6 (lane:{item['lane_zeros']}) | Total: {item['total']}/12")
