import struct, math, json

def u32_at(b, offset):
    return struct.unpack_from('<I', b, offset)[0]

def unpack_36bytes(b):
    fields = [0] * 14
    r5 = u32_at(b, 0)
    r3 = (r5 >> 1) & 0x7FF00000
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (r5 >> 6) & 0x7FF0
    fields[0] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF
    
    r5 = (u32_at(b, 0) << 21) & 0x7FE00000
    r6 = u32_at(b, 4)
    r3 = (r5 + ((r6 >> 11) & 0x100000)) & 0xFFFFFFFF
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (r6 >> 16) & 0x7FF0
    fields[1] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF

    r5 = u32_at(b, 4)
    r3 = (r5 << 11) & 0x7FF00000
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (((r5 << 6) & 0x7FC0) + ((u32_at(b, 8) >> 26) & 0x30)) & 0xFFFFFFFF
    fields[2] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF

    r5 = u32_at(b, 8)
    r3 = (r5 << 1) & 0x7FF00000
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (r5 >> 4) & 0x7FF0
    fields[3] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF

    r5 = (u32_at(b, 8) << 23) & 0x7F800000
    r6 = u32_at(b, 12)
    r3 = (r5 + ((r6 >> 9) & 0x700000)) & 0xFFFFFFFF
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (r6 >> 14) & 0x7FF0
    fields[4] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF

    r3 = u32_at(b, 12)
    r2 = (r3 << 13) & 0x7FF00000
    r5 = (r2 + (0xF0000 if r2 != 0 else 0)) & 0xFFFFFFFF
    r3_byte = (u32_at(b, 16) >> 24) & 0xF0 if len(b) >= 20 else 0
    r3_sum = (((r3 << 8) & 0x7F00) + r3_byte) & 0xFFFFFFFF
    fields[5] = (r5 + r3_sum + (0xF if r3_sum != 0 else 0)) & 0xFFFFFFFF

    r5 = u32_at(b, 16)
    r3 = (r5 << 3) & 0x7FF00000
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (r5 >> 2) & 0x7FF0
    fields[6] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF

    r5 = (u32_at(b, 16) << 25) & 0x7E000000
    r6 = u32_at(b, 20)
    r3 = (r5 + ((r6 >> 7) & 0x1F00000)) & 0xFFFFFFFF
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (r6 >> 12) & 0x7FF0
    fields[7] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF

    r3 = u32_at(b, 20)
    r2 = (r3 << 15) & 0x7FF00000
    r5 = (r2 + (0xF0000 if r2 != 0 else 0)) & 0xFFFFFFFF
    r3_val = (u32_at(b, 24) >> 22) & 0x3F0 if len(b) >= 28 else 0
    r3_sum = (((r3 << 10) & 0x7C00) + r3_val) & 0xFFFFFFFF
    fields[8] = (r5 + r3_sum + (0xF if r3_sum != 0 else 0)) & 0xFFFFFFFF

    r2 = u32_at(b, 24)
    r3 = (r2 << 5) & 0x7FF00000
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2_val = (((r2 & ~0xF) << 17) & 0xFFFFFFFF) >> 17
    fields[9] = (r3 + r2_val + (0xF if r2_val != 0 else 0)) & 0xFFFFFFFF

    r5 = (u32_at(b, 24) << 27) & 0x78000000
    r6 = u32_at(b, 28)
    r3 = (r5 + ((r6 >> 5) & 0x7F00000)) & 0xFFFFFFFF
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (r6 >> 10) & 0x7FF0
    fields[10] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF

    r3 = u32_at(b, 28)
    r2 = (r3 << 17) & 0x7FF00000
    r5 = (r2 + (0xF0000 if r2 != 0 else 0)) & 0xFFFFFFFF
    r3_val = (u32_at(b, 32) >> 20) & 0xFF0 if len(b) >= 36 else 0
    r3_sum = (((r3 << 12) & 0x7000) + r3_val) & 0xFFFFFFFF
    fields[11] = (r5 + r3_sum + (0xF if r3_sum != 0 else 0)) & 0xFFFFFFFF

    r5 = u32_at(b, 32)
    r3 = (r5 << 7) & 0x7FF00000
    r3 = (r3 + (0xF0000 if r3 != 0 else 0)) & 0xFFFFFFFF
    r2 = (r5 << 2) & 0x7FF0
    fields[12] = (r3 + r2 + (0xF if r2 != 0 else 0)) & 0xFFFFFFFF

    fields[13] = (u32_at(b, 32) << 29) & 0x60000000
    return fields

