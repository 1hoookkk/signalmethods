import os, csv, math, json, glob, struct
from collections import defaultdict, Counter
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

TAU = 2.0 * math.pi
SR_39K = 39062.5
SR_44K = 44100.0
SR_48K = 48000.0
RIM_DB = 96.0
COMBINE_K = 4.0
IDENTITY_WORDS = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)

def armadillo_decode(k_angle, k_radius, fs):
    theta_prime = math.pi * (k_angle / 255.0)
    fc = (fs * 0.5) * (2.0 ** (10.0 * (k_angle / 255.0) - 10.0))
    if k_radius >= 250:
        r = 1.0
        r_prime_db = float('inf')
    else:
        r_prime_db = RIM_DB * (k_radius / 255.0)
        r = 1.0 - 10.0 ** (-r_prime_db / 20.0)
    return fc, r, theta_prime, r_prime_db


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

def armadillo_to_geometry(k_angle, k_radius, fs):
    if k_angle == 255 and k_radius == 255:
        return {'type': 'Degenerate', 'hz': 0.0, 'r': 0.0}
    fc = (fs * 0.5) * (2.0 ** (10.0 * (k_angle / 255.0) - 10.0))
    if k_radius >= 250:
        r = 1.0 # Traveling null
    else:
        r_prime_db = RIM_DB * (k_radius / 255.0)
        r = 1.0 - 10.0 ** (-r_prime_db / 20.0)
    return {'type': 'Conjugate', 'hz': float(fc), 'r': float(r)}

def words_to_kernel(words):
    d = [decode_u16(w) for w in words]
    return [COMBINE_K * d[0] + d[1], d[1], COMBINE_K * d[2] + d[3], d[3], COMBINE_K * d[4]]

def kernel_to_biquad(k):
    c0, c1, c2, c3, c4 = k
    return [c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3]

def words_to_biquad(words):
    return kernel_to_biquad(words_to_kernel(words))

def biquad_response_db(b, freqs, sr):
    w = 2.0 * np.pi * freqs / sr
    z1 = np.exp(-1j * w)
    z2 = np.exp(-2j * w)
    num = b[0] + b[1] * z1 + b[2] * z2
    den = 1.0 + b[3] * z1 + b[4] * z2
    h = num / np.where(np.abs(den) < 1e-12, 1e-12, den)
    return 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))

def cascade_response_db(biquads, freqs, sr):
    total_db = np.zeros_like(freqs, dtype=float)
    for b in biquads:
        total_db += biquad_response_db(b, freqs, sr)
    return total_db

# 1. Load P2k factory presets
p2k_files = sorted(glob.glob('ref/presets/P2k_*.bin'))
p2k_presets = {}
for pf in p2k_files:
    name = os.path.splitext(os.path.basename(pf))[0]
    data = open(pf, 'rb').read()
    words = struct.unpack('<' + 'H' * 120, data)
    corners = {}
    for ci, cname in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
        stages = [tuple(words[(ci*6 + si)*5 : (ci*6 + si + 1)*5]) for si in range(6)]
        corners[cname] = stages
    p2k_presets[name] = corners

# 2. Load 289 cubes from CSV with raw ARMAdillo bytes
path_cubes = r'C:\Users\hooki\trench-filter-list\ref\p2k_variants\combined\cubes_raw_bytes.csv'
cubes_289 = defaultdict(lambda: defaultdict(list))
with open(path_cubes, 'r', encoding='utf-8', errors='ignore') as f:
    r = csv.reader(f)
    for row in r:
        if not row or row[0].startswith('#') or row[0] == 'cube': continue
        c_num, corner, r0, r1, stage, pk1, pk2, zk1, zk2 = row
        cubes_289[int(c_num)][corner].append({
            'stage': int(stage),
            'pk1': int(pk1), 'pk2': int(pk2),
            'zk1': int(zk1), 'zk2': int(zk2)
        })

# 3. Load firmware names for the 289 cubes
from map_cubes_to_rest import cubes_extracted
cube_names = {c['index']: c['name'] for c in cubes_extracted}

print(f"Loaded {len(p2k_presets)} P2k presets and {len(cubes_289)} Cubes ({len(cube_names)} named).")

# 4. Cross-Match Analysis: Find Direct Overlaps between Cubes and P2k Presets!
# Compare decoded physical roots at 39,062.5 Hz and with sample-rate scaling:
# Check if any P2k preset matches any Cube corner!

matches_39k = []
matches_rate_scaled = []

