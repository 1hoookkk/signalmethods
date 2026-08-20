import json, glob, os, math
from collections import defaultdict, Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Load Morpheus cubes
morph_path = os.path.join(ROOT, 'ref/morpheus/cubes_decoded.json')
with open(morph_path) as f:
    morph_data = json.load(f)

# Load P2K filters
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

# Analyze P2K
p2k_stage_octaves = defaultdict(list)
p2k_morph_travel_poles = defaultdict(list)
p2k_morph_travel_zeros = defaultdict(list)
p2k_filter_ends = []

for filt in p2k_filters:
    fname = filt['name']
    fidx = filt['index']
    ftype = filt.get('x3_type', 'UNK')
    for sec in filt['sections']:
        slot = sec['slot']
        for cname in ['M0_Q0', 'M0_Q100', 'M100_Q0', 'M100_Q100']:
            c = sec['corners'][cname]
            p_hz, p_r, p_real = parse_root(c['pole'])
            z_hz, z_r, z_real = parse_root(c['zero'])
            if p_r > 0.02 and z_r > 0.02 and p_hz > 20 and z_hz > 20:
                oct_sep = math.log2(p_hz / z_hz)
                p2k_stage_octaves[slot].append((fname, fidx, cname, p_hz, p_r, z_hz, z_r, oct_sep))
        
        # Morph travel (M0_Q0 -> M100_Q0)
        c0 = sec['corners']['M0_Q0']
        c1 = sec['corners']['M100_Q0']
        p0, pr0, _ = parse_root(c0['pole'])
        p1, pr1, _ = parse_root(c1['pole'])
        z0, zr0, _ = parse_root(c0['zero'])
        z1, zr1, _ = parse_root(c1['zero'])
        if pr0 > 0.02 and pr1 > 0.02 and p0 > 20 and p1 > 20:
            p2k_morph_travel_poles[slot].append((fname, math.log2(p1/p0)))
        if zr0 > 0.02 and zr1 > 0.02 and z0 > 20 and z1 > 20:
            p2k_morph_travel_zeros[slot].append((fname, math.log2(z1/z0)))
            
    # Record M0_Q0 ends
    s1 = filt['sections'][0]['corners']['M0_Q0']
    s6 = filt['sections'][-1]['corners']['M0_Q0']
    p1, pr1, _ = parse_root(s1['pole'])
    z1, zr1, _ = parse_root(s1['zero'])
    p6, pr6, _ = parse_root(s6['pole'])
    z6, zr6, _ = parse_root(s6['zero'])
    oct1 = math.log2(p1/z1) if p1 > 0 and z1 > 0 else 0
    oct6 = math.log2(p6/z6) if p6 > 0 and z6 > 0 else 0
    p2k_filter_ends.append((fidx, fname, ftype, oct1, p1, z1, oct6, p6, z6))

# Analyze Morpheus
morph_stage_octaves = defaultdict(list)
morph_morph_travel_poles = defaultdict(list)
morph_morph_travel_zeros = defaultdict(list)
morph_filter_ends = []

