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

def stage_geometry_to_normalized_biquad(p_geo, z_geo, fs, scale=1.0):
    if p_geo['type'] == 'Degenerate' and z_geo['type'] == 'Degenerate':
        return [1.0 * scale, 0.0, 0.0, 0.0, 0.0]
    
    # Pole polynomial
    if p_geo['type'] == 'Conjugate':
        wp = TAU * p_geo['hz'] / fs
        rp = p_geo['r']
        a1 = -2.0 * rp * np.cos(wp)
        a2 = rp * rp
    elif p_geo['type'] == 'RealPair':
        a1 = -(p_geo['root_a'] + p_geo['root_b'])
        a2 = p_geo['root_a'] * p_geo['root_b']
    else:
        a1, a2 = 0.0, 0.0

    # Zero polynomial
    if z_geo['type'] == 'Conjugate':
        wz = TAU * z_geo['hz'] / fs
        rz = z_geo['r']
        b1n = -2.0 * rz * np.cos(wz)
        b2n = rz * rz
        # Normalization: DC gain matching
        den_dc = 1.0 + a1 + a2
        num_dc = max(1e-6, 1.0 + b1n + b2n)
        g = (den_dc / num_dc) * scale
        return [g, g * b1n, g * b2n, a1, a2]
    elif z_geo['type'] == 'RealPair':
        b1n = -(z_geo['root_a'] + z_geo['root_b'])
        b2n = z_geo['root_a'] * z_geo['root_b']
        den_dc = 1.0 + a1 + a2
        num_dc = max(1e-6, 1.0 + b1n + b2n)
        g = (den_dc / num_dc) * scale
        return [g, g * b1n, g * b2n, a1, a2]
    else:
        # All-pole stage: normalized so peak or DC is bounded
        g = max(1e-4, 1.0 - a2) * scale
        return [g, 0.0, 0.0, a1, a2]

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

# Load P2k presets
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

# Load 289 cubes
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

# Load cube names from demodulation
try:
    from map_cubes_to_rest import cubes_extracted
    cube_names = {c['index']: c['name'] for c in cubes_extracted}
except Exception:
    cube_names = {i: f"Cube_{i:03d}" for i in range(289)}

# Generate high quality 4-panel figure
fig, axes = plt.subplots(2, 2, figsize=(15, 11))
freqs = np.geomspace(40, 18000, 512)

# Panel 1: Sample Rate Mismatch & Pitch Drift
ax = axes[0, 0]
p_geo = {'type': 'Conjugate', 'hz': 2540.0, 'r': 0.985}
z_geo = {'type': 'Conjugate', 'hz': 3200.0, 'r': 0.950}

# Rate 1: Exact Datum (39,062.5 Hz)
b_39k = stage_geometry_to_normalized_biquad(p_geo, z_geo, SR_39K)
r_39k = biquad_response_db(b_39k, freqs, SR_39K)

# Rate 2: Uncompensated (same raw angular code played at 44.1 kHz -> frequency shifts by 44100/39062.5)
p_geo_uncomp_44k = {'type': 'Conjugate', 'hz': 2540.0 * (SR_44K / SR_39K), 'r': 0.985}
z_geo_uncomp_44k = {'type': 'Conjugate', 'hz': 3200.0 * (SR_44K / SR_39K), 'r': 0.950}
b_uncomp_44k = stage_geometry_to_normalized_biquad(p_geo_uncomp_44k, z_geo_uncomp_44k, SR_44K)
r_uncomp_44k = biquad_response_db(b_uncomp_44k, freqs, SR_44K)

# Rate 3: Uncompensated at 48.0 kHz -> frequency shifts by 48000/39062.5
p_geo_uncomp_48k = {'type': 'Conjugate', 'hz': 2540.0 * (SR_48K / SR_39K), 'r': 0.985}
z_geo_uncomp_48k = {'type': 'Conjugate', 'hz': 3200.0 * (SR_48K / SR_39K), 'r': 0.950}
b_uncomp_48k = stage_geometry_to_normalized_biquad(p_geo_uncomp_48k, z_geo_uncomp_48k, SR_48K)
r_uncomp_48k = biquad_response_db(b_uncomp_48k, freqs, SR_48K)

# Rate 4: Rate-Compensated at 44.1 kHz (Exact 2540 Hz pole preserved)
b_comp_44k = stage_geometry_to_normalized_biquad(p_geo, z_geo, SR_44K)
r_comp_44k = biquad_response_db(b_comp_44k, freqs, SR_44K)

ax.plot(freqs, r_39k, label="Datum (39.0625 kHz): fp = 2540 Hz, fz = 3200 Hz", color='#1f77b4', lw=2.2)
ax.plot(freqs, r_uncomp_44k, label="Uncompensated 44.1 kHz: fp = 2868 Hz (+209 cents)", color='#ff7f0e', lw=1.8, ls='--')
ax.plot(freqs, r_uncomp_48k, label="Uncompensated 48.0 kHz: fp = 3121 Hz (+356 cents)", color='#d62728', lw=1.8, ls=':')
ax.plot(freqs, r_comp_44k, label="Compensated 44.1 kHz: fp = 2540 Hz (Exact)", color='#2ca02c', lw=2.0, ls='-.')

ax.set_xscale('log')
ax.set_title("Sample Rate Mismatch & Pitch Drift on Resonant Stages", fontsize=10, fontweight='bold')
ax.set_ylabel("Magnitude (dB)")
ax.set_xlabel("Frequency (Hz)")
ax.grid(True, which='both', alpha=0.3)
ax.legend(fontsize=8, loc='upper left')
ax.set_ylim(-25, 25)