# Collect all P2k stage poles and zeros
p2k_stage_pool = []
for pname, corners in p2k_presets.items():
    for cname, stages in corners.items():
        for si, sw in enumerate(stages):
            if sw != IDENTITY_WORDS:
                z, p, sc = decode_stage(sw, SR_39K)
                p2k_stage_pool.append({
                    'preset': pname, 'corner': cname, 'slot': si + 1,
                    'zero': z, 'pole': p, 'scale': sc, 'words': sw
                })

# Cross-compare each cube stage against P2k stages
cube_to_p2k_matches = defaultdict(list)

for cid, corners in cubes_289.items():
    cname_str = cube_names.get(cid, f"Cube_{cid:03d}")
    for corner_id, stages in corners.items():
        for s in stages:
            s_idx = s['stage'] + 1
            # Decode ARMAdillo stage at 39,062.5 Hz (native legacy datum)
            z_39k = armadillo_to_geometry(s['zk1'], s['zk2'], SR_39K)
            p_39k = armadillo_to_geometry(s['pk1'], s['pk2'], SR_39K)
            
            # Decode at 44.1 kHz and 48 kHz to test rate-mismatches
            z_48k = armadillo_to_geometry(s['zk1'], s['zk2'], SR_48K)
            p_48k = armadillo_to_geometry(s['pk1'], s['pk2'], SR_48K)
            
            # Check for close frequency and radius matches in P2k
            for p2k_s in p2k_stage_pool:
                # Compare pole frequencies and radii
                if p2k_s['pole']['type'] == 'Conjugate' and p_39k['type'] == 'Conjugate':
                    f_diff_pct = abs(p2k_s['pole']['hz'] - p_39k['hz']) / max(1.0, p2k_s['pole']['hz'])
                    r_diff = abs(p2k_s['pole']['r'] - p_39k['r'])
                    if f_diff_pct < 0.03 and r_diff < 0.015:
                        cube_to_p2k_matches[(cid, cname_str, p2k_s['preset'])].append({
                            'cube_stage': s_idx, 'cube_corner': corner_id,
                            'p2k_slot': p2k_s['slot'], 'p2k_corner': p2k_s['corner'],
                            'pole_cube': p_39k, 'pole_p2k': p2k_s['pole'],
                            'zero_cube': z_39k, 'zero_p2k': p2k_s['zero']
                        })

print(f"\nFound {len(cube_to_p2k_matches)} Cube <-> P2k filter correlation groups.")

# Rank top correlated Cube <-> P2k pairs
sorted_corrs = sorted(cube_to_p2k_matches.items(), key=lambda x: len(x[1]), reverse=True)
print("\n=== TOP CORRELATED CUBE <-> P2K PRESET FAMILIES ===")
for i, ((cid, cname, pname), matches) in enumerate(sorted_corrs[:20]):
    print(f"#{i+1}: Cube {cid:03d} ('{cname}') <-> P2k '{pname}' : {len(matches)} matching stage pairs")
    for m in matches[:3]:
        print(f"     S{m['cube_stage']}({m['cube_corner']}) <-> S{m['p2k_slot']}({m['p2k_corner']}): Pole={m['pole_cube']['hz']:.0f}Hz (r={m['pole_cube']['r']:.3f}) vs P2k={m['pole_p2k']['hz']:.0f}Hz (r={m['pole_p2k']['r']:.3f})")

# Generate Comprehensive Plot comparing Cube 289 decodes to P2k Presets at 39.0625 kHz and 44.1 kHz
fig, axes = plt.subplots(2, 2, figsize=(14, 10))
freqs = np.geomspace(40, 18000, 512)

# Plot 1: Sample Rate Mismatch Effect on ARMAdillo Coordinates (+209 cents shift)
ax = axes[0, 0]
k_ang = 180
k_rad = 220
fc_39k, r_39k, _, _ = armadillo_decode(k_ang, k_rad, SR_39K)
fc_44k, r_44k, _, _ = armadillo_decode(k_ang, k_rad, SR_44K)
fc_48k, r_48k, _, _ = armadillo_decode(k_ang, k_rad, SR_48K)

# Biquad curves for each rate
def make_biquad(fc, r, fs):
    w = 2.0 * np.pi * fc / fs
    a1 = -2.0 * r * np.cos(w)
    a2 = r * r
    return [1.0, 0.0, 0.0, a1, a2]

r_39 = biquad_response_db(make_biquad(fc_39k, r_39k, SR_39K), freqs, SR_39K)
r_44 = biquad_response_db(make_biquad(fc_44k, r_44k, SR_44K), freqs, SR_44K)
r_48 = biquad_response_db(make_biquad(fc_48k, r_48k, SR_48K), freqs, SR_48K)

