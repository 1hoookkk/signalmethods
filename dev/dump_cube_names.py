import wave, numpy as np, json, os

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
offset = 3
b_sub = bits_arr[offset : offset + (len(bits_arr) - offset) // 8 * 8]
weights = 2 ** np.arange(8)
data = np.dot(b_sub.reshape(-1, 8), weights).astype(np.uint8).tobytes()

first_cube_offset = 1090
record_len = 332
total_cubes = 289

names = []
for c_idx in range(total_cubes):
    pos = first_cube_offset + c_idx * record_len
    rec = data[pos : pos + record_len]
    raw_name = rec[:12]
    clean_name = ''.join(chr(b) if 32 <= b <= 126 else ' ' for b in raw_name).strip()
    names.append((c_idx, clean_name))

print(f"Extracted {len(names)} cube names (Cube 0 to Cube 288):")
for i in range(0, len(names), 10):
    chunk = names[i:i+10]
    print(f"[{i:03d}..{min(len(names)-1, i+9):03d}]: " + ", ".join(f"#{idx}: '{n}'" for idx, n in chunk))

os.makedirs('ref', exist_ok=True)
with open('ref/cubes_289_names.json', 'w', encoding='utf-8') as f:
    json.dump([{'id': idx, 'name': n} for idx, n in names], f, indent=2)

with open('ref/cubes_289_names.csv', 'w', encoding='utf-8') as f:
    f.write("cube_id,name\n")
    for idx, n in names:
        f.write(f"{idx},{n}\n")

print("Saved to ref/cubes_289_names.json and ref/cubes_289_names.csv")
