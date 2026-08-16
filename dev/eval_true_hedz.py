import json, math

with open('recipes/architectures/P2k_013_TalkingHedz.json') as fp:
    hedz = json.load(fp)

sr = hedz['datum_sr_hz']
sections = hedz['sections']

print("="*85)
print("TALKING HEDZ — CORNER M0_Q0 EXACT GEOMETRY & BIQUAD COEFFICIENTS:")
print("="*85)
print(f"{'Stg':>3} | {'Pole (Hz)':>10} | {'Pole R':>7} | {'Zero (Hz)':>10} | {'Zero R':>7} | {'Scale':>7} | {'a1':>9} | {'a2':>8}")
print("-" * 85)

biquads = []
for si, sec in enumerate(sections):
    c = sec['corners']['M0_Q0']
    p_hz = c['pole']['hz']
    p_r = c['pole']['r']
    z_hz = c['zero']['hz']
    z_r = c['zero']['r']
    scale = c['scale']
    
    # 0x080378D0 FPU calculation
    theta_p = 2.0 * math.pi * p_hz / sr
    theta_z = 2.0 * math.pi * z_hz / sr
    
    a1 = -2.0 * p_r * math.cos(theta_p)
    a2 = p_r * p_r
    
    b0 = scale
    b1 = -2.0 * scale * z_r * math.cos(theta_z)
    b2 = scale * z_r * z_r
    
    biquads.append({'a1': a1, 'a2': a2, 'b0': b0, 'b1': b1, 'b2': b2})
    print(f"{si+1:3d} | {p_hz:10.1f} | {p_r:7.5f} | {z_hz:10.1f} | {z_r:7.5f} | {scale:7.4f} | {a1:9.5f} | {a2:8.5f}")

def eval_biquad_db(b, hz):
    omega = 2.0 * math.pi * hz / sr
    cos_w = math.cos(omega)
    cos_2w = math.cos(2.0 * omega)
    sin_w = math.sin(omega)
    sin_2w = math.sin(2.0 * omega)
    
    num_re = b['b0'] + b['b1'] * cos_w + b['b2'] * cos_2w
    num_im = -(b['b1'] * sin_w + b['b2'] * sin_2w)
    
    den_re = 1.0 + b['a1'] * cos_w + b['a2'] * cos_2w
    den_im = -(b['a1'] * sin_w + b['a2'] * sin_2w)
    
    mag2 = (num_re**2 + num_im**2) / (den_re**2 + den_im**2)
    return 10.0 * math.log10(mag2)

test_freqs = [100, 200, 346, 500, 890, 1000, 2000, 3260, 5000, 9320, 15000]
print("\n" + "="*85)
print("TALKING HEDZ M0_Q0 — 7-STAGE MAGNITUDE RESPONSE (dB):")
print("="*85)
for tf in test_freqs:
    resp = sum(eval_biquad_db(b, tf) for b in biquads)
    print(f"  {tf:5d} Hz: {resp:7.2f} dB")
