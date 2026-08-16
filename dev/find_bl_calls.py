import struct

bin_path = r'C:\Users\hooki\trench-authoring\dev\disasm\vulcan_app_08020000.bin'
with open(bin_path, 'rb') as f:
    firmware = f.read()

base_addr = 0x08020000

def decode_thumb2_bl(pc, w1, w2):
    # w1 is first halfword (at pc), w2 is second halfword (at pc+2)
    # BL encoding:
    # w1: 1111 0S [imm10]
    # w2: 11 J1 1 J2 [imm11]
    if (w1 & 0xF800) == 0xF000 and (w2 & 0xD000) == 0xD000:
        S = (w1 >> 10) & 1
        imm10 = w1 & 0x3FF
        J1 = (w2 >> 13) & 1
        J2 = (w2 >> 11) & 1
        imm11 = w2 & 0x7FF
        
        I1 = ~(J1 ^ S) & 1
        I2 = ~(J2 ^ S) & 1
        
        imm32 = (S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1)
        # Sign extend 25-bit to 32-bit
        if S:
            imm32 -= (1 << 25)
            
        target = pc + 4 + imm32
        return target
    return None

# Find all BL calls to 0x08037000..0x08039000
print("=== DECODING ALL THUMB-2 BL CALLS IN FIRMWARE ===")
calls_to_dsp_region = []
for off in range(0, len(firmware) - 4, 2):
    pc = base_addr + off
    w1, w2 = struct.unpack('<HH', firmware[off : off + 4])
    tgt = decode_thumb2_bl(pc, w1, w2)
    if tgt is not None and 0x08037000 <= tgt <= 0x08039000:
        calls_to_dsp_region.append((pc, tgt))

print(f"Found {len(calls_to_dsp_region)} BL calls into DSP region [0x08037000..0x08039000]:")
for pc, tgt in calls_to_dsp_region:
    print(f"  Caller at 0x{pc:08X} ----BL----> 0x{tgt:08X}")
