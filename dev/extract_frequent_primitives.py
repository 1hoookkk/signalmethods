import json, glob, math
from collections import Counter, defaultdict

# Load Morpheus cubes and P2K architectures
with open("ref/morpheus/cubes_decoded.json", "r", encoding="utf-8") as f:
    morph = json.load(f)

p2k_files = sorted(glob.glob("recipes/architectures/P2k_*.json"))
p2k_presets = [json.load(open(f, "r", encoding="utf-8")) for f in p2k_files]

# Extract all stage primitives
stages = []

# 1. Morpheus stages
for c in morph["cubes"]:
    for ci, cor in enumerate(c["corners"]):
        for si, s in enumerate(cor["sections"]):
            p = s["pole"]
            z = s["zero"]
            w = s.get("raw", [0,0,0,0])
            
            # Identify exact rounded geometry
            phz = round(p.get("hz", 0.0), 1)
            pr  = round(p.get("r", 0.0), 4)
            zhz = round(z.get("hz", 0.0), 1)
            zr  = round(z.get("r", 0.0), 4)
            
            stages.append({
                "source": "morpheus",
                "cube": c["name"],
                "corner": ci,
                "slot": si + 1,
                "pole": (phz, pr),
                "zero": (zhz, zr),
                "raw": tuple(w),
                "key": (phz, pr, zhz, zr)
            })

# 2. P2K stages
for p in p2k_presets:
    pname = p["name"]
    for cname in ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]:
        for si, s in enumerate(p["sections"]):
            g = s["corners"][cname]
            p_data = g["pole"]
            z_data = g["zero"]
            
            if "pair" in p_data or "pair" in z_data:
                continue
            phz = round(p_data.get("hz", 0.0), 1)
            pr  = round(p_data.get("r", 0.0), 4)
            zhz = round(z_data.get("hz", 0.0), 1)
            zr  = round(z_data.get("r", 0.0), 4)
            
            stages.append({
                "source": "p2k",
                "preset": pname,
                "corner": cname,
                "slot": si + 1,
                "pole": (phz, pr),
                "zero": (zhz, zr),
                "raw": (0,0,0,0),
                "key": (phz, pr, zhz, zr)
            })

print(f"Total Stage Instances: {len(stages)}")

# Count distinct primitives
counts = Counter([s["key"] for s in stages])
print(f"Total Distinct Primitives: {len(counts)}")

singletons = sum(1 for k, v in counts.items() if v == 1)
frequent = [(k, v) for k, v in counts.items() if v >= 5]
frequent.sort(key=lambda x: x[1], reverse=True)

print(f"Singletons (n=1): {singletons}")
print(f"Frequent Primitives (n >= 5): {len(frequent)}\n")

# Classify frequent primitives into functional acoustic families
def classify_prim(phz, pr, zhz, zr):
    if pr < 0.25 and zr < 0.25:
        return "1. Sentinels & Transparent Bypass"
    if phz < 100.0 and pr > 0.8:
        return "2. Sub-Bass & DC Anchors (< 100 Hz)"
    if phz > 14000.0 and pr > 0.7:
        return "3. High-Frequency Air Caps (> 14 kHz)"
    if zr >= 0.98 and abs(phz - zhz) > 1000.0:
        return "4. Pinna & Deep Valley Carving Zeroes (Rz ~ 1.0)"
    if abs(phz - zhz) < 150.0 and zr > 0.8 and pr > 0.8:
        return "5. Bound Pairs & Notch Cancellation"
    if pr > 0.95 and zr > 0.85:
        return "6. Resonant Formant / Phaser Rungs (High Q Pole-Zero)"
    if pr > 0.95 and zr < 0.2:
        return "7. Pure Resonant Needles (All-Pole Chords)"
    return "8. Broadband Slopes & General EQ"

by_family = defaultdict(list)
for (phz, pr, zhz, zr), cnt in frequent:
    fam = classify_prim(phz, pr, zhz, zr)
    by_family[fam].append(((phz, pr, zhz, zr), cnt))

for fam in sorted(by_family.keys()):
    items = by_family[fam]
    tot_cnt = sum(c for _, c in items)
    print(f"=== {fam} ({len(items)} primitives, {tot_cnt} instances) ===")
    for (phz, pr, zhz, zr), cnt in items[:15]:
        print(f"  n={cnt:4d} | Pole: {phz:7.1f} Hz (r={pr:.4f})  | Zero: {zhz:7.1f} Hz (r={zr:.4f})")
    if len(items) > 15:
        print(f"  ... and {len(items) - 15} more in this family")
    print()
