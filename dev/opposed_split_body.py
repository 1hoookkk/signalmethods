import struct, sys, math
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl

SR = 39062.5

def compiled(v):
    return ((v >> 1) + 0x6400) & 0xFFFF

def rad_byte(freq_byte):
    return ((freq_byte * 0x7C) >> 8) + 0x76

def clamp(x):
    return max(0, min(255, x))

def encode_word(value):
    if value >= 1.0: return 0xFFFF
    if value <= 0.0: return 0x0000
    dm = round(value * 134217728.0)
    if 0 < dm <= 0xFFF: return (dm - 1) & 0xFFFF
    e = min(int(math.floor(math.log2(value))) + 1, 0)
    if e < -14: return 0
    m = round(value / 2.0**(e - 13))
    if m >= 0x2000:
        e += 1
        m = round(value / 2.0**(e - 13))
    return ((((e + 15) << 12) | max(0, min(0xFFF, m - 0x1000))) - 1) & 0xFFFF

def mag_word(hz, rsq_word):
    d_rsq = dl.minifloat_decode(rsq_word)
    p = -2.0 * math.sqrt(max(1.0 - d_rsq, 0.0)) * math.cos(2 * math.pi * hz / SR)
    return encode_word((p + 2.0 - d_rsq) / 4.0)

freq_bytes = [16, 32, 56, 88, 128, 176]
gain = int(sys.argv[1]) if len(sys.argv) > 1 else 24

words = []
for fb in freq_bytes:
    rad = rad_byte(fb)
    w1 = clamp(rad + gain) << 8
    w3 = clamp(rad - gain) << 8
    hz = fb / 256.0 * (SR / 2.0)
    pole_rsq = compiled(w1)
    zero_rsq = compiled(w3)
    pm = mag_word(hz, pole_rsq)
    zm = mag_word(hz, zero_rsq)
    row = [zm, zero_rsq, pm, pole_rsq, 0]
    words.append(row)

ratio = 1.0
for row in words:
    for is_pole in (False, True):
        m, r = (row[2], row[3]) if is_pole else (row[0], row[1])
        d_rsq = dl.minifloat_decode(r)
        p = 4.0 * dl.minifloat_decode(m) - 2.0 + d_rsq
        q = 1.0 - d_rsq
        term = abs(1.0 + p + q)
        ratio = ratio * term if is_pole else ratio / max(term, 1e-12)
scale = ratio ** (1.0 / 6.0)
best, err = 0, 1e9
for w in range(0, 65536, 3):
    e = abs(dl.minifloat_decode(w) - scale / 4.0)
    if e < err: err, best = e, w
for row in words: row[4] = best

flat = []
for _ in range(4):
    for row in words: flat.extend(row)
open('dev/e2e/opposed_split.body240', 'wb').write(struct.pack('<120H', *flat))
for i, (fb, row) in enumerate(zip(freq_bytes, words)):
    print(f"S{i+1} freq_byte {fb:3d} rad {rad_byte(fb):3d} pole_rsq {row[3]:#06x} zero_rsq {row[1]:#06x}")
print("wrote dev/e2e/opposed_split.body240, gain =", gain)
