import json, math

with open('C:/Users/hooki/trench-x3-clean/dev/reference/p2k_vowel_isolated_sections_44100.json') as fp:
    vowels = json.load(fp)

preset = vowels['P2k_010_ooh_to_eee']['M0_Q0']
sr = 44100.0

print("="*90)
print("P2K_010 OOH-TO-EEE (CORNER M0_Q0 @ 44.1 kHz) — GROUND TRUTH REFERENCE:")
print("="*90)

biquads = []
for si, sec in enumerate(preset['sections']):
    p_hz = sec['pole_hz']
    p_r = sec['pole_r']
    z_hz = sec['zero_hz']
    z_r = sec['zero_r']
    g_db = sec['gain_db']
    g = 10.0 ** (g_db / 20.0)
    
    theta_p = 2.0 * math.pi * p_hz / sr
    theta_z = 2.0 * math.pi * z_hz / sr
    
    a1 = -2.0 * p_r * math.cos(theta_p)
    a2 = p_r * p_r
    
    b0 = g
    b1 = -2.0 * g * z_r * math.cos(theta_z)
    b2 = g * z_r * z_r
    
    biquads.append({
        'stage': si + 1,
        'f_p': p_hz, 'r_p': p_r,
        'f_z': z_hz, 'r_z': z_r,
        'b0': b0, 'b1': b1, 'b2': b2,
        'a1': a1, 'a2': a2
    })

print(f"{'Stg':>3} | {'b0':>10} | {'b1':>10} | {'b2':>10} | {'a1':>10} | {'a2':>10}")
print("-" * 90)
for b in biquads:
    print(f"{b['stage']:3d} | {b['b0']:10.6f} | {b['b1']:10.6f} | {b['b2']:10.6f} | {b['a1']:10.6f} | {b['a2']:10.6f}")

def eval_biquad(b, hz):
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

test_points = [100, 300, 610.9, 884.3, 958.4, 1577.3, 2431.8, 2682.1, 3207.4, 4089.1, 4935.7, 10000]
print("\n" + "="*90)
print("EXACT MAGNITUDE RESPONSE AT KEY FORMANT PEAKS & TRANSMISSION ZERO NOTCHES:")
print("="*90)
for tf in test_points:
    resp = sum(eval_biquad(b, tf) for b in biquads)
    print(f"  {tf:7.1f} Hz: {resp:7.2f} dB")