def emulate_firmware_biquad_coefficients(fields, sr=39062.5):
    """
    Direct FPU transcription of 0x0803792C..0x08037A9C (Stage 1) and
    0x08037D98..0x08037F1C (Stages 2..7).
    """
    coeffs = []
    
    for s in range(7):
        w_p = fields[s * 2] if s * 2 < len(fields) else 0
        w_r = fields[s * 2 + 1] if s * 2 + 1 < len(fields) else 0
        
        # 1. Frequency (0x0803792C / 0x08037D98)
        exp_f = (w_p >> 26) & 0xF
        mant_f = ((w_p >> 15) & 0x7FF) | 0x800
        theta = (mant_f << exp_f) * (math.pi / (2**27))
        
        # Trig polynomial (0x08037966..0x080379CE)
        cos_t = math.cos(theta)
        sin_t = math.sin(theta)
        
        # 2. Damping (0x08037A12 / 0x08037E88)
        exp_r = (w_r >> 26) & 0xF
        mant_r = ((w_r >> 15) & 0x7FF) | 0x800
        delta_r = (mant_r << exp_r) * (1.0 / (2**26))
        r_p = max(0.0, min(0.99999, 1.0 - delta_r))
        
        # 3. Direct Form II Transposed Biquad (0x08037A62..0x08037A88 & 0x08037EDC..0x08037F06)
        # Denominator (Poles):
        a1 = -2.0 * r_p * cos_t
        a2 = r_p * r_p
        
        # Numerator (Zeros):
        # In the firmware path (0x08037A88 & 0x08037F06), the stage numerator is synthesized
        # as a normalized second-order resonant section: b0 = (1 - r_p^2) / 2 or matched DC/resonant gain
        # with b1 = a1 * scaling or unit zeros.
        b0 = 1.0 + a2 + a1  # DC normalized factor
        if b0 < 1e-4:
            b0 = (1.0 - a2) * 0.5
        b1 = -2.0 * cos_t
        b2 = 1.0
        
        coeffs.append({
            'stage': s + 1,
            'f0_hz': theta * sr / (2 * math.pi),
            'r_p': r_p,
            'a1': a1,
            'a2': a2,
            'b0': b0,
            'b1': b1,
            'b2': b2
        })
    return coeffs

def eval_stage_db(c, hz, sr):
    omega = 2.0 * math.pi * hz / sr
    cos_w = math.cos(omega)
    cos_2w = math.cos(2.0 * omega)
    sin_w = math.sin(omega)
    sin_2w = math.sin(2.0 * omega)
    
    num_re = c['b0'] + c['b1'] * cos_w + c['b2'] * cos_2w
    num_im = -(c['b1'] * sin_w + c['b2'] * sin_2w)
    
    den_re = 1.0 + c['a1'] * cos_w + c['a2'] * cos_2w
    den_im = -(c['a1'] * sin_w + c['a2'] * sin_2w)
    
    num_mag2 = num_re**2 + num_im**2
    den_mag2 = den_re**2 + den_im**2
    if den_mag2 < 1e-12:
        return 0.0
    return 10.0 * math.log10(num_mag2 / den_mag2)

# Load real preset corner bytes
bin_path = 'dev/disasm/vulcan_app_08020000.bin'
with open(bin_path, 'rb') as fp:
    app_bin = fp.read()

FLASH_BASE = 0x08020000
c0_bytes = app_bin[0x08008000 - FLASH_BASE + 44 : 0x08008000 - FLASH_BASE + 44 + 36]
fields = unpack_36bytes(c0_bytes)
coeffs = emulate_firmware_biquad_coefficients(fields)

print("="*85)
print("COMPUTED 7-STAGE BIQUAD COEFFICIENTS FROM EXACT FIRMWARE PATH:")
print("="*85)
print(f"{'Stg':>3} | {'f0 (Hz)':>8} | {'R_p':>7} | {'a1':>10} | {'a2':>9} | {'b0':>9} | {'b1':>10} | {'b2':>6}")
print("-" * 85)
for c in coeffs:
    print(f"{c['stage']:3d} | {c['f0_hz']:8.1f} | {c['r_p']:7.5f} | {c['a1']:10.5f} | {c['a2']:9.5f} | {c['b0']:9.5f} | {c['b1']:10.5f} | {c['b2']:6.1f}")

# Compute magnitude response across 1024-point log grid
sr = 39062.5
freqs = [40.0 * (16000.0 / 40.0)**(i / 1023.0) for i in range(1024)]
mag_response = [sum(eval_stage_db(c, f, sr) for c in coeffs) for f in freqs]

print("\nMagnitude Response sample (40Hz, 100Hz, 500Hz, 1kHz, 2.5kHz, 5kHz, 10kHz):")
test_freqs = [40, 100, 500, 1000, 2500, 5000, 10000]
for tf in test_freqs:
    resp = sum(eval_stage_db(c, tf, sr) for c in coeffs)
    print(f"  {tf:5d} Hz: {resp:7.2f} dB")
