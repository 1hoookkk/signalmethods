import csv
import numpy as np

# Load recovered bitstream
bin_path = r'C:\Users\hooki\trench-authoring\dev\cubes_bitstream_recovered.bin'
with open(bin_path, 'rb') as f:
    data = f.read()

rec0_offset = 340
stride = 332

# Load CSV
csv_path = r'C:\Users\hooki\trench-filter-list\ref\p2k_variants\combined\cubes_raw_bytes.csv'
csv_rows = []
with open(csv_path, 'r') as f:
    r = csv.reader(f)
    for row in r:
        if row and not row[0].startswith('#') and row[0] != 'cube':
            csv_rows.append(row)

print(f"Loaded {len(csv_rows)} rows from cubes_raw_bytes.csv")

# Let's inspect Cube 0 in both the CSV and the recovered 44-byte stage stream
rec0_bytes = data[rec0_offset : rec0_offset + stride]
name0 = rec0_bytes[:12]
payload0 = rec0_bytes[12:320] # 308 bytes = 7 * 44

print(f"Cube 0 Name: {name0}")
print("Cube 0 payload (308 bytes hex):")
print(payload0.hex())

# Let's see all rows for cube 0 in CSV
cube0_csv = [row for row in csv_rows if row[0] == '0']
print(f"\nCube 0 in CSV has {len(cube0_csv)} rows (expected 8 corners * 7 stages = 56 rows):")
for r in cube0_csv[:14]:
    print("  ", r)

# Search for the CSV bytes inside payload0 or vice-versa!
# In CSV, corner 000 stage 0 has bytes: pole_k1=85, pole_k2=85, zero_k1=85, zero_k2=85 -> [0x55, 0x55, 0x55, 0x55]
# corner 000 stage 1 has: [85, 85, 85, 95] -> [0x55, 0x55, 0x55, 0x5f]
# corner 000 stage 2 has: [255, 255, 245, 95] -> [0xff, 0xff, 0xf5, 0x5f]
# Let's search for sequences from CSV in payload0
csv_byte_seq = bytes([int(x) for x in cube0_csv[0][2:]])
print(f"\nSearching for corner 000 stage 0 bytes {list(csv_byte_seq)} in payload0: {payload0.find(csv_byte_seq)}")

# Let's search across all 289 cubes to find the exact byte mapping between CSV and recovered payload
for cube_id in range(5):
    c_csv = [row for row in csv_rows if row[0] == str(cube_id)]
    c_bytes = data[rec0_offset + cube_id * stride + 12 : rec0_offset + (cube_id + 1) * stride]
    print(f"\nCube {cube_id}:")
    for s_idx in range(7):
        stage_bytes = c_bytes[s_idx*44 : (s_idx+1)*44]
        print(f"  Stage S{s_idx+1} (44B): {stage_bytes.hex()}")
