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

# Let's inspect where magic strings are
for i in range(len(data) - 5):
    if data[i:i+2] == b'_V':
        print(f"Magic at 0x{i:X} ({i}): {data[i:i+16]}")
