import json, math, collections
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

cuts = json.load(open('ref/stage_vocabulary.json'))['cuts']
SR = 39062.5
OFF = cuts['root_off_r']
GRID = np.logspace(math.log10(40), math.log10(18000), 512)
W = 2*np.pi*GRID/SR

def sec_db(p, z):
    def part(hz, r):
        if r <= 0: return np.zeros_like(W)
        th = 2*np.pi*hz/SR
        return 10*np.log10(np.maximum((1 - 2*r*np.cos(th)*np.cos(W) + r*r*np.cos(2*W))**2
                                      + (2*r*np.cos(th)*np.sin(W) - r*r*np.sin(2*W))**2, 1e-30))
    return part(z['hz'], z['r']) - part(p['hz'], p['r'])

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
seqs = collections.Counter(); ex = {}
for cu in cubes:
    for ci, c in enumerate(cu['corners']):
        s = tuple(x for x in (cls(sec['pole'], sec['zero']) for sec in c['sections'][:7]) if x)
        if len(s) >= 6:
            seqs[s] += 1
            ex.setdefault(s, (cu['name'], ci, c))

top = seqs.most_common(4)
fig, axes = plt.subplots(2, 4, figsize=(20, 9), facecolor='white')
for col, (s, n) in enumerate(top):
    name, ci, corner = ex[s]
    stages = [sec for sec in corner['sections'][:7] if cls(sec['pole'], sec['zero'])]
    a = axes[0][col]
    for k, sec in enumerate(stages):
        a.semilogx(GRID, sec_db(sec['pole'], sec['zero']), lw=1.2, label=f"S{k+1} {cls(sec['pole'],sec['zero'])}")
    a.set_title(f"{n}x  {name} c{ci}\nper-section", fontsize=10)
    a.set_ylim(-60, 45); a.grid(alpha=.25); a.legend(fontsize=6)
    b = axes[1][col]
    tot = np.zeros_like(GRID)
    for sec in stages:
        tot = tot + sec_db(sec['pole'], sec['zero'])
        b.semilogx(GRID, tot, lw=.8, alpha=.45, color='#888')
    b.semilogx(GRID, tot, lw=2.2, color='#d62728')
    b.set_title('signal so far → sum', fontsize=10)
    b.set_ylim(-60, 45); b.grid(alpha=.25); b.set_xlabel('Hz')
plt.suptitle('Morpheus: the four most repeated full-cascade templates — what they actually sound like', fontsize=13)
plt.tight_layout()
plt.savefig('plots/template_magnitudes.png', dpi=105)
for s, n in top: print(n, ' → '.join(s), '|', ex[s][0])
