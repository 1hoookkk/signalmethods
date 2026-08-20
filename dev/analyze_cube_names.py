import json, re
from collections import defaultdict, Counter

with open("ref/morpheus/cubes_decoded.json", "r", encoding="utf-8") as f:
    morph = json.load(f)

cubes = morph["cubes"]
print(f"Loaded {len(cubes)} Morpheus cubes.")

categories = defaultdict(list)

for c in cubes:
    name = c["name"]
    corners = c["corners"]
    
    # Analyze acoustic traits
    is_plane_4 = name.endswith(".4") or all(corners[i]["sections"] == corners[i+4]["sections"] for i in range(4))
    has_arrow = ">" in name or "->" in name or "-To-" in name or "-to-" in name
    has_plus_minus = "+" in name or "-" in name
    
    # Check naming syntax
    if re.search(r"(-To-|-to-|->|>)", name):
        categories["1. DIRECTIONAL MORPH (A -> B Transition)"].append(name)
    elif name.endswith(".4"):
        categories["2. 2D PLANAR CARD (.4 Suffix / 4-Corner Plane)"].append(name)
    elif any(t in name.lower() for t in ["vow", "vox", "hedz", "bouche", "orator", "mouth", "throat", "choral", "sing", "talk", "be-ye", "ee-yi", "ii-yi", "uhr", "yeah", "yah", "yoyo"]):
        categories["3. PHONETIC VOCAL TRACT & TALKER"].append(name)
    elif any(t in name.lower() for t in ["bell", "chime", "gong", "ring", "metal", "anvil", "bronze", "crystal", "glass", "tine"]):
        categories["4. INHARMONIC METALLIC & BELL"].append(name)
    elif any(t in name.lower() for t in ["flng", "flange", "comb", "phase", "sweep", "wah", "pan", "space", "shifter"]):
        categories["5. MODULATION, COMB & PHASER"].append(name)
    elif any(t in name.lower() for t in ["lp", "hp", "bp", "pole", "bass", "boost", "acid", "303", "cutoff", "brick"]):
        categories["6. SYNTH FILTERS & EQ CUTOFFS"].append(name)
    elif any(t in name.lower() for t in ["horn", "brass", "reed", "oboe", "clar", "sax", "string", "cello", "body", "guitar", "piano", "pipe"]):
        categories["7. ACOUSTIC INSTRUMENT BODIES"].append(name)
    else:
        categories["8. CREATIVE / METAPHORIC SOUND DESIGN"].append(name)

print("=== MORPHEUS CUBE NAMING SYNTAX & DESIGN METHODOLOGY ===\n")
for cat, names in sorted(categories.items()):
    print(f"[{cat}] ({len(names)} cubes):")
    # Show sample names
    print(f"  Examples: {', '.join(names[:10])}")
    if len(names) > 10:
        print(f"  ... and {len(names) - 10} more")
    print()
