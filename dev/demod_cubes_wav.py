import wave
import numpy as np

wav_path = r'C:\Users\hooki\Downloads\extracted_firmware\cubes_v1.01vc_170120.wav'
with wave.open(wav_path, 'rb') as w:
    raw = w.readframes(w.getnframes())

# 8-bit unsigned -> signed float
samples = np.frombuffer(raw, dtype=np.uint8).astype(int) - 128

# Detect zero crossings (from <=0 to >0, and >0 to <=0)
# A simple way to demodulate:
# Clock is 48000 / 6000 = 8 samples per bit period.
# Two pulses of length 4 (short-short) vs one pulse of length 8 (long).

crossings = np.where(np.diff(np.signbit(samples)))[0]
intervals = np.diff(crossings)

# Convert intervals to symbols:
# interval <= 6 is SHORT (S), interval > 6 is LONG (L)
symbols = []
for iv in intervals:
    if iv <= 6:
        symbols.append('S')
    else:
        symbols.append('L')

print(f"Total zero crossing intervals: {len(intervals)}")
print(f"Symbol counts: S={symbols.count('S')}, L={symbols.count('L')}")

# In biphase mark:
# A bit boundary occurs at every transition.
# A '1' has an intermediate transition (SS -> length 8 with transition at 4).
# A '0' has no intermediate transition (L -> length 8 with no transition).
# Let's parse symbol stream:
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
        # resync / anomaly
        i += 1

print(f"Decoded {len(bits)} bits ({len(bits)/8:.1f} bytes)")

# Convert bits to byte array (try MSB-first and LSB-first)
bits_arr = np.array(bits)
n_bytes = len(bits) // 8

def bits_to_bytes_msb(b_slice):
    b = b_slice.reshape(-1, 8)
    weights = 2 ** np.arange(7, -1, -1)
    return np.dot(b, weights).astype(np.uint8)

def bits_to_bytes_lsb(b_slice):
    b = b_slice.reshape(-1, 8)
    weights = 2 ** np.arange(8)
    return np.dot(b, weights).astype(np.uint8)

# Check all 8 possible bit alignments for sync patterns / ASCII headers / SysEx
for offset in range(8):
    b_sub = bits_arr[offset : offset + (len(bits_arr) - offset) // 8 * 8]
    bytes_msb = bits_to_bytes_msb(b_sub).tobytes()
    bytes_lsb = bits_to_bytes_lsb(b_sub).tobytes()
    
    # Check for ASCII strings or common headers
    for name, data in [('MSB', bytes_msb), ('LSB', bytes_lsb)]:
        # Search for ASCII runs
        ascii_runs = []
        cur_run = []
        for byte in data:
            if 32 <= byte <= 126:
                cur_run.append(chr(byte))
            else:
                if len(cur_run) >= 4:
                    ascii_runs.append("".join(cur_run))
                cur_run = []
        if len(cur_run) >= 4:
            ascii_runs.append("".join(cur_run))
            
        print(f"Offset {offset} ({name}): Found {len(ascii_runs)} ASCII runs >= 4 chars.")
        if ascii_runs:
            print(f"   Sample runs: {ascii_runs[:10]}")
            
        # Check SysEx 0xF0 ... 0xF7
        f0_count = data.count(b'\xf0')
        f7_count = data.count(b'\xf7')
        if f0_count > 0:
            print(f"   SysEx markers: F0={f0_count}, F7={f7_count}")

