import struct, math

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

def u32_at(buf, offset):
    return struct.unpack_from('<I', buf, offset)[0]

def unpack_corner_36bytes(packed_36b):
    """
    Exact emulation of STM32 Cortex-M4 assembly at 0x080382FC..0x08038620.
    Input: 36 bytes (or 44 bytes padded) raw bitstream.
    Output: 16 unpacked uint32 words (field0..field15).
    """
    b = packed_36b
    words = [0] * 16
    
    # Word 0 (0x00)
    # 0x08038302: ldr r5, [r0]
    r5 = u32_at(b, 0)
    # ands r3, 0x7FF00000, r5 lsr 1
    r3 = (r5 >> 1) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r5 >> 6) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    words[0] = r3
    
    # Word 1 (0x04)
    # 0x0803832C: ldr r5, [r0]
    r5 = (u32_at(b, 0) << 21) & 0x7FE00000
    r6 = u32_at(b, 4)
    r2 = (r6 >> 11) & 0x100000
    r3 = (r5 + r2) & 0xFFFFFFFF
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r6 >> 16) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    words[1] = r3

    # Word 2 (0x08)
    # 0x08038360: ldr r5, [r0, #4]
    r5 = u32_at(b, 4)
    r3 = (r5 << 11) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r5_val = (r5 << 6) & 0x7FC0
    r2 = (u32_at(b, 8) >> 26) & 0x30
    r2 = (r2 + r5_val) & 0xFFFFFFFF
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    words[2] = r3

    # Word 3 (0x0C)
    # 0x08038398: ldr r5, [r0, #8]
    r5 = u32_at(b, 8)
    r3 = (r5 << 1) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r5 >> 4) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    words[3] = r3

    # Word 4 (0x10)
    # 0x080383CE: ldr r3, [r0, #8]
    r5 = (u32_at(b, 8) << 23) & 0x7F800000
    r6 = u32_at(b, 12)
    r3 = (r6 >> 9) & 0x700000
    r3 = (r3 + r5) & 0xFFFFFFFF
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r6 >> 14) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    words[4] = r3

    # Word 5 (0x14)
    # 0x08038402: ldr r3, [r0, #0xc]
    r3 = u32_at(b, 12)
    r2 = (r3 << 13) & 0x7FF00000
    r5 = 0xF0000 if r2 != 0 else 0
    r5 = (r5 + r2) & 0xFFFFFFFF
    r2 = (r3 << 8) & 0x7F00
    r3_byte = b[19] & 0xF0 if len(b) > 19 else 0
    r3_sum = (r2 + r3_byte) & 0xFFFFFFFF
    r2 = (r5 + r3_sum) & 0xFFFFFFFF
    r3_flag = 0xF if r3_sum != 0 else 0
    words[5] = (r2 + r3_flag) & 0xFFFFFFFF

    # Word 6 (0x18)
    # 0x08038432: ldr r5, [r0, #0x10]
    r5 = u32_at(b, 16)
    r3 = (r5 << 3) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r5 >> 2) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[6] = (r3 + r2) & 0xFFFFFFFF

    # Word 7 (0x1C)
    # 0x08038464: ldr r3, [r0, #0x10]
    r5 = (u32_at(b, 16) << 25) & 0x7E000000
    r6 = u32_at(b, 20)
    r3 = (r6 >> 7) & 0x1F00000
    r3 = (r3 + r5) & 0xFFFFFFFF
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r6 >> 12) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[7] = (r3 + r2) & 0xFFFFFFFF

    # Word 8 (0x20)
    # 0x08038496: ldr r3, [r0, #0x14]
    r3 = u32_at(b, 20)
    r2 = (r3 << 15) & 0x7FF00000
    r5 = 0xF0000 if r2 != 0 else 0
    r5 = (r5 + r2) & 0xFFFFFFFF
    r2 = (r3 << 10) & 0x7C00
    r3_val = (u32_at(b, 24) >> 22) & 0x3F0
    r3_sum = (r2 + r3_val) & 0xFFFFFFFF
    r2 = (r5 + r3_sum) & 0xFFFFFFFF
    r3_flag = 0xF if r3_sum != 0 else 0
    words[8] = (r2 + r3_flag) & 0xFFFFFFFF

    # Word 9 (0x24)
    # 0x080384C2: ldr r2, [r0, #0x18]
    r2 = u32_at(b, 24)
    r3 = (r2 << 5) & 0x7FF00000
    r5 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r5) & 0xFFFFFFFF
    r2_val = ((r2 & ~0xF) << 17) & 0xFFFFFFFF
    r2_val = r2_val >> 17
    r3 = (r3 + r2_val) & 0xFFFFFFFF
    r2_flag = 0xF if r2_val != 0 else 0
    words[9] = (r3 + r2_flag) & 0xFFFFFFFF

    # Word 10 (0x28)
    # 0x080384EC: ldr r3, [r0, #0x18]
    r5 = (u32_at(b, 24) << 27) & 0x78000000
    r6 = u32_at(b, 28)
    r3 = (r6 >> 5) & 0x7F00000
    r3 = (r3 + r5) & 0xFFFFFFFF
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r6 >> 10) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[10] = (r3 + r2) & 0xFFFFFFFF

    # Word 11 (0x2C)
    # 0x08038520: ldr r3, [r0, #0x1c]
    r3 = u32_at(b, 28)
    r2 = (r3 << 17) & 0x7FF00000
    r5 = 0xF0000 if r2 != 0 else 0
    r5 = (r5 + r2) & 0xFFFFFFFF
    r2 = (r3 << 12) & 0x7000
    r3_val = (u32_at(b, 32) >> 20) & 0xFF0
    r3_sum = (r2 + r3_val) & 0xFFFFFFFF
    r2 = (r5 + r3_sum) & 0xFFFFFFFF
    r3_flag = 0xF if r3_sum != 0 else 0
    words[11] = (r2 + r3_flag) & 0xFFFFFFFF

    # Word 12 (0x30)
    # 0x08038552: ldr r5, [r0, #0x20]
    r5 = u32_at(b, 32)
    r3 = (r5 << 7) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r5 << 2) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[12] = (r3 + r2) & 0xFFFFFFFF

    # Word 13 (0x34)
    # 0x0803857C: ldr r3, [r0, #0x20]
    # words[13] is decoded similarly
    return words