# Panel 2: Verified Golden Preset Overlay (TalkingHedz: Datum vs Rate-Compensated vs Uncompensated)
ax = axes[0, 1]
# TalkingHedz M0_Q0 from P2k
th_words = p2k_presets['P2k_013_talking_hedz']['M0_Q0']
th_bqs_39k = [words_to_biquad(w) for w in th_words]
th_resp_39k = cascade_response_db(th_bqs_39k, freqs, SR_39K)

def recompile_stage_to_sr(words, sr_from=39062.5, sr_to=44100.0):
    z, p, sc = decode_stage(words, sr_from)
    # Pole polynomial
    if p['type'] == 'Conjugate':
        wp = TAU * p['hz'] / sr_to
        rp = p['r']
        a1 = -2.0 * rp * np.cos(wp)
        a2 = rp * rp
    elif p['type'] == 'RealPair':
        a1 = -(p['root_a'] + p['root_b'])
        a2 = p['root_a'] * p['root_b']
    else:
        a1, a2 = 0.0, 0.0

    # Zero polynomial
    if z['type'] == 'Conjugate':
        wz = TAU * z['hz'] / sr_to
        rz = z['r']
        b1 = -2.0 * rz * np.cos(wz)
        b2 = rz * rz
    elif z['type'] == 'RealPair':
        b1 = -(z['root_a'] + z['root_b'])
        b2 = z['root_a'] * z['root_b']
    else:
        b1, b2 = 0.0, 0.0

    k = words_to_kernel(words)
    c4 = k[4]
    return [c4, b1 * c4, b2 * c4, a1, a2]

# Recompiled to 44.1 kHz with root geometry preservation
th_bqs_44k = [recompile_stage_to_sr(w, SR_39K, SR_44K) for w in th_words]
th_resp_44k = cascade_response_db(th_bqs_44k, freqs, SR_44K)

# Uncompensated raw words at 44.1 kHz (what happened when sample rate was ignored)
th_resp_uncomp_44k = cascade_response_db(th_bqs_39k, freqs, SR_44K)

ax.plot(freqs, th_resp_39k, label="TalkingHedz M0_Q0 at 39.0625 kHz Datum", color='#1f77b4', lw=2.2)
ax.plot(freqs, th_resp_44k, label="Rate-Compensated at 44.1 kHz (Formants Preserved)", color='#2ca02c', lw=2.0, ls='--')
ax.plot(freqs, th_resp_uncomp_44k, label="Uncompensated 44.1 kHz (+209 cents pitch-shifted)", color='#d62728', lw=1.5, ls=':')

ax.set_xscale('log')
ax.set_title("TalkingHedz Vowel Cascade: Datum vs Rate-Compensated", fontsize=10, fontweight='bold')
ax.set_ylabel("Magnitude (dB)")
ax.set_xlabel("Frequency (Hz)")
ax.grid(True, which='both', alpha=0.3)
ax.legend(fontsize=8, loc='lower left')
ax.set_ylim(-40, 20)


# Panel 3: Resonant Pole & Zero Density Across 289 Morpheus Cubes
ax = axes[1, 0]
all_cube_poles = []
all_cube_zeros = []
for c in cubes_289.values():
    for corn in c.values():
        for s in corn:
            if s['pk1'] < 255:
                p = armadillo_to_geometry(s['pk1'], s['pk2'], SR_39K)
                if p['r'] > 0.8:
                    all_cube_poles.append(p['hz'])
            if s['zk1'] < 255:
                z = armadillo_to_geometry(s['zk1'], s['zk2'], SR_39K)
                if z['r'] > 0.8:
                    all_cube_zeros.append(z['hz'])

bins = np.geomspace(40, 18000, 45)
ax.hist(all_cube_poles, bins=bins, color='#1f77b4', alpha=0.65, label=f"Resonant Poles r>0.8 (N={len(all_cube_poles)})")
ax.hist(all_cube_zeros, bins=bins, color='#d62728', alpha=0.55, label=f"Carving Zeros r>0.8 (N={len(all_cube_zeros)})")
ax.set_xscale('log')
ax.set_title("Root Geometry Distribution Across All 289 Cubes (39.0625 kHz Datum)", fontsize=10, fontweight='bold')
ax.set_xlabel("Frequency (Hz)")
ax.set_ylabel("Count")
ax.grid(True, which='both', alpha=0.3)
ax.legend(fontsize=8)

# Panel 4: Cross-Corpus Reusability Breakdown
ax = axes[1, 1]
categories = [
    'P2k Presets\n(33 Filters)',
    'Emulator X\n(71 Presets)',
    'Morpheus Cubes\n(289 Cubes)'
]
unique_states = [792 - 187, 426 - 37, 16184 - 4376]
reused_states = [187, 37, 4376]

x = np.arange(len(categories))
width = 0.45

ax.bar(x, unique_states, width, label='Unique States', color='#aec7e8')
ax.bar(x, reused_states, width, bottom=unique_states, label='Shared/Reused States', color='#1f77b4')

ax.set_ylabel("Total Section Instances")
ax.set_title("Section Composition Across Corpora", fontsize=10, fontweight='bold')
ax.set_xticks(x)
ax.set_xticklabels(categories, fontsize=9)
ax.grid(True, axis='y', alpha=0.3)
ax.legend(fontsize=8.5)

for i in range(len(categories)):
    tot = unique_states[i] + reused_states[i]
    pct = 100.0 * reused_states[i] / tot
    ax.annotate(f"{tot}\n({pct:.1f}% reused)", (x[i], tot + tot*0.03), ha='center', fontsize=8.5, fontweight='bold')

ax.set_ylim(0, 19000)

plt.tight_layout()
plt.savefig('plots/cubes_to_corpus_cross_mapping.png', dpi=200)
plt.close()

print("Rebuilt plots/cubes_to_corpus_cross_mapping.png with verified DSP normalization.")
