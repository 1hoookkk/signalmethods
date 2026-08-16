import wave
import numpy as np

wav_path = r'C:\Users\hooki\Downloads\extracted_firmware\cubes_v1.01vc_170120.wav'
with wave.open(wav_path, 'rb') as w:
    raw = w.readframes(w.getnframes())

samples = np.frombuffer(raw, dtype=np.uint8).astype(float) - 128.0
signs = np.signbit(samples)
cross_idx = np.where(np.diff(signs))[0]
t_interp = cross_idx - samples[cross_idx] / (samples[cross_idx+1] - samples[cross_idx])
intervals = np.diff(t_interp)
syms = np.where(intervals < 6.0, 1, 2)

bits = []
i = 0
while i < len(syms):
    if syms[i] == 2:
        bits.append(0)
        i += 1
    elif i + 1 < len(syms) and syms[i] == 1 and syms[i+1] == 1:
        bits.append(1)
        i += 2
    else:
        i += 1

bits = np.array(bits, dtype=np.uint8)

shift = 3
n_bytes = (len(bits) - shift) // 8
b_slice = bits[shift : shift + n_bytes * 8].reshape(n_bytes, 8)
data = np.dot(b_slice, 2**np.arange(8)).astype(np.uint8).tobytes()

print(f"Total decoded bytes: {len(data)}")

# Let's inspect all 289 records starting at offset 1090, stride 332
start_offset = 1090
stride = 332
n_records = 289

names = []
clean_ascii_count = 0

for rec_i in range(n_records):
    offset = start_offset + rec_i * stride
    rec_data = data[offset : offset + stride]
    if len(rec_data) < stride:
        print(f"Record {rec_i} truncated: length {len(rec_data)}")
        break
    name_bytes = rec_data[:12]
    payload = rec_data[12:]
    
    # Check ASCII
    try:
        name_str = name_bytes.decode('ascii')
        is_clean = all(32 <= b <= 126 for b in name_bytes)
    except:
        name_str = repr(name_bytes)
        is_clean = False
        
    if is_clean:
        clean_ascii_count += 1
        
    names.append(name_str)
    
    if rec_i < 10 or rec_i in (22, 50, 100, 200, 288):
        print(f"Rec {rec_i:3d} (offset {offset:6d}): '{name_str}' | payload (first 32B): {payload[:32].hex()}")

print(f"\nSummary: {clean_ascii_count} / {n_records} records have clean 12-byte ASCII names at stride 332.")
