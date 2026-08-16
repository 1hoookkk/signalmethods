import numpy as np

# In Morpheus, let's decode each word w as:
# exp = (w >> 12) & 0xF or (w >> 11) & 0xF
# mantissa = (w & 0x7FF) | 0x800

def word_to_val(w):
    exp = (w >> 11) & 0xF
    mant = (w & 0x7FF) | 0x800
    # Minifloat scalar: (mant * 2^exp) / 2^26 for radius/damping
    return float(mant * (1 << exp)) / (1 << 26)

def word_to_angle(w):
    exp = (w >> 11) & 0xF
    mant = (w & 0x7FF) | 0x800
    # Angle scalar: (mant * 2^exp) * pi / 2^27
    return float(mant * (1 << exp)) * np.pi / (1 << 27)

def decode_biquad(w0, w1, w2, w3, w4, sr=39062.5):
    # Damping / radius
    c1 = word_to_val(w1) # Zero damping: Rz^2 = 1 - c1
    c0 = 4.0 * word_to_val(w0) + c1
    c3 = word_to_val(w3) # Pole damping: Rp^2 = 1 - c3
    c2 = 4.0 * word_to_val(w2) + c3
    g = 4.0 * word_to_val(w4)
    
    # Biquad coefficients:
    # b0 = g
    # b1 = g * (c0 - 2)
    # b2 = g * (1 - c1)
    # a1 = c2 - 2
    # a2 = 1 - c3
    b = [g, g * (c0 - 2.0), g * (1.0 - c1)]
    a = [1.0, c2 - 2.0, 1.0 - c3]
    return b, a

# Let's test for Cube 0 Corner 0 Stage 1:
w = [0x12d5, 0xb550, 0x55dd, 0x775b, 0x75ff]
b, a = decode_biquad(*w)
print(f"Decoded Biquad Coeffs: b={b}, a={a}")

# Compute magnitude response at 1 kHz
w_rad = 2.0 * np.pi * 1000.0 / 39062.5
z = np.exp(1j * w_rad)
H = (b[0] + b[1]*z**-1 + b[2]*z**-2) / (a[0] + a[1]*z**-1 + a[2]*z**-2)
print(f"Magnitude at 1kHz: {20*np.log10(np.abs(H)):.2f} dB")
