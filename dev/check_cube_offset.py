import os

cubes_bin = 'dev/cubes_bitstream_recovered.bin'
with open(cubes_bin, 'rb') as f:
    data = f.read()

# Let's find where 'Null Cube' and 'MdQ 2PoleLP' are in data
pos_null = data.find(b'Null Cube')
pos_c46 = data.find(b'MdQ 2PoleLP')
print(f"'Null Cube' offset in file: {pos_null}")
print(f"'MdQ 2PoleLP' offset in file: {pos_c46}")

id_template = bytes.fromhex('bad7bdeeddeb5ef7fb75af7bfdef7ffffef7bfff0000dfff0000000000000000ffffff00ffffffffffffffff')

if pos_c46 != -1:
    c46_payload = data[pos_c46 + 12 : pos_c46 + 320]
    for s in range(7):
        stg = c46_payload[s*44 : (s+1)*44]
        match = (stg == id_template)
        print(f"Stage S{s+1} match identity: {match} (hex: {stg.hex()[:32]}...)")