for cube in morph_data['cubes']:
    cname = cube['name']
    cidx = cube['index']
    for ci, corner in enumerate(cube['corners']):
        for si, sec in enumerate(corner['sections']):
            stage = si + 1
            p_hz, p_r = sec['pole']['hz'], sec['pole']['r']
            z_hz, z_r = sec['zero']['hz'], sec['zero']['r']
            w = sec['raw']
            sentinel = (w[1] == 2047 and w[3] == 2047)
            selfc = (w[0] == w[2] and w[1] == w[3]) and not sentinel
            idle = (w[0] == 1909 and w[1] == 2015 and w[3] == 2047)
            if not (sentinel or selfc or idle):
                if p_r > 0.02 and z_r > 0.02 and p_hz > 20 and z_hz > 20:
                    oct_sep = math.log2(p_hz / z_hz)
                    morph_stage_octaves[stage].append((cname, cidx, ci, p_hz, p_r, z_hz, z_r, oct_sep))

    # Morph travel on M axis (ci=0 vs ci=2)
    c0 = cube['corners'][0]
    c1 = cube['corners'][2]
    for si in range(min(len(c0['sections']), len(c1['sections']))):
        stage = si + 1
        s0 = c0['sections'][si]
        s1 = c1['sections'][si]
        w0, w1 = s0['raw'], s1['raw']
        live0 = not ((w0[1] == 2047 and w0[3] == 2047) or (w0[0] == 1909 and w0[1] == 2015 and w0[3] == 2047))
        live1 = not ((w1[1] == 2047 and w1[3] == 2047) or (w1[0] == 1909 and w1[1] == 2015 and w1[3] == 2047))
        if live0 and live1:
            p0, pr0 = s0['pole']['hz'], s0['pole']['r']
            p1, pr1 = s1['pole']['hz'], s1['pole']['r']
            z0, zr0 = s0['zero']['hz'], s0['zero']['r']
            z1, zr1 = s1['zero']['hz'], s1['zero']['r']
            if pr0 > 0.02 and pr1 > 0.02 and p0 > 20 and p1 > 20:
                morph_morph_travel_poles[stage].append((cname, math.log2(p1/p0)))
            if zr0 > 0.02 and zr1 > 0.02 and z0 > 20 and z1 > 20:
                morph_morph_travel_zeros[stage].append((cname, math.log2(z1/z0)))
                
    # Record Morpheus corner 0 ends
    live_secs = [s for s in c0['sections'] if not ((s['raw'][1]==2047 and s['raw'][3]==2047) or (s['raw'][0]==1909 and s['raw'][1]==2015 and s['raw'][3]==2047))]
    if live_secs:
        s_first = live_secs[0]
        s_last = live_secs[-1]
        p1, z1 = s_first['pole']['hz'], s_first['zero']['hz']
        p_last, z_last = s_last['pole']['hz'], s_last['zero']['hz']
        oct1 = math.log2(p1/z1) if p1 > 20 and z1 > 20 and s_first['pole']['r']>0.02 and s_first['zero']['r']>0.02 else 0
        oct_last = math.log2(p_last/z_last) if p_last > 20 and z_last > 20 and s_last['pole']['r']>0.02 and s_last['zero']['r']>0.02 else 0
        morph_filter_ends.append((cidx, cname, len(live_secs), oct1, p1, z1, oct_last, p_last, z_last))

print("\n" + "="*85)
print("=== P2K 33 ARCHITECTURES: ALL 33 FILTERS S1 & S6 AT M0_Q0 ===")
print("="*85)
print(f"{'Idx':>3} | {'Name':<16} | {'Type':<4} | {'S1 Oct':>7} | {'S1 Pole':>7} | {'S1 Zero':>7} | {'S6 Oct':>7} | {'S6 Pole':>7} | {'S6 Zero':>7}")
print("-" * 85)
for (fidx, fname, ftype, oct1, p1, z1, oct6, p6, z6) in sorted(p2k_filter_ends, key=lambda x: x[0]):
    print(f"{fidx:3d} | {fname:<16} | {ftype:<4} | {oct1:+6.2f}o | {p1:7.1f} | {z1:7.1f} | {oct6:+6.2f}o | {p6:7.1f} | {z6:7.1f}")

# Top Morpheus Spanning filters
print("\n" + "="*85)
print("=== MORPHEUS 289 CUBES: TOP REPRESENTATIVE SPANNING AND VOCAL CUBES ===")
print("="*85)
print(f"{'Idx':>3} | {'Name':<16} | {'Stages':>6} | {'S1 Oct':>7} | {'S1 Pole':>7} | {'S1 Zero':>7} | {'End Oct':>7} | {'End Pole':>8} | {'End Zero':>8}")
print("-" * 85)
notable_morpheus = [262, 72, 42, 23, 255, 62, 129, 266, 196, 141, 150, 159, 174, 180, 288, 63, 11, 7, 8]
for row in sorted(morph_filter_ends, key=lambda x: x[0]):
    if row[0] in notable_morpheus:
        cidx, cname, n_st, oct1, p1, z1, oct_last, p_last, z_last = row
        print(f"{cidx:3d} | {cname:<16} | {n_st:6d} | {oct1:+6.2f}o | {p1:7.1f} | {z1:7.1f} | {oct_last:+6.2f}o | {p_last:8.1f} | {z_last:8.1f}")
