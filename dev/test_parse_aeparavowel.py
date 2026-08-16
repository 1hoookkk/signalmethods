import wave, numpy as np, struct, math, json

# Load 289 cubes
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
        bits.append(0); i += 1
    elif i + 1 < len(symbols) and symbols[i] == 'S' and symbols[i+1] == 'S':
        bits.append(1); i += 2
    else:
        i += 1

bits_arr = np.array(bits)
offset = 3
b_sub = bits_arr[offset : offset + (len(bits_arr) - offset) // 8 * 8]
weights = 2 ** np.arange(8)
data = np.dot(b_sub.reshape(-1, 8), weights).astype(np.uint8).tobytes()

first_cube_offset = 1090
record_len = 332

# Let's inspect Cube #22: AEParaVowel
cube_idx = 22
pos = first_cube_offset + cube_idx * record_len
rec = data[pos : pos + record_len]
name = rec[:12].decode('ascii', errors='ignore').strip()
payload = rec[12:332]

print(f"=== PARSING CUBE #{cube_idx}: \"{name}\" ===")
print(f"Payload length: {len(payload)} bytes")
print("Payload Hex:")
for row in range(0, len(payload), 32):
    print(f"  {row:03d}: {payload[row:row+32].hex()}")

# Let's analyze the first few bytes (header / mode descriptor)
mode_byte = payload[0]
print(f"\nHeader Byte 0: 0x{mode_byte:02X} ({mode_byte})")

# Let's inspect 16-bit words in payload
words_16 = struct.unpack('<160H', payload)
print(f"First 16 words (int16 / uint16): {words_16[:16]}")
