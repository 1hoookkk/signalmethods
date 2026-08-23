import glob, json, math, collections, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

MSR, PSR = 39062.5, 44100.0
bw = lambda r, sr: -math.log(r) * sr / math.pi
def audible(hz, r, sr): return r > 0.001 and 40 < hz < 8000 and bw(r, sr) < 1500

cubes = json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']
cp = []
for c in cubes:
    for corner in c['corners']:
        for s in corner['sections']:
            h, r = s['pole']['hz'], s['pole']['r']
            if audible(h, r, MSR): cp.append((h, bw(r, MSR), c['name']))
pp = []
for path in sorted(glob.glob('ref/presets/P2k_0*.bin')):
    for stages in dl.decode_p2k_body(open(path, 'rb').read(), PSR):
        for g in stages:
            if g.pole.kind == 'conjugate' and audible(g.pole.hz, g.pole.r, PSR): pp.append((g.pole.hz, bw(g.pole.r, PSR)))

cells = collections.defaultdict(list)
for h, b, n in cp: cells[(round(1200 * math.log2(h / 40) / 30), round(math.log2(b) * 2))].append((h, b, n))
prims = []
for k, v in cells.items():
    names = {n for _, _, n in v}
    if len(names) >= 3:
        hz = math.exp(np.mean([math.log(h) for h, _, _ in v])); b = math.exp(np.mean([math.log(x) for _, x, _ in v]))
        match = any(abs(1200 * math.log2(ph / hz)) < 20 and abs(math.log2(pb / b)) < 0.5 for ph, pb in pp)
        prims.append((hz, b, len(v), len(names), match, sorted(names)[:4]))
prims.sort(key=lambda p: -p[3])
print(f"{len(cp)} audible cube poles -> {len(cells)} cells (30 c x half-octave width); {len(prims)} primitives used by >=3 cubes, covering {sum(p[2] for p in prims)} poles ({100*sum(p[2] for p in prims)/len(cp):.0f}%)")
print(f"primitives also present in the P2K bank (20 c, width within 2x): {sum(p[4] for p in prims)}/{len(prims)}")
print(f"bank audible poles ({len(pp)}) with a cube primitive within 20 c / 2x width: {sum(any(abs(1200*math.log2(p[0]/ph))<20 and abs(math.log2(p[1]/pb))<0.5 for p in prims) for ph,pb in pp)}/{len(pp)}")
for p in prims[:40]: print(f"  {p[0]:6.0f} Hz  bw {p[1]:5.0f}  x{p[2]:3d} in {p[3]:2d} cubes  {'P2K' if p[4] else '   '}  {', '.join(p[5])}")
open('dev/e2e/mask_proof/cube_primitives.txt','w').write("\n".join(f"{p[0]:.0f} Hz bw {p[1]:.0f} x{p[2]} in {p[3]} cubes {'P2K' if p[4] else '-'}" for p in prims))

F = 20 * (800.0) ** (np.arange(300) / 299)
fig, ax = plt.subplots(figsize=(14, 7))
for h, b, n, k, m, _ in prims:
    r = math.exp(-math.pi * b / MSR); w = 2 * np.pi * F / MSR; t = 2 * np.pi * h / MSR
    H = 1 / (1 - 2 * r * math.cos(t) * np.exp(-1j * w) + r * r * np.exp(-2j * w))
    db = 20 * np.log10(np.abs(H)); db -= db[0]
    ax.semilogx(F, db, color='#c44' if m else '#236', lw=0.5 + k / 8, alpha=.6)
ax.set_xlim(40, 16000); ax.set_ylim(-30, 60); ax.grid(True, which='both', alpha=.3)
ax.set_title(f'{len(prims)} primitive pole frames across the 289 cubes (one pole pair each; line weight = how many cubes use it). red = also in the P2K bank, blue = cubes only')
ax.set_xlabel('Hz'); ax.set_ylabel('dB')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/cube_primitives.png', dpi=110)
