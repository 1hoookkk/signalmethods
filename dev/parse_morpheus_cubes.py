import wave, os, struct
import numpy as np

wav_path = r'C:\Users\hooki\Downloads\extracted_firmware\cubes_v1.01vc_170120.wav'
with wave.open(wav_path, 'rb') as w:
    raw = w.readframes(w.getnframes())

samples = np.frombuffer(raw, dtype=np.uint8).astype(int) - 128
crossings = np.where(np.diff(np.signbit(samples)))[0]
intervals = np.diff(crossings)

symbols = ['S' if iv <= 6 else 'L' for iv in intervals]

bits = []
i = 0
while i < len(symbols):
    if symbols[i] == 'L':
        bits.append(0)
        i += 1
    elif i + 1 < len(symbols) and symbols[i] == 'S' and symbols[i+1] == 'S':
        bits.append(1)
        i += 2
    else:
        i += 1

bits_arr = np.array(bits)
# Offset 3, LSB
offset = 3
b_sub = bits_arr[offset : offset + (len(bits_arr) - offset) // 8 * 8]
weights = 2 ** np.arange(8)
data = np.dot(b_sub.reshape(-1, 8), weights).astype(np.uint8).tobytes()

print(f"Demodulated binary stream: {len(data)} bytes")

# Search for the header / cube records
# In Rossum Morpheus firmware:
# Header starts with '_VCB1' or similar magic
magic_idx = data.find(b'_VCB1')
print(f"Magic '_VCB1' found at byte index: {magic_idx}")

# Let's inspect the 256 bytes around magic_idx
if magic_idx != -1:
    header = data[magic_idx:magic_idx+256]
    print("Header bytes (hex):", header[:64].hex())
    print("Header text:", repr(header[:64]))

# Find all occurrences of 12-char cube names (or fixed-size records)
# Let's see the distance between consecutive cube names
# Search for names like 'Null Cube', 'LPFlange.4', etc.
import re
# Cube names in Morpheus are 12 characters ASCII padded with spaces
matches = [m.start() for m in re.finditer(rb'[A-Za-z0-9_\-\. ]{8,12}  ', data)]
print(f"Total cube-name-like string occurrences: {len(matches)}")
for m in matches[:20]:
    print(f"Offset 0x{m:05X} ({m:6d}): {repr(data[m:m+16])}")

# Let's measure distance between consecutive name matches
diffs = np.diff(matches)
print(f"Common distances between name matches: {np.unique(diffs, return_counts=True)}")
