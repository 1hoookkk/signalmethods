import os, sys, glob, json, struct, math
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analyze_corpus_sections import load_all_corpora, analyze_binary_corpus
from audit_top_recurring_stages import decode_stage, pair_geom, decode_u16

corpus, xml_corpus = load_all_corpora()
bin_analysis = analyze_binary_corpus(corpus)

# 1. P2K Recipes Specific Breakdown
p2k_stages = defaultdict(list)
for f in glob.glob('recipes/architectures/*.json'):
    pname = os.path.basename(f).replace('.json', '')
    with open(f, 'r') as fp:
        d = json.load(fp)
    for cname, cdata in d.get('corners', {}).items():
        for si, w in enumerate(cdata.get('words', [])):
            wt = tuple(w)
            if wt != (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xE000) and wt != (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF):
                p2k_stages[wt].append((pname, cname, si))

print("=" * 80)
print(f"TOP RECURRING BIQUAD STAGE STATES ACROSS 33 P2K FACTORY PRESETS")
print("=" * 80)

sorted_p2k = sorted(p2k_stages.items(), key=lambda x: len(x[1]), reverse=True)
for idx, (words, locs) in enumerate(sorted_p2k[:10]):
    z, p, g = decode_stage(words)
    distinct_p = set(l[0] for l in locs)
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
    
    print(f"\n[P2K RANK #{idx+1}] {len(locs)} occurrences across {len(distinct_p)} presets ({stages_str})")
    print(f"  Minifloat Words : [{words[0]:04X}, {words[1]:04X}, {words[2]:04X}, {words[3]:04X}, {words[4]:04X}]")
    print(f"  Pole Geometry   : {p_desc}")
    print(f"  Zero Geometry   : {z_desc}")
    print(f"  Stage Gain      : {g:.4f} ({gain_db:+.2f} dB)")
    sample_f = list(distinct_p)[:5]
    print(f"  Used in         : {sample_f}")