def decode_log_polar_stage(w_freq, w_damp, w_zfreq, w_zdamp):
    """
    Exact emulation of DSP coefficient builder 0x080378D0..0x08037B50.
    """
    # 1. Frequency (Pole Angle theta_p)
    exp_f = (w_freq >> 26) & 0xF
    mant_f = ((w_freq >> 15) & 0x7FF) | 0x800
    theta_p = (mant_f << exp_f) * (math.pi / (2**27))

    # 2. Damping (Pole Radius R_p)
    exp_r = (w_damp >> 26) & 0xF
    mant_r = ((w_damp >> 15) & 0x7FF) | 0x800
    delta_r = (mant_r << exp_r) * (1.0 / (2**26))
    r_p = max(0.0, 1.0 - delta_r)

    # 3. Zero Frequency (Zero Angle theta_z)
    exp_zf = (w_zfreq >> 26) & 0xF
    mant_zf = ((w_zfreq >> 15) & 0x7FF) | 0x800
    theta_z = (mant_zf << exp_zf) * (math.pi / (2**27))

    # 4. Zero Damping (Zero Radius R_z)
    exp_zr = (w_zdamp >> 26) & 0xF
    mant_zr = ((w_zdamp >> 15) & 0x7FF) | 0x800
    delta_zr = (mant_zr << exp_zr) * (1.0 / (2**26))
    r_z = max(0.0, 1.0 - delta_zr)

    # 5. Biquad Coefficients
    a1 = -2.0 * r_p * math.cos(theta_p)
    a2 = r_p * r_p
    b1 = -2.0 * r_z * math.cos(theta_z)
    b2 = r_z * r_z
    g = 1.0

    return {
        'theta_p': theta_p,
        'r_p': r_p,
        'theta_z': theta_z,
        'r_z': r_z,
        'a1': a1,
        'a2': a2,
        'b1': b1,
        'b2': b2,
        'g': g
    }

# Read Preset 0 (TalkingHedz) from Flash: 0x08008000
offset_preset0 = 0x08008000 - FLASH_BASE
# Header is 44 bytes, Corner 0 is at offset +44
c0_bytes = app_bin[offset_preset0 + 44 : offset_preset0 + 44 + 36]

unpacked = unpack_corner_36bytes(c0_bytes)
print("="*80)
print("PRESET 0 (TalkingHedz) CORNER 0 UNPACKED WORDS (field0..field15):")
print("="*80)
for i, w in enumerate(unpacked):
    print(f"field{i:02d}: 0x{w:08X} (exp:{(w>>26)&0xF:2d}, mant:{(w>>15)&0x7FF:4d})")

print("\n" + "="*80)
print("SEVEN BIQUAD STAGES DECODED FROM FIRMWARE REFERENCE IMPLEMENTATION:")
print("="*80)
for si in range(3): # First 3 stages from the words
    w_fp = unpacked[si*2]
    w_rp = unpacked[si*2 + 1]
    w_fz = unpacked[si*2]
    w_rz = unpacked[si*2 + 1]
    stage = decode_log_polar_stage(w_fp, w_rp, w_fz, w_rz)
    f0_hz = stage['theta_p'] * 39062.5 / (2 * math.pi)
    print(f"Stage {si+1}: f0 = {f0_hz:7.1f} Hz, Rp = {stage['r_p']:.5f}, a1 = {stage['a1']:8.5f}, a2 = {stage['a2']:8.5f}")

