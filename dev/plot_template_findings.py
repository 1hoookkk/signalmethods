import json, math, collections, statistics as st
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

cuts = json.load(open('ref/stage_vocabulary.json'))['cuts']
SR = 39062.5
OFF = cuts['root_off_r']

def bw(r): return -math.log(r)*SR/math.pi if 0 < r < 1 else 0

def cls(p, z):
    ph, pr, zh, zr = p['hz'], p['r'], z['hz'], z['r']
    a, b = pr > OFF, zr > OFF
    if not a and not b: return None
    if a and not b: return 'peak' if bw(pr) <= cuts['sharp_bw_hz'] else 'lift'
    if b and not a: return 'notch' if (zr < 1 and bw(zr) <= cuts['sharp_bw_hz']) else 'dip'
    if zr >= cuts['unit_zero_r']: return 'guarded peak'
    s = abs(12*math.log2(zh/ph)) if ph > 0 and zh > 0 else 99
    if s < 0.17 and abs(pr-zr) < 0.01: return 'parked pair'
    if s > cuts['near_interval_st']: return 'low shelf' if zh > ph else 'high shelf'
    return 'bell' if bw(zr) >= 2*bw(pr) else ('carved band' if bw(pr) >= 2*bw(zr) else 'close pair')

cubes = json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']

ratios = []
for cu in cubes:
    for c in cu['corners']:
        f = sorted(s['pole']['hz'] for s in c['sections'][:7] if s['pole']['r'] > OFF)
        f = [x for x in f if x > 20]
        ratios += [12*math.log2(f[i+1]/f[i]) for i in range(len(f)-1) if f[i] > 20 and f[i+1]/f[i] > 1.0001]

AX = [(1, 'Transform 2'), (2, 'Morph'), (4, 'Frequency')]
moves = {n: [] for _, n in AX}
for cu in cubes:
    cs = cu['corners']
    for i in range(8):
        for bit, name in AX:
            j = i ^ bit
            if j < i: continue
            d = []
            for x, y in zip(cs[i]['sections'][:7], cs[j]['sections'][:7]):
                if x['pole']['r'] > OFF and y['pole']['r'] > OFF and x['pole']['hz'] > 20 and y['pole']['hz'] > 20:
                    d.append(abs(12*math.log2(y['pole']['hz']/x['pole']['hz'])))
            if d: moves[name].append(st.mean(d))

held = collections.Counter()
for cu in cubes:
    seqs = set()
    for c in cu['corners']:
        s = tuple(x for x in (cls(sec['pole'], sec['zero']) for sec in c['sections'][:7]) if x)
        if s: seqs.add(s)
    if seqs: held[len(seqs)] += 1

pos = [collections.Counter() for _ in range(7)]
for cu in cubes:
    for c in cu['corners']:
        for i, sec in enumerate(c['sections'][:7]):
            k = cls(sec['pole'], sec['zero'])
            if k: pos[i][k] += 1

fig, ax = plt.subplots(2, 2, figsize=(15, 10), facecolor='white')

a = ax[0][0]
a.hist(ratios, bins=120, range=(0, 36), color='#1f77b4')
a.axvline(6, color='#d62728', ls='--', lw=2, label='6.00 st  (ratio 1.414 = √2)')
a.axvline(12, color='#999', ls=':', lw=1.5, label='octave')
a.set_title(f'Spacing between consecutive poles within a corner\nmedian {st.median(ratios):.2f} st   n={len(ratios)}')
a.set_xlabel('semitones'); a.set_ylabel('count'); a.legend()

a = ax[0][1]
names = [n for _, n in AX]
vals = [st.mean(moves[n]) for n in names]
a.bar(names, vals, color=['#8c8c8c', '#d62728', '#1f77b4'])
for i, v in enumerate(vals): a.text(i, v+0.4, f'{v:.1f} st', ha='center')
a.set_title('How far each cube axis moves the poles\n(mean |shift| over one-bit corner pairs)')
a.set_ylabel('semitones')

a = ax[1][0]
ks = sorted(held)
a.bar([str(k) for k in ks], [held[k] for k in ks], color='#2ca02c')
a.set_title('Distinct shape-sequences per cube (across its 8 corners)\n1 = the card never changes')
a.set_xlabel('distinct sequences'); a.set_ylabel('cubes')

a = ax[1][1]
kinds = [k for k, _ in collections.Counter({k: sum(p[k] for p in pos) for p in pos for k in p}).most_common()]
kinds = sorted({k for p in pos for k in p}, key=lambda k: -sum(p[k] for p in pos))[:6]
bottom = [0]*7
for k in kinds:
    v = [pos[i][k] for i in range(7)]
    a.bar(range(1, 8), v, bottom=bottom, label=k)
    bottom = [b+x for b, x in zip(bottom, v)]
a.set_title('Shape by stage position — Morpheus 289 cubes')
a.set_xlabel('stage'); a.set_ylabel('active sections'); a.legend(fontsize=8)

plt.tight_layout()
plt.savefig('plots/template_findings.png', dpi=110)
print('median spacing', round(st.median(ratios), 3), 'st')
print({n: round(st.mean(moves[n]), 2) for n in names})
print('cubes with a single held card:', held.get(1, 0), 'of', sum(held.values()))
