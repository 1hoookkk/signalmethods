import glob, json, math, os, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

MSR = 39062.5
cubes = json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']
cp, cz = [], []
for c in cubes:
    for corner in c['corners']:
        for s in corner['sections']:
            p, z = s['pole'], s['zero']
            if p['r'] > 0.001: cp.append((p['hz'], p['r']))
            if z['r'] > 0.001: cz.append((z['hz'], z['r']))
cp = np.array(cp); cz = np.array(cz)
ncorners = sum(len(c['corners']) for c in cubes)
print(f"289 cubes, {ncorners} corners: {len(cp)} poles, {len(cz)} zeros")

bw = lambda r, sr: -np.log(r) * sr / math.pi
sharp = cp[(cp[:, 0] < 8000) & (bw(cp[:, 1], MSR) < 1500)]
keys = {(round(1200 * math.log2(h) / 20), round(bw(r, MSR) / 25)) for h, r in sharp}
print(f"audible cube poles (<8 kHz, bw<1500): {len(sharp)}; distinct at 20 c x 25 Hz: {len(keys)}")

pp = []
for path in sorted(glob.glob('ref/presets/P2k_0*.bin')):
    for stages in dl.decode_p2k_body(open(path, 'rb').read(), 44100.0):
        for g in stages:
            if g.pole.kind == 'conjugate' and g.pole.hz < 8000 and bw(g.pole.r, 44100.0) < 1500:
                pp.append(g.pole.hz)
pp = np.array(pp)
near = np.array([np.min(np.abs(1200 * np.log2(pp / h))) for h, _ in sharp])
print(f"cube pole to nearest P2K bank pole: within 20 c {np.mean(near<20)*100:.0f}%, within 50 c {np.mean(near<50)*100:.0f}%, median {np.median(near):.0f} c")
hist, edges = np.histogram(1200 * np.log2(sharp[:, 0] / 20), bins=40)
top = np.argsort(hist)[::-1][:8]
print("busiest cube pole frequencies:", ", ".join(f"{20*2**(edges[i]/1200):.0f} Hz x{hist[i]}" for i in sorted(top)))

fig, ax = plt.subplots(1, 2, figsize=(14, 6.5))
th = np.linspace(0, math.pi, 200)
for a, (pts, col, nm) in zip(ax, [(cp, '#c44', 'poles'), (cz, '#236', 'zeros')]):
    a.plot(np.cos(th), np.sin(th), 'k', lw=.6)
    ang = 2 * np.pi * pts[:, 0] / MSR
    a.scatter(pts[:, 1] * np.cos(ang), pts[:, 1] * np.sin(ang), s=4, c=col, alpha=.25)
    a.set_aspect('equal'); a.set_title(f'289 Morpheus cubes, all corners: {nm} ({len(pts)})')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/cube_poles.png', dpi=110)
