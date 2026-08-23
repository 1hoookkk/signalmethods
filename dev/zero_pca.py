import glob, math, os, sys
import numpy as np
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'cell_dictionary'))
import decode_lib as dl
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 44100.0
bodies = sorted(glob.glob('ref/presets/P2k_0*.bin'))
zeros, poles, masks, names, corner_of = [], [], [], [], []
pole_q = []
for path in bodies:
    name = os.path.basename(path)[8:-4]
    corners = dl.decode_p2k_body(open(path, 'rb').read(), SR)
    for ci, stages in enumerate(corners):
        mask = []
        for si, g in enumerate(stages):
            z, p = g.zero, g.pole
            if z.kind == 'conjugate' and z.r > 0.001:
                zeros.append((z.hz, z.r, ci, name))
                mask += [math.log2(z.hz), math.log(max(1 - z.r, 1e-6))]
            else:
                mask += [math.log2(16000.0), math.log(1.0)]
            if p.kind == 'conjugate':
                poles.append((p.hz, p.r, ci, name))
        masks.append(mask); names.append(name); corner_of.append(ci)
    for ci in (0, 1):
        for si in range(6):
            a, b = corners[ci][si].pole, corners[ci + 2][si].pole
            if a.kind == 'conjugate' and b.kind == 'conjugate':
                bw = lambda r: -math.log(r) * SR / math.pi
                pole_q.append((name, ci, si, a.hz, b.hz, 1200 * math.log2(b.hz / a.hz), bw(a.r), bw(b.r)))

Z = np.array([[math.log2(h), math.log(max(1 - r, 1e-6))] for h, r, _, _ in zeros])
Zc = Z - Z.mean(0)
U, S, Vt = np.linalg.svd(Zc, full_matrices=False)
var = S**2 / (S**2).sum()
print(f"zeros: {len(Z)} conjugate of {len(masks)*6}; per-zero PCA (log2 hz, log(1-r)): PC1 {var[0]*100:.1f}% dir {Vt[0].round(3)}, PC2 {var[1]*100:.1f}%")

M = np.array(masks); Mc = M - M.mean(0)
U2, S2, Vt2 = np.linalg.svd(Mc, full_matrices=False)
v2 = S2**2 / (S2**2).sum()
print("per-corner mask PCA (12-dim): cumulative", [f"{x*100:.0f}%" for x in np.cumsum(v2)[:6]])

moved = [r for r in pole_q if abs(r[5]) > 20]
bwmoved = [r for r in pole_q if abs(math.log(r[7] / r[6])) > 0.1]
print(f"pole lanes Q0->Q100: {len(pole_q)}; freq moved >20 cents: {len(moved)}; width changed >10%: {len(bwmoved)}")
print("  bodies with moving pole freq:", sorted({r[0] for r in moved}))
narrower = sum(1 for r in bwmoved if r[7] < r[6])
print(f"  of width changes, narrower at Q100: {narrower}/{len(bwmoved)}; median Q0 bw {np.median([r[6] for r in pole_q]):.0f} Hz, Q100 {np.median([r[7] for r in pole_q]):.0f} Hz")
for r in moved[:12]: print("   ", r[0], "corner", r[1], "S%d" % (r[2]+1), f"{r[3]:.0f}->{r[4]:.0f} Hz ({r[5]:+.0f} c)")

fig, ax = plt.subplots(1, 2, figsize=(14, 6.5))
th = np.linspace(0, math.pi, 200)
ax[0].plot(np.cos(th), np.sin(th), 'k', lw=.6)
zx = np.array([r * math.cos(2 * math.pi * h / SR) for h, r, _, _ in zeros])
zy = np.array([r * math.sin(2 * math.pi * h / SR) for h, r, _, _ in zeros])
px = np.array([r * math.cos(2 * math.pi * h / SR) for h, r, _, _ in poles])
py = np.array([r * math.sin(2 * math.pi * h / SR) for h, r, _, _ in poles])
ax[0].scatter(px, py, s=8, c='#c44', alpha=.35, label=f'poles ({len(poles)})')
ax[0].scatter(zx, zy, s=8, c='#236', alpha=.5, label=f'zeros ({len(zeros)})')
ax[0].set_aspect('equal'); ax[0].legend(); ax[0].set_title('the 33 bodies, 4 corners: zeros on the z-plane (armadillo)')
sc = U2[:, :2] * S2[:2]
ax[1].scatter(sc[:, 0], sc[:, 1], c=corner_of, cmap='viridis', s=28)
for (x, y), n, c in zip(sc, names, corner_of):
    if c == 0: ax[1].annotate(n[:10], (x, y), fontsize=6, alpha=.8)
ax[1].set_title(f'per-corner mask PCA: PC1 {v2[0]*100:.0f}%  PC2 {v2[1]*100:.0f}%  (colour = corner)')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/zero_pca.png', dpi=110)

sharp = [r for r in pole_q if r[3] < 8000 and r[6] < 1500]
ms = [r for r in sharp if abs(r[5]) > 20]
print(f"\naudible poles only (Q0 < 8 kHz, bw < 1500 Hz): {len(sharp)} lanes; freq moved >20 cents at Q100: {len(ms)} ({100*len(ms)/len(sharp):.0f}%); median |move| {np.median([abs(r[5]) for r in sharp]):.0f} c")
print(f"  width Q0 median {np.median([r[6] for r in sharp]):.0f} Hz -> Q100 {np.median([r[7] for r in sharp]):.0f} Hz; narrower in {sum(1 for r in sharp if r[7] < r[6])}/{len(sharp)}")
for r in sorted(ms, key=lambda r: -abs(r[5]))[:10]: print("   ", r[0], "c%d S%d" % (r[1], r[2]+1), f"{r[3]:.0f}->{r[4]:.0f} Hz ({r[5]:+.0f} c) bw {r[6]:.0f}->{r[7]:.0f}")

near = []
for path in bodies:
    corners = dl.decode_p2k_body(open(path, 'rb').read(), SR)
    for ci in (0, 1):
        a = [g.pole for g in corners[ci] if g.pole.kind == 'conjugate' and g.pole.hz < 8000 and -math.log(g.pole.r) * SR / math.pi < 1500]
        b = [g.pole for g in corners[ci + 2] if g.pole.kind == 'conjugate']
        for p in a:
            near.append(min(abs(1200 * math.log2(q.hz / p.hz)) for q in b))
near = np.array(near)
print(f"nearest Q100 pole to each audible Q0 pole, any slot: within 20 c {np.mean(near<20)*100:.0f}%, within 100 c {np.mean(near<100)*100:.0f}%, median {np.median(near):.0f} c")
