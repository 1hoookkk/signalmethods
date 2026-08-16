import os, sys, glob, struct, math, re
from collections import defaultdict, Counter
import numpy as np
import xml.etree.ElementTree as ET
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

TAU = 2.0 * math.pi
SR_39K = 39062.5
SR_44K = 44100.0
SR_48K = 48000.0

# 1. Demodulate the ground truth WAV bitstream from cubes_v1.01vc_170120.wav
wav_path = r'C:\Users\hooki\Downloads\extracted_firmware\cubes_v1.01vc_170120.wav'
import wave
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
firmware_data = np.dot(b_sub.reshape(-1, 8), weights).astype(np.uint8).tobytes()

print(f"Demodulated Morpheus firmware stream: {len(firmware_data)} bytes")

# Record format: 332 bytes per cube
# Start of first cube record after header:
first_cube_offset = 1090
record_len = 332
total_cubes = 289

cubes_extracted = []
for c_idx in range(total_cubes):
    pos = first_cube_offset + c_idx * record_len
    rec = firmware_data[pos : pos + record_len]
    if len(rec) < record_len:
        break
    
    # First 12 bytes is cube name
    raw_name = rec[:12]
    # Clean ASCII
    name = "".join(chr(b) if 32 <= b <= 126 else ' ' for b in raw_name).strip()
    if not name:
        name = f"Cube_{c_idx:03d}"
        
    cubes_extracted.append({
        'index': c_idx,
        'name': name,
        'offset': pos,
        'raw_record': rec
    })

print(f"Extracted {len(cubes_extracted)} cube records from firmware stream.")
print("Sample extracted cube names:")
for c in cubes_extracted[:15]:
    print(f"  Cube {c['index']:03d}: '{c['name']}' (offset 0x{c['offset']:05X})")
print("  ...")
for c in cubes_extracted[100:115]:
    print(f"  Cube {c['index']:03d}: '{c['name']}' (offset 0x{c['offset']:05X})")
