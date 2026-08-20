import json, glob, os, math
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

morph_path = os.path.join(ROOT, 'ref/morpheus/cubes_decoded.json')
with open(morph_path) as f:
    morph_data = json.load(f)

p2k_files = sorted(glob.glob(os.path.join(ROOT, 'recipes/architectures/*.json')))
p2k_filters = []
for p in p2k_files:
    with open(p) as f:
        p2k_filters.append(json.load(f))

def parse_root(r_dict, datum=39062.5):
    if 'pair' in r_dict:
        a, b = r_dict['pair']
        f = 30.0 if (a + b) >= 0 else (datum / 2.0)
        r = min(math.sqrt(abs(a * b)), 0.99999)
        return f, r, True
    return r_dict['hz'], r_dict['r'], False

# Categorize P2K by type
type_stats = defaultdict(lambda: defaultdict(list))
for filt in p2k_filters:
    fname = filt['name']
    ftype = filt.get('x3_type', 'UNK')
    for sec in filt['sections']:
        slot = sec['slot']
        c = sec['corners']['M0_Q0']
        p_hz, p_r, _ = parse_root(c['pole'])
        z_hz, z_r, _ = parse_root(c['zero'])
        if p_r > 0.02 and z_r > 0.02 and p_hz > 20 and z_hz > 20:
            oct_sep = math.log2(p_hz / z_hz)
            type_stats[ftype][slot].append(oct_sep)

print("=== P2K STAGE 1 & 6 OCTAVE SEPARATION BY FILTER SPECIES (M0_Q0) ===")
for ftype, slots in sorted(type_stats.items()):
    s1_vals = slots[1]
    s6_vals = slots[6]
    s1_mean = sum(s1_vals)/len(s1_vals) if s1_vals else 0
    s6_mean = sum(s6_vals)/len(s6_vals) if s6_vals else 0
    print(f"Type {ftype:<4} (N={len(s1_vals):2d}): S1 Mean = {s1_mean:+5.2f} oct | S6 Mean = {s6_mean:+5.2f} oct")

# Vowel filters specifically
print("\n=== P2K VOWEL FILTERS CORNER M0_Q0 STAGE-BY-STAGE SPAN ===")
vowels = [f for f in p2k_filters if f.get('x3_type') == 'VOW']
for v in vowels:
    print(f"Filter: {v['name']}")
    for s in v['sections']:
        c = s['corners']['M0_Q0']
        p, pr, _ = parse_root(c['pole'])
        z, zr, _ = parse_root(c['zero'])
        sep = math.log2(p/z) if p > 0 and z > 0 else 0
        print(f"  Stage {s['slot']}: Pole {p:7.1f} Hz (r={pr:.4f}) | Zero {z:7.1f} Hz (r={zr:.4f}) -> {sep:+5.2f} oct ({sep*12:+5.1f} st)")
