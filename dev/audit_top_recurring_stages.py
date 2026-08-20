import os, sys, glob, json, struct, math
from collections import Counter, defaultdict

SR = 39062.5
TAU = 2.0 * math.pi
IDENTITY_WORDS = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xE000)

def decode_u16(word):
    u = int(word) + 1
    if u == 65536: return 1.0
    if u == 1: return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))

def pair_geom(d_mag, d_rsq):
    q = 1.0 - d_rsq
    c = 4.0 * d_mag + d_rsq
    p = c - 2.0
    if abs(p) < 1e-12 and abs(q) < 1e-12:
        return {'type': 'Bypass', 'hz': 0.0, 'r': 0.0}
    disc = p * p - 4.0 * q
    if disc < 0.0:
        r = math.sqrt(max(0.0, q))
        cos_w = max(-1.0, min(1.0, -p / (2.0 * r))) if r > 0 else 0.0
        hz = math.acos(cos_w) / TAU * SR
        return {'type': 'Conjugate', 'hz': hz, 'r': r}
    else:
        s = math.sqrt(disc)
        return {'type': 'Real', 'r1': (-p + s) / 2.0, 'r2': (-p - s) / 2.0}

def decode_stage(w):
    d0, d1, d2, d3, d4 = [decode_u16(x) for x in w]
    z = pair_geom(d0, d1)
    p = pair_geom(d2, d3)
    g = 4.0 * d4
    return z, p, g

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analyze_corpus_sections import load_all_corpora, analyze_binary_corpus
corpus, xml_corpus = load_all_corpora()
bin_analysis = analyze_binary_corpus(corpus)

section_occ = bin_analysis['sections']
exact_ranked = sorted(section_occ.items(), key=lambda x: len(x[1]), reverse=True)

print("=" * 80)
print(f"TOP 10 MOST RECURRING EXACT COMPLETE STAGES ACROSS ENTIRE CORPUS")
print(f"Total Unique Filter Corners Audited: {len(bin_analysis['all_corners'])}")
print(f"Total Active Section States Found: {len(section_occ)}")
print("=" * 80)

for rank, (words, locs) in enumerate(exact_ranked[:10]):
    z, p, g = decode_stage(words)
    distinct_filters = set(l[0] for l in locs)
    stages_hit = Counter(l[2]+1 for l in locs)
    stages_str = ', '.join([f"S{s}:{c}" for s, c in sorted(stages_hit.items())])
    
    if p['type'] == 'Conjugate':
        p_desc = f"{p['hz']:.1f} Hz (r={p['r']:.4f})"
    else:
        p_desc = f"Real [{p.get('r1',0):.3f}, {p.get('r2',0):.3f}]"
        
    if z['type'] == 'Conjugate':
        z_desc = f"{z['hz']:.1f} Hz (r={z['r']:.4f})"
    else:
        z_desc = f"Real [{z.get('r1',0):.3f}, {z.get('r2',0):.3f}]"
    
    gain_db = 20.0 * math.log10(max(g, 1e-6))
    
    print(f"\n[RANK #{rank+1}] {len(locs)} occurrences across {len(distinct_filters)} filters ({stages_str})")
    print(f"  Minifloat Words : [{words[0]:04X}, {words[1]:04X}, {words[2]:04X}, {words[3]:04X}, {words[4]:04X}]")
    print(f"  Pole Geometry   : {p_desc}")
    print(f"  Zero Geometry   : {z_desc}")
    print(f"  Stage Gain      : {g:.4f} ({gain_db:+.2f} dB)")
    sample_f = list(set(str(l[0]).split(':')[-1] for l in locs))[:6]
    print(f"  Sample Filters  : {sample_f}")

# 2. Clustered / Closely Occurring States (within +/- 30 Hz and +/- 0.02 radius)
print("\n" + "=" * 80)
print("TOP 5 DENSEST CLUSTERS OF CLOSELY OCCURRING POLE/ZERO STAGES")
print("=" * 80)

# Group conjugate pole-zero stages into 1/12th octave frequency bands
clusters = defaultdict(list)
for words, locs in section_occ.items():
    z, p, g = decode_stage(words)
    if p['type'] == 'Conjugate' and z['type'] == 'Conjugate':
        # Quantize pole Hz to 1/6th octave band, zero Hz to 1/6th octave
        p_bin = round(math.log2(max(p['hz'], 20.0)) * 6.0)
        z_bin = round(math.log2(max(z['hz'], 20.0)) * 6.0)
        clusters[(p_bin, z_bin)].append((words, p, z, g, locs))

sorted_clusters = sorted(clusters.items(), key=lambda x: sum(len(item[4]) for item in x[1]), reverse=True)

for c_rank, ((p_bin, z_bin), items) in enumerate(sorted_clusters[:5]):
    total_inst = sum(len(item[4]) for item in items)
    avg_p_hz = sum(item[1]['hz'] * len(item[4]) for item in items) / total_inst
    avg_p_r  = sum(item[1]['r'] * len(item[4]) for item in items) / total_inst
    avg_z_hz = sum(item[2]['hz'] * len(item[4]) for item in items) / total_inst
    avg_z_r  = sum(item[2]['r'] * len(item[4]) for item in items) / total_inst
    
    distinct_f = set()
    for item in items:
        for loc in item[4]:
            distinct_f.add(loc[0])
            
    print(f"\n[CLUSTER #{c_rank+1}] {total_inst} instances across {len(distinct_f)} distinct filters")
    print(f"  Mean Center Pole : {avg_p_hz:.1f} Hz (mean r = {avg_p_r:.4f})")
    print(f"  Mean Center Zero : {avg_z_hz:.1f} Hz (mean r = {avg_z_r:.4f})")
    print(f"  Number of close variants in cluster: {len(items)} distinct minifloat word tuples")
