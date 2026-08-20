import json, re

with open("ref/morpheus/cubes_decoded.json", "r", encoding="utf-8") as f:
    morph = json.load(f)

cubes = morph["cubes"]
print(f"Total Morpheus cubes: {len(cubes)}")

numeric_cubes = []
for idx, c in enumerate(cubes):
    name = c["name"]
    # Check if name contains numbers, starts with numbers, or is numeric/operator code
    if re.match(r"^[\d\W]", name) or any(char.isdigit() for char in name):
        numeric_cubes.append((idx, name, c))

print(f"Found {len(numeric_cubes)} cubes with numeric / operator naming patterns:\n")

for idx, name, c in numeric_cubes:
    corners = c["corners"]
    
    # Check active stages and what it does
    active_poles_c0 = []
    active_zeros_c0 = []
    for s in corners[0]["sections"]:
        if s["pole"]["r"] > 0.45:
            active_poles_c0.append((round(s["pole"]["hz"], 1), round(s["pole"]["r"], 4)))
        if s["zero"]["r"] > 0.45:
            active_zeros_c0.append((round(s["zero"]["hz"], 1), round(s["zero"]["r"], 4)))
            
    active_poles_c1 = []
    for s in corners[1]["sections"]:
        if s["pole"]["r"] > 0.45:
            active_poles_c1.append((round(s["pole"]["hz"], 1), round(s["pole"]["r"], 4)))
            
    print(f"Cube #{idx:03d}: '{name}'")
    print(f"  C0 (M0_Q0): Poles={active_poles_c0[:4]} | Zeros={active_zeros_c0[:4]}")
    print(f"  C1 (M100_Q0): Poles={active_poles_c1[:4]}")
    print()
