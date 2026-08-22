"""Does our packed-word interpolation match the device's?"""
import random

def s16(v):
    v &= 0xFFFF
    return v - 65536 if v >= 32768 else v

def ours(a, b, frac):
    """trench-native packed_body.cpp interpolate_word / x3 minifloat.rs lerp_u16"""
    diff = float(b - a)
    d32 = int(diff * frac)
    d16 = s16(d32)                      # <-- truncation to int16
    return (a + d16) & 0xFFFF

def device(a, b, frac, signed):
    """Q15 weights, SMUAD, >>15, negate -- as decoded from the firmware."""
    w_hi = -int(round(frac * 32768.0))
    w_lo = -32768 - w_hi
    A = s16(a) if signed else a
    B = s16(b) if signed else b
    acc = w_lo * A + w_hi * B
    return (-(acc >> 15)) & 0xFFFF

rng = random.Random(7)
pool = [rng.randrange(65536) for _ in range(3000)] + [0xDFFF, 0xFFFF, 0x01F0, 0x0000] * 40
fracs = [0.0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1.0]

for label, signed in (("device, signed int16 words", True), ("device, unsigned words", False)):
    nbad = worst = 0; ex = []
    N = 60000
    for _ in range(N):
        a = rng.choice(pool); b = rng.choice(pool); f = rng.choice(fracs)
        o = ours(a, b, f); d = device(a, b, f, signed)
        if o != d:
            nbad += 1
            e = min(abs(o - d), 65536 - abs(o - d))
            if e > worst:
                worst = e
                if len(ex) < 5: ex.append((a, b, f, o, d, e))
    print(f"{label:28} mismatch {100*nbad/N:6.2f}%   worst |error| {worst}")
    for a, b, f, o, d, e in ex[:3]:
        print(f"     a=0x{a:04X} b=0x{b:04X} f={f:<6} ours=0x{o:04X} dev=0x{d:04X}  off by {e}")
    print()

print("=== how often does OUR int16 truncation wrap? ===")
wrap = 0; N = 60000
for _ in range(N):
    a = rng.choice(pool); b = rng.choice(pool); f = rng.choice(fracs)
    d32 = int(float(b - a) * f)
    if d32 < -32768 or d32 > 32767: wrap += 1
print(f"   delta outside int16 range: {100*wrap/N:.2f}% of draws")
print("   when it wraps, a large step becomes a step of the OPPOSITE sign")

