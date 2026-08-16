import numpy as np, wave, struct

def demod_wav(wav_path, offset=3):
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
    b_sub = bits_arr[offset : offset + (len(bits_arr) - offset) // 8 * 8]
    weights = 2 ** np.arange(8)
    return np.dot(b_sub.reshape(-1, 8), weights).astype(np.uint8).tobytes()

wav_path = r'C:\Users\hooki\Downloads\extracted_firmware\vulcan_v1.00v_170111.wav'
data = demod_wav(wav_path, 3)

hdr_pos = 749
hdr = data[hdr_pos : hdr_pos + 64]
print("Header hex:", hdr.hex())
print("Header ASCII:", "".join(chr(b) if 32 <= b <= 126 else "." for b in hdr))

# Check payload starting at offset 749 + header_len
# In cubes, cube 0 was at 1090 (which was 749 + 341 bytes).
# Let's inspect bytes between 749 and 1200
for pos in range(749, 1200, 4):
    w = struct.unpack('<I', data[pos:pos+4])[0]
    # Check if this could be initial SP
    if 0x20000000 <= w <= 0x20040000:
        words = struct.unpack('<8I', data[pos:pos+32])
        print(f"Possible Vector Table at offset 0x{pos:X} ({pos}):")
        for j, val in enumerate(words):
            print(f"  V[{j}]: 0x{val:08X}")
