import math

def eval_firmware_biquad(f0_hz, r_p, sr=39062.5):
    theta = 2.0 * math.pi * f0_hz / sr
    cos_t = math.cos(theta)
    sin_t = math.sin(theta)
    
    # 0x080379F2: s3 = cos(theta) / sin(theta) = cot(theta)
    # 0x08037A88: s0 = 1 + r_p^2 - 2 r_p cos(theta)
    s3 = cos_t / sin_t if abs(sin_t) > 1e-6 else 1.0
    s0 = 1.0 + (r_p**2) - 2.0 * r_p * cos_t
    
    def H_db(hz):
        omega = 2.0 * math.pi * hz / sr
        z = complex(math.cos(omega), math.sin(omega))
        
        # Coupled transfer function to output w2 * s3:
        # H(z) = s0 * s3 * (r_p * sin_t * z) / (z^2 - 2 r_p cos_t z + r_p^2)
        #      = s0 * (r_p * cos_t * z) / (z^2 - 2 r_p cos_t z + r_p^2)
        num = s0 * (r_p * cos_t) * z
        den = z**2 - 2.0 * r_p * cos_t * z + (r_p**2)
        
        h = num / den if abs(den) > 1e-12 else 1.0
        return 20.0 * math.log10(max(1e-6, abs(h)))
        
    return H_db

# Preset 0 Corner 0 stages:
stages_f0 = [86.9, 8.4, 0.3, 314.4, 2629.8, 16.8, 1982.5]
stages_r = [0.99613, 0.99890, 0.99305, 0.99994, 0.99195, 0.82043, 0.99219]

sr = 39062.5
test_freqs = [40, 100, 200, 300, 500, 1000, 2000, 3000, 5000, 10000]

print("="*80)
print("EXACT FIRMWARE DISASSEMBLED TRANSFER FUNCTION MAGNITUDE (dB):")
print("="*80)
for tf in test_freqs:
    tot_db = sum(eval_firmware_biquad(f0, r, sr)(tf) for f0, r in zip(stages_f0, stages_r))
    print(f"  {tf:5d} Hz: {tot_db:7.2f} dB")
