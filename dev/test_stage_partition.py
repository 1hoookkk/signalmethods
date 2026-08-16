import numpy as np

# Let's inspect the 20-byte and 44-byte structure
bin_path = r'C:\Users\hooki\trench-authoring\dev\cubes_bitstream_recovered.bin'
with open(bin_path, 'rb') as f:
    data = f.read()

rec0_offset = 340
stride = 332

# Look at Stage S1..S7 of Cube 0 (Null Cube)
c0_payload = data[rec0_offset + 12 : rec0_offset + 320]

print("=== DECONSTRUCTING 44-BYTE STAGE INTO (20B + 4B + 20B) OR (22B + 22B) ===")
for s_idx in range(7):
    s_bytes = c0_payload[s_idx*44 : (s_idx+1)*44]
    
    # Split into 20B, 4B, 20B
    p1 = s_bytes[:20]
    mid = s_bytes[20:24]
    p2 = s_bytes[24:]
    
    print(f"\nStage S{s_idx+1}:")
    print(f"  Part 1 (20B / 160b): {p1.hex()}")
    print(f"  Middle ( 4B /  32b): {mid.hex()} (uint16s: {np.frombuffer(mid, dtype='<u2')})")
    print(f"  Part 2 (20B / 160b): {p2.hex()}")

# Let's analyze the bit patterns in Part 1 and Part 2:
# 1. As 10 uint16 words (little endian)
# 2. As 14 x 11-bit mantissas + 4-bit exponents
# 3. As 4 corners * 5 bytes
print("\n--- Part 1 as 10 uint16 words (Stage 1..7) ---")
for s_idx in range(7):
    s_bytes = c0_payload[s_idx*44 : (s_idx+1)*44]
    p1 = s_bytes[:20]
    words_u16 = np.frombuffer(p1, dtype='<u2')
    words_hex = [f"0x{w:04x}" for w in words_u16]
    print(f"S{s_idx+1} P1: {' '.join(words_hex)}")

print("\n--- Part 2 as 10 uint16 words (Stage 1..7) ---")
for s_idx in range(7):
    s_bytes = c0_payload[s_idx*44 : (s_idx+1)*44]
    p2 = s_bytes[24:]
    words_u16 = np.frombuffer(p2, dtype='<u2')
    words_hex = [f"0x{w:04x}" for w in words_u16]
    print(f"S{s_idx+1} P2: {' '.join(words_hex)}")
