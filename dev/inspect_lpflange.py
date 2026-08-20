import json

with open("ref/morpheus/cubes_decoded.json") as f:
    d = json.load(f)

c = d["cubes"][1]
print("NAME:", c["name"])
for ci, co in enumerate(c["corners"]):
    print(f"\n=== CORNER C{ci} (gain={co['gain']:.4f}) ===")
    for si, s in enumerate(co["sections"]):
        hp = s["pole"]["hz"]
        rp = s["pole"]["r"]
        hz = s["zero"]["hz"]
        rz = s["zero"]["r"]
        raw = s["raw"]
        print(f"  S{si+1}: Pole {hp:7.1f}Hz (r={rp:.4f}), Zero {hz:7.1f}Hz (r={rz:.4f}) | raw={raw}")
