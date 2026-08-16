import os, glob, struct, json
from collections import defaultdict, Counter
import numpy as np

TAU = 2.0 * np.pi
COMBINE_K = 4.0
SR_39K = 39062.5
IDENTITY_WORDS = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)

def decode_u16(word):
    u = int(word) + 1
    if u == 65536: return 1.0
    if u == 1: return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    return (m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0) * (2.0 ** (e - 15))

def pair_geometry_at(d_mag, d_rsq, sr=39062.5):
    q = 1.0 - d_rsq
    c = 4.0 * d_mag + d_rsq
    p = c - 2.0
    if abs(p) < 1e-12 and abs(q) < 1e-12:
        return {'type': 'Degenerate', 'hz': 0.0, 'r': 0.0}
    disc = p * p - 4.0 * q
    if disc < 0.0:
        r = np.sqrt(max(0.0, q))
        cos_w = np.clip(-p / (2.0 * r) if r > 0 else 0.0, -1.0, 1.0)
        hz = np.arccos(cos_w) / TAU * sr
        return {'type': 'Conjugate', 'hz': float(hz), 'r': float(r)}
    else:
        s = np.sqrt(disc)
        return {'type': 'RealPair', 'root_a': float((-p + s) / 2.0), 'root_b': float((-p - s) / 2.0)}

def decode_stage(words, sr=39062.5):
    d0, d1, d2, d3, d4 = [decode_u16(w) for w in words]
    zero = pair_geometry_at(d0, d1, sr)
    pole = pair_geometry_at(d2, d3, sr)
    scale = COMBINE_K * d4
    return zero, pole, scale

p2k_files = sorted(glob.glob('ref/presets/*.bin'))
presets = {}
for pf in p2k_files:
    name = os.path.splitext(os.path.basename(pf))[0]
    data = open(pf, 'rb').read()
    words = struct.unpack('<' + 'H' * 120, data)
    corners = {}
    for ci, cname in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
        stages = [tuple(words[(ci*6 + si)*5 : (ci*6 + si + 1)*5]) for si in range(6)]
        corners[cname] = stages
    presets[name] = corners

print(f"Loaded {len(presets)} P2k presets.")

# 1. Exact Section Matches across distinct presets
sec_to_loc = defaultdict(list)
for pname, corners in presets.items():
    for cname, stages in corners.items():
        for si, sw in enumerate(stages):
            if sw != IDENTITY_WORDS:
                sec_to_loc[sw].append((pname, cname, si + 1))

multi_presets_sec = {}
for sw, locs in sec_to_loc.items():
    distinct_p = set(l[0] for l in locs)
    if len(distinct_p) > 1:
        multi_presets_sec[sw] = locs

print(f"\nTotal active sections in P2k: {len(sec_to_loc)}")
print(f"Sections shared across 2 or more distinct P2k presets: {len(multi_presets_sec)}")

sorted_multi = sorted(multi_presets_sec.items(), key=lambda x: len(set(l[0] for l in x[1])), reverse=True)
for i, (sw, locs) in enumerate(sorted_multi[:20]):
    distinct_p = sorted(list(set(l[0] for l in locs)))
    slots = Counter(l[2] for l in locs)
    z, p, sc = decode_stage(sw, SR_39K)
    print(f"\n--- Section #{i+1}: {len(distinct_p)} presets ({len(locs)} total corners) | Slots: {dict(slots)} ---")
    print(f"  Words: {[f'0x{w:04X}' for w in sw]}")
    print(f"  Zero: {z}")
    print(f"  Pole: {p}")
    print(f"  Scale: {sc:.4f} ({20*np.log10(sc):.2f} dB)")
    print(f"  Presets: {distinct_p}")

# 2. Exact Zero states across distinct presets
zero_to_loc = defaultdict(list)
for pname, corners in presets.items():
    for cname, stages in corners.items():
        for si, sw in enumerate(stages):
            if sw != IDENTITY_WORDS:
                zero_to_loc[(sw[0], sw[1])].append((pname, cname, si + 1))

multi_p_zeros = {zw: locs for zw, locs in zero_to_loc.items() if len(set(l[0] for l in locs)) > 1}
print(f"\nTotal active zero states: {len(zero_to_loc)}")
print(f"Zero states shared across 2 or more distinct P2k presets: {len(multi_p_zeros)}")

sorted_zeros = sorted(multi_p_zeros.items(), key=lambda x: len(set(l[0] for l in x[1])), reverse=True)
for i, (zw, locs) in enumerate(sorted_zeros[:15]):
    distinct_p = sorted(list(set(l[0] for l in locs)))
    slots = Counter(l[2] for l in locs)
    z = pair_geometry_at(decode_u16(zw[0]), decode_u16(zw[1]), SR_39K)
    print(f"Zero #{i+1}: {len(distinct_p)} presets ({len(locs)} corners) | Slots: {dict(slots)} | [0x{zw[0]:04X}, 0x{zw[1]:04X}] | Geo: {z}")

# 3. Exact Pole states across distinct presets
pole_to_loc = defaultdict(list)
for pname, corners in presets.items():
    for cname, stages in corners.items():
        for si, sw in enumerate(stages):
            if sw != IDENTITY_WORDS:
                pole_to_loc[(sw[2], sw[3])].append((pname, cname, si + 1))

multi_p_poles = {pw: locs for pw, locs in pole_to_loc.items() if len(set(l[0] for l in locs)) > 1}
print(f"\nTotal active pole states: {len(pole_to_loc)}")
print(f"Pole states shared across 2 or more distinct P2k presets: {len(multi_p_poles)}")

sorted_poles = sorted(multi_p_poles.items(), key=lambda x: len(set(l[0] for l in x[1])), reverse=True)
for i, (pw, locs) in enumerate(sorted_poles[:15]):
    distinct_p = sorted(list(set(l[0] for l in locs)))
    slots = Counter(l[2] for l in locs)
    p = pair_geometry_at(decode_u16(pw[0]), decode_u16(pw[1]), SR_39K)
    print(f"Pole #{i+1}: {len(distinct_p)} presets ({len(locs)} corners) | Slots: {dict(slots)} | [0x{pw[0]:04X}, 0x{pw[1]:04X}] | Geo: {p}")

