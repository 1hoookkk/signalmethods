import json

cubes_bin = 'dev/cubes_bitstream_recovered.bin'
with open(cubes_bin, 'rb') as f:
    data = f.read()

with open('ref/cubes_289_names.json', 'r') as f:
    cube_names = json.load(f)

id_template = bytes.fromhex('bad7bdeeddeb5ef7fb75af7bfdef7ffffef7bfff0000dfff0000000000000000ffffff00ffffffffffffffff')

base_offset = 340
stride = 332

active_stage_counts = []
print("=== ACTIVE STAGE COUNT PER FILTER ACROSS THE 289 CUBES ===")

for c in cube_names:
    cid = c['id']
    name = c['name']
    pos = base_offset + cid * stride
    payload = data[pos + 12 : pos + 320]
    
    # Check each of the 7 stages
    matches = [payload[s*44 : (s+1)*44] == id_template for s in range(7)]
    active_count = 7 - sum(matches)
    active_stage_counts.append((cid, name, active_count, matches))

# Summary of active stage counts across the 289 cubes:
counts_dist = {}
for cid, name, act, m in active_stage_counts:
    counts_dist[act] = counts_dist.get(act, 0) + 1

print("\nDistribution of Active Stages across all 289 Cubes:")
for act in sorted(counts_dist.keys()):
    print(f"  {act} Active Stages ({act*2}-Pole Filter): {counts_dist[act]} cubes ({counts_dist[act]/289*100:.1f}%)")

print("\nSample Filter Stage Allocations:")
for cid in [0, 46, 48, 203, 22, 13]: # Null, 2Pole, 4Pole, 6Pole, AEParaVowel, LP>Flng2
    cid, name, act, m = active_stage_counts[cid]
    stages_str = "".join(["." if is_id else "A" for is_id in m])
    print(f"  Cube #{cid:3d} {name:<15}: {act} Active Stages [{stages_str}]")
