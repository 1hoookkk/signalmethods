import struct, math

bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000

def u32_at(buf, offset):
    return struct.unpack_from('<I', buf, offset)[0]

def unpack_corner_36bytes(b):
    """
    Exact emulation of STM32 Cortex-M4 bit unpacker at 0x080382FC.
    Takes 36-byte packed corner slice.
    Returns 14 unpacked uint32 words (field0..field13).
    """
    words = [0] * 14
    
    # field0
    r5 = u32_at(b, 0)
    r3 = (r5 >> 1) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r5 >> 6) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[0] = (r3 + r2) & 0xFFFFFFFF
    
    # field1
    r5 = (u32_at(b, 0) << 21) & 0x7FE00000
    r6 = u32_at(b, 4)
    r2 = (r6 >> 11) & 0x100000
    r3 = (r5 + r2) & 0xFFFFFFFF
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r6 >> 16) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[1] = (r3 + r2) & 0xFFFFFFFF

    # field2
    r5 = u32_at(b, 4)
    r3 = (r5 << 11) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r5_val = (r5 << 6) & 0x7FC0
    r2 = (u32_at(b, 8) >> 26) & 0x30
    r2 = (r2 + r5_val) & 0xFFFFFFFF
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[2] = (r3 + r2) & 0xFFFFFFFF

    # field3
    r5 = u32_at(b, 8)
    r3 = (r5 << 1) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r5 >> 4) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[3] = (r3 + r2) & 0xFFFFFFFF

    # field4
    r5 = (u32_at(b, 8) << 23) & 0x7F800000
    r6 = u32_at(b, 12)
    r3 = (r6 >> 9) & 0x700000
    r3 = (r3 + r5) & 0xFFFFFFFF
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r6 >> 14) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[4] = (r3 + r2) & 0xFFFFFFFF

    # field5
    r3 = u32_at(b, 12)
    r2 = (r3 << 13) & 0x7FF00000
    r5 = 0xF0000 if r2 != 0 else 0
    r5 = (r5 + r2) & 0xFFFFFFFF
    r2 = (r3 << 8) & 0x7F00
    r3_byte = (u32_at(b, 16) >> 24) & 0xF0 if len(b) >= 20 else 0
    r3_sum = (r2 + r3_byte) & 0xFFFFFFFF
    r2 = (r5 + r3_sum) & 0xFFFFFFFF
    r3_flag = 0xF if r3_sum != 0 else 0
    words[5] = (r2 + r3_flag) & 0xFFFFFFFF

    # field6
    r5 = u32_at(b, 16)
    r3 = (r5 << 3) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r5 >> 2) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[6] = (r3 + r2) & 0xFFFFFFFF

    # field7
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

    # field8
    r3 = u32_at(b, 20)
    r2 = (r3 << 15) & 0x7FF00000
    r5 = 0xF0000 if r2 != 0 else 0
    r5 = (r5 + r2) & 0xFFFFFFFF
    r2 = (r3 << 10) & 0x7C00
    r3_val = (u32_at(b, 24) >> 22) & 0x3F0 if len(b) >= 28 else 0
    r3_sum = (r2 + r3_val) & 0xFFFFFFFF
    r2 = (r5 + r3_sum) & 0xFFFFFFFF
    r3_flag = 0xF if r3_sum != 0 else 0
    words[8] = (r2 + r3_flag) & 0xFFFFFFFF

    # field9
    r2 = u32_at(b, 24)
    r3 = (r2 << 5) & 0x7FF00000
    r5 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r5) & 0xFFFFFFFF
    r2_val = ((r2 & ~0xF) << 17) & 0xFFFFFFFF
    r2_val = r2_val >> 17
    r3 = (r3 + r2_val) & 0xFFFFFFFF
    r2_flag = 0xF if r2_val != 0 else 0
    words[9] = (r3 + r2_flag) & 0xFFFFFFFF

    # field10
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

    # field11
    r3 = u32_at(b, 28)
    r2 = (r3 << 17) & 0x7FF00000
    r5 = 0xF0000 if r2 != 0 else 0
    r5 = (r5 + r2) & 0xFFFFFFFF
    r2 = (r3 << 12) & 0x7000
    r3_val = (u32_at(b, 32) >> 20) & 0xFF0 if len(b) >= 36 else 0
    r3_sum = (r2 + r3_val) & 0xFFFFFFFF
    r2 = (r5 + r3_sum) & 0xFFFFFFFF
    r3_flag = 0xF if r3_sum != 0 else 0
    words[11] = (r2 + r3_flag) & 0xFFFFFFFF

    # field12
    r5 = u32_at(b, 32)
    r3 = (r5 << 7) & 0x7FF00000
    r2 = 0xF0000 if r3 != 0 else 0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = (r5 << 2) & 0x7FF0
    r3 = (r3 + r2) & 0xFFFFFFFF
    r2 = 0xF if r2 != 0 else 0
    words[12] = (r3 + r2) & 0xFFFFFFFF

    # field13
    r3 = (u32_at(b, 32) << 29) & 0x60000000
    # trailing bits
    words[13] = r3

    return words

