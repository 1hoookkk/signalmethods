import json, math, collections
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 39062.5
F = 20 * (800.0) ** (np.arange(300) / 299)
w = 2 * np.pi * F / SR
e1, e2 = np.exp(-1j * w), np.exp(-2j * w)
def pair(hz, r): return 1 - 2 * r * math.cos(2 * np.pi * hz / SR) * e1 + r * r * e2
bw = lambda r: -math.log(r) * SR / math.pi

cubes = json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']
groups = collections.defaultdict(lambda: {'cubes': set(), 'corners': 0, 'poles': None})
for c in cubes:
    for corner in c['corners']:
        poles = [(s['pole']['hz'], s['pole']['r']) for s in corner['sections'] if s['pole']['r'] > 0.001]
        if not poles: continue
        key = tuple(sorted((round(1200 * math.log2(h) / 20), round(bw(r) / 25)) for h, r in poles))
        g = groups[key]; g['cubes'].add(c['name']); g['corners'] += 1; g['poles'] = poles

rec = sorted([g for g in groups.values() if len(g['cubes']) >= 3], key=lambda g: -len(g['cubes']))
print(f"{len(groups)} distinct skeletons over {sum(g['corners'] for g in groups.values())} corners; {len(rec)} recur in >=3 cubes, covering {sum(g['corners'] for g in rec)} corners")
fig, ax = plt.subplots(figsize=(14, 7))
for i, g in enumerate(rec[:12]):
    h = np.ones_like(F, dtype=complex)
    for hz, r in g['poles']: h /= pair(hz, r)
    db = 20 * np.log10(np.abs(h)); db -= db[np.argmin(np.abs(F - 100))]
    desc = " ".join(f"{hz:.0f}" for hz, _ in sorted(g['poles']) if hz < 8000 and bw(_) < 1500)
    print(f"  {len(g['cubes']):3d} cubes {g['corners']:4d} corners  poles: {desc}")
    ax.semilogx(F, db, lw=1.8, label=f"{len(g['cubes'])} cubes: {desc}")
ax.set_xlim(20, 16000); ax.set_ylim(-80, 60); ax.grid(True, which='both', alpha=.3)
ax.set_title('Recurring cube skeletons (same pole set in >=3 cubes), poles only'); ax.legend(fontsize=7); ax.set_xlabel('Hz'); ax.set_ylabel('dB re 100 Hz')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/cube_recurring.png', dpi=110)
