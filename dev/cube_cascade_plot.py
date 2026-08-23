import json, math
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 39062.5
F = 20 * (800.0) ** (np.arange(300) / 299)
w = 2 * np.pi * F / SR
e1, e2 = np.exp(-1j * w), np.exp(-2j * w)

def pair(hz, r):
    t = 2 * np.pi * hz / SR
    return 1 - 2 * r * math.cos(t) * e1 + r * r * e2

def corner_db(corner, with_zeros):
    h = np.ones_like(F, dtype=complex)
    for s in corner['sections']:
        p, z = s['pole'], s['zero']
        if p['r'] > 0.001:
            den = pair(p['hz'], p['r'])
            num = pair(z['hz'], z['r']) if (with_zeros and z['r'] > 0.001) else 1.0
            h *= num / den
    db = 20 * np.log10(np.abs(h) + 1e-12)
    return db - db[np.argmin(np.abs(F - 100))]

cubes = json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']
full, bare = [], []
for c in cubes:
    for corner in c['corners']:
        if all(s['pole']['r'] <= 0.001 for s in corner['sections']): continue
        full.append(corner_db(corner, True)); bare.append(corner_db(corner, False))
full, bare = np.array(full), np.array(bare)
top = slice(np.argmin(np.abs(F - 8000)), np.argmin(np.abs(F - 16000)))
print(f"{len(full)} corners; level 8-16 kHz re 100 Hz: poles only median {np.median(bare[:, top]):.0f} dB, with zeros {np.median(full[:, top]):.0f} dB")
print(f"peak above 100 Hz: poles only median {np.median(bare.max(1)):.0f} dB, with zeros {np.median(full.max(1)):.0f} dB")

fig, ax = plt.subplots(1, 2, figsize=(15, 6.5), sharey=True)
for a, data, nm in [(ax[0], bare, 'poles only (skeletons)'), (ax[1], full, 'poles + zeros (as shipped)')]:
    for d in data: a.semilogx(F, d, color='#236', lw=.4, alpha=.05)
    a.semilogx(F, np.median(data, 0), color='#c44', lw=2, label='median')
    a.set_ylim(-90, 50); a.set_xlim(20, 16000); a.grid(True, which='both', alpha=.3)
    a.set_title(f'289 cubes, {len(data)} corners: cascade response, {nm}'); a.legend(); a.set_xlabel('Hz')
ax[0].set_ylabel('dB re 100 Hz')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/cube_cascade.png', dpi=110)