def decode_seven_stages(unpacked_words):
    """
    Extracts all 7 stages: (theta_p, R_p, theta_z, R_z, a1, a2, b1, b2, g)
    """
    stages = []
    for s in range(7):
        w_p = unpacked_words[s*2] if s*2 < len(unpacked_words) else 0
        w_r = unpacked_words[s*2 + 1] if s*2 + 1 < len(unpacked_words) else 0
        
        # Pole angle theta_p
        exp_f = (w_p >> 26) & 0xF
        mant_f = ((w_p >> 15) & 0x7FF) | 0x800
        theta_p = (mant_f << exp_f) * (math.pi / (2**27))
        
        # Pole radius R_p
        exp_r = (w_r >> 26) & 0xF
        mant_r = ((w_r >> 15) & 0x7FF) | 0x800
        delta_r = (mant_r << exp_r) * (1.0 / (2**26))
        r_p = max(0.0, min(0.99999, 1.0 - delta_r))
        
        # Zero defaults (or paired conjugate zeros)
        theta_z = 0.0
        r_z = 0.0
        
        a1 = -2.0 * r_p * math.cos(theta_p)
        a2 = r_p * r_p
        b1 = -2.0 * r_z * math.cos(theta_z)
        b2 = r_z * r_z
        g = 1.0
        
        f0 = theta_p * 39062.5 / (2.0 * math.pi)
        stages.append({
            'stage': s + 1,
            'f0_hz': f0,
            'theta_p': theta_p,
            'r_p': r_p,
            'theta_z': theta_z,
            'r_z': r_z,
            'a1': a1,
            'a2': a2,
            'b1': b1,
            'b2': b2,
            'g': g
        })
    return stages

presets = [
    (0, "TalkingHedz", 0),
    (0, "TalkingHedz", 7),
    (1, "Vowel Morph", 0),
    (21, "AEParLPVow", 0),
]

for p_idx, name, corner in presets:
    record_offset = 0x08008000 + p_idx * 332 - FLASH_BASE
    c_bytes = app_bin[record_offset + 44 + corner * 36 : record_offset + 44 + (corner + 1) * 36]
    unpacked = unpack_corner_36bytes(c_bytes)
    stages = decode_seven_stages(unpacked)
    
    print("\n" + "="*85)
    print(f"PRESET {p_idx:03d} '{name}' CORNER {corner} — 7 STAGES DUMP:")
    print("="*85)
    print(f"{'Stg':>3} | {'f0 (Hz)':>8} | {'theta_p':>8} | {'R_p':>7} | {'a1':>9} | {'a2':>8} | {'b1':>8} | {'b2':>8} | {'g':>4}")
    print("-" * 85)
    for st in stages:
        print(f"{st['stage']:3d} | {st['f0_hz']:8.1f} | {st['theta_p']:8.5f} | {st['r_p']:7.5f} | {st['a1']:9.5f} | {st['a2']:8.5f} | {st['b1']:8.5f} | {st['b2']:8.5f} | {st['g']:4.1f}")
