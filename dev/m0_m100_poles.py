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
bw = lambda r: -math.log(r) * SR / math.pi

bodies = [('bassbox_303', 'ref/presets/P2k_006_bassbox_303.bin'), ('tb_or_not_tb', 'ref/presets/P2k_009_tb_or_not_tb.bin')]
fig, axs = plt.subplots(len(bodies) * 2, 7, figsize=(22, 2.3 * len(bodies) * 2), sharex=True, sharey=True)
row = 0
for name, path in bodies:
    corners = dl.decode_p2k_body(open(path, 'rb').read(), SR)
    for ci, label in ((0, 'M0 Q0'), (1, 'M100 Q0')):
        casc = np.ones_like(F, dtype=complex); desc = []
        for si, g in enumerate(corners[ci]):
            a = axs[row][si]; p = g.pole
            if p.kind == 'conjugate':
                h = 1 / pair(p.hz, p.r); casc *= h
                a.semilogx(F, lvl(20 * np.log10(np.abs(h))), color='#8b0000', lw=1.2)
                a.set_title(f"S{si+1} {p.hz:.0f} Hz / {bw(p.r):.0f} Hz wide", fontsize=8)
                desc.append(f"{p.hz:.0f}/{bw(p.r):.0f}")
            elif p.kind == 'real':
                c = (1 - p.root_a * np.exp(-1j * w)) * (1 - p.root_b * np.exp(-1j * w)); h = 1 / c; casc *= h
                a.semilogx(F, lvl(20 * np.log10(np.abs(h))), color='#8b0000', lw=1.2, ls='--')
                a.set_title(f"S{si+1} real pair {p.root_a:.3f} {p.root_b:.3f}", fontsize=8)
                desc.append(f"real({p.root_a:.2f},{p.root_b:.2f})")
            else:
                a.set_title(f"S{si+1} off", fontsize=8)
            a.grid(True, which='both', alpha=.2)
        axs[row][6].semilogx(F, lvl(20 * np.log10(np.abs(casc))), color='k', lw=1.6)
        axs[row][6].set_title('poles cascade', fontsize=8); axs[row][6].grid(True, which='both', alpha=.2)
        axs[row][0].set_ylabel(f"{name}\n{label}", fontsize=9, rotation=0, ha='right', va='center')
        print(f"{name:13s} {label:8s} " + "  ".join(desc))
        row += 1
axs[0][0].set_xlim(20, 20000); axs[0][0].set_ylim(-50, 50)
fig.suptitle('the two 303 bodies: M0 and M100 corners, poles only — each section alone and the cascade (44.1 kHz, level at 100 Hz)')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/m0_m100_poles_303.png', dpi=90)
