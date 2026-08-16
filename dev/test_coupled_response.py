import math, struct

def eval_coupled_state_space_response(f0_hz, r_p, sr=39062.5):
    """
    Computes exact frequency response of Rossum Coupled State-Space Section:
    w1[n+1] = (R cos theta) w1[n] - (R sin theta) w2[n] + (1 - R) x[n]
    w2[n+1] = (R sin theta) w1[n] + (R cos theta) w2[n]
    y[n] = w2[n]
    """
    theta = 2.0 * math.pi * f0_hz / sr
    c_cos = r_p * math.cos(theta)
    c_sin = r_p * math.sin(theta)
    g_in = 1.0 - r_p
    
    # State-space transfer function: H(z) = C (zI - A)^(-1) B
    # A = [[c_cos, -c_sin], [c_sin, c_cos]], B = [[g_in], [0]], C = [0, 1]
    # det(zI - A) = (z - c_cos)^2 + c_sin^2 = z^2 - 2 (R cos theta) z + R^2
    # Numerator of w2(z): g_in * c_sin * z^(-1)
    
    def H_db(hz):
        omega = 2.0 * math.pi * hz / sr
        z = complex(math.cos(omega), math.sin(omega))
        
        den = z**2 - 2.0 * c_cos * z + (r_p**2)
        num = g_in * c_sin * z
        
        h = num / den if abs(den) > 1e-12 else 1.0
        return 20.0 * math.log10(max(1e-6, abs(h)))
    
    return H_db

# Preset 0 Corner 0 stages:
stages_f0 = [86.9, 8.4, 0.3, 314.4, 2629.8, 16.8, 1982.5]
stages_r = [0.99613, 0.99890, 0.99305, 0.99994, 0.99195, 0.82043, 0.99219]

sr = 39062.5
test_freqs = [40, 100, 200, 300, 500, 1000, 2000, 3000, 5000, 10000]

print("="*80)
print("7-STAGE COUPLED STATE-SPACE CASCADE MAGNITUDE RESPONSE (dB):")
print("="*80)
for tf in test_freqs:
    tot_db = 0.0
    for f0, r in zip(stages_f0, stages_r):
        tot_db += eval_coupled_state_space_response(f0, r, sr)(tf)
    print(f"  {tf:5d} Hz: {tot_db:7.2f} dB")
