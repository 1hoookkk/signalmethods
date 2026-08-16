import numpy as np

bin_path = r'C:\Users\hooki\trench-authoring\dev\cubes_bitstream_recovered.bin'
with open(bin_path, 'rb') as f:
    data = f.read()

rec0_offset = 340
stride = 332
n_records = 289

# Extract all 289 records
# Each record:
# - [0:12]: 12-byte ASCII Name
# - [12:320]: 308 bytes = 7 stages * 44 bytes
# - [320:332]: 12 bytes trailer
records = []
for i in range(n_records):
    start = rec0_offset + i * stride
    rec = data[start : start + stride]
    name = rec[:12].decode('ascii', errors='replace')
    payload = np.frombuffer(rec[12:320], dtype=np.uint8).reshape(7, 44)
    trailer = np.frombuffer(rec[320:], dtype=np.uint8)
    records.append((name, payload, trailer))

print("=== STAGE 0..6 (44 BYTES PER STAGE) ANALYSIS ACROSS 289 RECORDS ===")

# Check byte variances at each of the 44 byte positions within a stage
all_stages = np.array([r[1] for r in records]) # Shape: (289, 7, 44)
print(f"All stages tensor shape: {all_stages.shape}")

# Column-wise statistics across all 289 records * 7 stages = 2023 stage instances
flat_stages = all_stages.reshape(-1, 44) # Shape: (2023, 44)
stage_vars = np.var(flat_stages, axis=0)
stage_means = np.mean(flat_stages, axis=0)
stage_mins = np.min(flat_stages, axis=0)
stage_maxs = np.max(flat_stages, axis=0)

print("\n44-Byte Intra-Stage Field Map (Statistics over 2,023 stage blocks):")
print("Byte Offset | Mean   | Std    | Min | Max | Unique Values")
print("------------+--------+--------+-----+-----+--------------")
for b in range(44):
    u_vals = len(np.unique(flat_stages[:, b]))
    print(f"   +{b:02d}     | {stage_means[b]:6.2f} | {np.sqrt(stage_vars[b]):6.2f} | {stage_mins[b]:3d} | {stage_maxs[b]:3d} | {u_vals:4d}")

# Let's inspect Stage 0..6 of Record 0 (Null Cube) vs Record 22 (AEParaVowel)
print("\n--- Record 0 ('Null Cube') 7 Stages (44 bytes each) in Hex ---")
for s_idx in range(7):
    s_bytes = records[0][1][s_idx]
    print(f"Stage S{s_idx+1}: {bytes(s_bytes).hex()}")

print("\n--- Record 22 ('AEParaVowel') 7 Stages (44 bytes each) in Hex ---")
for s_idx in range(7):
    s_bytes = records[22][1][s_idx]
    print(f"Stage S{s_idx+1}: {bytes(s_bytes).hex()}")
