import wave, numpy as np, struct, re, os

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
print(f"Demodulated Vulcan firmware: {len(data)} bytes")

# Save raw binary to disk for disassembler
os.makedirs('dev/disasm', exist_ok=True)
raw_bin_path = 'dev/disasm/vulcan_raw.bin'
with open(raw_bin_path, 'wb') as fp:
    fp.write(data)

# Let's search for vector table:
# Vector table pattern:
# Word 0: Initial SP: 0x2000xxxx - 0x2003xxxx (STM32F4 SRAM: 128KB - 192KB, 0x20000000 to 0x20030000)
# Word 1: Reset Vector: 0x0800xxxx - 0x081fffff (STM32 Flash, odd address for Thumb)
# Word 2: NMI: 0x080xxxxx (odd)
# Word 3: HardFault: 0x080xxxxx (odd)

print("\nSearching for Cortex-M Vector Tables...")
candidates = []
for idx in range(0, len(data) - 64, 4):
    words = struct.unpack('<16I', data[idx : idx + 64])
    sp = words[0]
    rv = words[1]
    nmi = words[2]
    hf = words[3]
    
    # Check if SP is in SRAM (0x20000000..0x20030000) and vectors are in Flash (0x08000000..0x08100000) with bit 0 set
    if (0x20000000 <= sp <= 0x20040000) and (0x08000000 <= rv <= 0x08200000) and (rv & 1) and (nmi & 1) and (hf & 1):
        print(f"Candidate Vector Table at container offset 0x{idx:X} ({idx}):")
        print(f"  Initial SP:     0x{sp:08X}")
        print(f"  Reset Vector:   0x{rv:08X}")
        print(f"  NMI Handler:    0x{nmi:08X}")
        print(f"  HardFault:      0x{hf:08X}")
        print(f"  MemManage:      0x{words[4]:08X}")
        print(f"  BusFault:       0x{words[5]:08X}")
        print(f"  UsageFault:     0x{words[6]:08X}")
        candidates.append((idx, sp, rv))

if not candidates:
    print("No direct vector table found with standard flash base. Checking other base addresses...")
    for idx in range(0, len(data) - 64, 4):
        words = struct.unpack('<16I', data[idx : idx + 64])
        sp = words[0]
        rv = words[1]
        if (0x20000000 <= sp <= 0x20040000) and (rv & 1) and (words[2] & 1) and (words[3] & 1):
            print(f"Potential RAM/custom vector table at 0x{idx:X}: SP=0x{sp:08X}, RV=0x{rv:08X}")