ax.plot(freqs, r_39, label=f"Datum 39.0625 kHz: {fc_39k:.0f} Hz", color='#1f77b4', lw=2)
ax.plot(freqs, r_44, label=f"Uncompensated 44.1 kHz: {fc_44k:.0f} Hz (+209 cents)", color='#ff7f0e', lw=2, ls='--')
ax.plot(freqs, r_48, label=f"Uncompensated 48.0 kHz: {fc_48k:.0f} Hz (+356 cents)", color='#d62728', lw=2, ls=':')
ax.set_xscale('log')
ax.set_title("Sample Rate Mismatch Shift on Raw ARMAdillo Words", fontsize=10, fontweight='bold')
ax.set_ylabel("Magnitude (dB)")
ax.grid(True, which='both', alpha=0.3)
ax.legend(fontsize=8, loc='upper left')
ax.set_ylim(-30, 30)

# Plot 2: Top Correlated Cube vs P2k Preset Overlay
ax = axes[0, 1]
# Compare a top matching cube corner with its P2k counterpart
top_pair = sorted_corrs[0]
cid, cname, pname = top_pair[0]
ax.set_title(f"Cube {cid:03d} ('{cname}') vs P2k '{pname}'", fontsize=10, fontweight='bold')
# Construct cascade for cube corner
c_stages = cubes_289[cid]['000']
bqs_cube_39 = [make_biquad(armadillo_to_geometry(s['pk1'], s['pk2'], SR_39K)['hz'], armadillo_to_geometry(s['pk1'], s['pk2'], SR_39K)['r'], SR_39K) for s in c_stages]
bqs_p2k = [words_to_biquad(w) for w in p2k_presets[pname]['M0_Q0']]
r_cube = cascade_response_db(bqs_cube_39, freqs, SR_39K)
r_p2k = cascade_response_db(bqs_p2k, freqs, SR_39K)

ax.plot(freqs, r_cube, label=f"Cube {cid:03d} ('{cname}') Corner 000", color='#2ca02c', lw=2)
ax.plot(freqs, r_p2k, label=f"P2k '{pname}' Corner M0_Q0", color='#1f77b4', lw=2, ls='--')
ax.set_xscale('log')
ax.set_ylabel("Magnitude (dB)")
ax.grid(True, which='both', alpha=0.3)
ax.legend(fontsize=8, loc='upper right')
ax.set_ylim(-40, 40)

# Plot 3: 289-Cube Pole Frequency Distribution at 39.0625 kHz vs 44.1 kHz
ax = axes[1, 0]
all_cube_poles_39k = [armadillo_to_geometry(s['pk1'], s['pk2'], SR_39K)['hz'] for c in cubes_289.values() for corn in c.values() for s in corn if s['pk1'] < 255]
ax.hist(all_cube_poles_39k, bins=np.geomspace(20, 19000, 50), color='#1f77b4', alpha=0.7, label='Poles at 39.0625 kHz Datum')
ax.set_xscale('log')
ax.set_title("Resonant Pole Density Across All 289 Morpheus Cubes", fontsize=10, fontweight='bold')
ax.set_xlabel("Frequency (Hz)")
ax.set_ylabel("Pole Count")
ax.grid(True, which='both', alpha=0.3)
ax.legend(fontsize=8)

# Plot 4: Cross-Corpus Section Reuse Summary
ax = axes[1, 1]
labels = ['P2k Shared Secs', 'XML Shared Secs', 'Cubes Shared Secs', 'Cubes Shared Corners']
counts = [187, 37, 11808, 42]
# Bar chart of multi-occurrence sections across all 3 corpora
ax.bar(['P2k\n(33 Presets)', 'Emulator X\n(71 XMLs)', 'Morpheus Cubes\n(289 Cubes)'], [187, 37, 11808], color=['#1f77b4', '#ff7f0e', '#2ca02c'])
ax.set_yscale('log')
ax.set_title("Section Reuse Depth Across the 3 Corpora (Log Scale)", fontsize=10, fontweight='bold')
ax.set_ylabel("Reused Section Instances")
ax.grid(True, axis='y', alpha=0.3)
for p in ax.patches:
    ax.annotate(f"{int(p.get_height())}", (p.get_x() + p.get_width() / 2., p.get_height() * 1.15), ha='center', fontsize=9, fontweight='bold')

plt.tight_layout()
plt.savefig('plots/cubes_to_corpus_cross_mapping.png', dpi=200)
plt.close()

print("Saved plots/cubes_to_corpus_cross_mapping.png")
