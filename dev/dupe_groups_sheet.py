import math, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 44100.0
F = 20 * (1000.0) ** (np.arange(400) / 399)
w = 2 * np.pi * F / SR
def pair(hz, r): return 1 - 2 * r * math.cos(2 * math.pi * hz / SR) * np.exp(-1j * w) + r * r * np.exp(-2j * w)
def lvl(db): return db - db[np.argmin(np.abs(F - 100))]
bw = lambda r: -math.log(max(r, 1e-12)) * SR / math.pi

groups = [
    ("klub_klassik c0 = acid_ravage c0 = tooth_comb c0", '005_klub_klassik', 0),
    ("dead_ringer c1 = ooh_to_eee c0 = eeh_to_aah c1", '008_dead_ringer', 1),
    ("millennium c0 = meaty_gizmo c0  (full corner)", '003_millennium', 0),
    ("millennium c1 = meaty_gizmo c1  (full corner)", '003_millennium', 1),
    ("klub_klassik c1 = acid_ravage c1", '005_klub_klassik', 1),
    ("fuzzi_face c0 = cruz_pusher c0", '007_fuzzi_face', 0),
    ("ooh_to_eee c1 = eeh_to_aah c0", '010_ooh_to_eee', 1),
    ("boland_bass c0 = lucifer_s_q c0", '011_boland_bass', 0),
    ("boland_bass c1 = bass_tracer c1", '011_boland_bass', 1),
    ("talking_hedz c0 = ubu_orator c1", '013_talking_hedz', 0),
]
fig, axs = plt.subplots(len(groups), 7, figsize=(22, 2.1 * len(groups)), sharex=True, sharey=True)
for row, (title, body, ci) in zip(axs, groups):
    stages = dl.decode_p2k_body(open(f'ref/presets/P2k_{body}.bin', 'rb').read(), SR)[ci]
    casc = np.ones_like(F, dtype=complex)
    for si, g in enumerate(stages):
        a = row[si]; p = g.pole
        if p.kind == 'conjugate':
            h = 1 / pair(p.hz, p.r); casc *= h
            a.semilogx(F, lvl(20 * np.log10(np.abs(h))), color='#8b0000', lw=1.2)
            a.set_title(f"S{si+1}  {p.hz:.0f} Hz / {bw(p.r):.0f}", fontsize=7)
        elif p.kind == 'real':
            h = 1 / ((1 - p.root_a * np.exp(-1j * w)) * (1 - p.root_b * np.exp(-1j * w))); casc *= h
            a.semilogx(F, lvl(20 * np.log10(np.abs(h))), color='#8b0000', lw=1.2, ls='--')
            a.set_title(f"S{si+1}  real {p.root_a:.2f}/{p.root_b:.2f}", fontsize=7)
        else:
            a.set_title(f"S{si+1}  off", fontsize=7)
        a.grid(True, which='both', alpha=.2)
    row[6].semilogx(F, lvl(20 * np.log10(np.abs(casc))), color='k', lw=1.5)
    row[6].set_title('poles cascade', fontsize=7); row[6].grid(True, which='both', alpha=.2)
    row[0].set_ylabel(title, fontsize=7, rotation=0, ha='right', va='center')
axs[0][0].set_xlim(20, 20000); axs[0][0].set_ylim(-50, 50)
fig.suptitle('the 10 exact-copy groups: the shared pole half drawn once, section by section — every listed corner carries these bytes verbatim')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/dupe_groups_sheet.png', dpi=88)
print('ok')
