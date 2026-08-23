import math, re, sys, collections
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl

SR = 44100.0
bw = lambda r: -math.log(max(r, 1e-12)) * SR / math.pi

groups = []
for line in open('dev/pca/skeleton_library.txt'):
    m = re.match(r'posture (\d+):', line)
    if m: groups.append([])
    m2 = re.match(r'\s+(\S+) c(\d)', line)
    if m2: groups[-1].append((m2.group(1), int(m2.group(2))))

def corner_data(body, ci):
    raw = dl.parse_p2k_bytes(open(f'ref/presets/P2k_{body}.bin', 'rb').read())[ci]
    geo = [dl.geometry_from_words_at(w, SR) for w in raw]
    out = []
    for si, (w, g) in enumerate(zip(raw, geo)):
        if g.pole.kind == 'conjugate' and g.pole.hz < 8000 and bw(g.pole.r) < 1500:
            out.append({'si': si, 'hz': g.pole.hz, 'r': g.pole.r, 'pmag': w[2], 'prsq': w[3],
                        'zmag': w[0], 'zrsq': w[1],
                        'zero': g.zero})
    return sorted(out, key=lambda d: d['hz'])

for gi, members in enumerate(groups, 1):
    datas = [corner_data(b, c) for b, c in members]
    n = min(len(d) for d in datas)
    print(f"\nposture {gi} ({len(members)} corners: " + ", ".join(f"{b} c{c}" for b, c in members) + ")")
    for k in range(n):
        ref = datas[0][k]
        pm = [d[k]['pmag'] for d in datas]; pr = [d[k]['prsq'] for d in datas]
        hz = [d[k]['hz'] for d in datas]; rr = [d[k]['r'] for d in datas]
        cents = max(abs(1200 * math.log2(h / ref['hz'])) for h in hz)
        ident_m = len(set(pm)) == 1; ident_r = len(set(pr)) == 1
        print(f"  lane {k+1} ~{ref['hz']:6.0f} Hz: pole mag words {sorted(set(pm))} "
              f"{'IDENTICAL' if ident_m else f'span {max(pm)-min(pm)} units / {cents:.1f} c'};"
              f"  rsq words {sorted(set(pr))} "
              f"{'IDENTICAL' if ident_r else f'span {max(pr)-min(pr)} units / bw {min(bw(x) for x in rr):.0f}-{max(bw(x) for x in rr):.0f} Hz'}")
    zm = [tuple(sorted((d['zmag'], d['zrsq']) for d in data)) for data in datas]
    zsame = len(set(zm)) == 1
    zw = collections.Counter()
    for data in datas:
        for d in data: zw[(d['zmag'], d['zrsq'])] += 1
    shared = {k: v for k, v in zw.items() if v > 1}
    print(f"  zeros on these lanes: whole zero-sets identical across members: {zsame}; "
          f"individual zero words shared by >1 member: {len(shared)} of {len(zw)} distinct"
          + (f"  {[(k, v) for k, v in sorted(shared.items(), key=lambda x: -x[1])][:4]}" if shared else ""))
