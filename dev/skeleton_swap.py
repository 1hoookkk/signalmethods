import glob, json, math, os, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl
sys.path.insert(0, 'dev'); from recipe_measured_templates import peaks
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 44100.0
F = 20 * (1000.0) ** (np.arange(400) / 399)
w = 2 * np.pi * F / SR
def pair(hz, r): return 1 - 2 * r * math.cos(2 * math.pi * hz / SR) * np.exp(-1j * w) + r * r * np.exp(-2j * w)
def r_of_bw(bw): return math.exp(-math.pi * bw / SR)
def body(path): return dl.decode_p2k_body(open(path, 'rb').read(), SR)
def poles_of(corner): return [(g.pole.hz, g.pole.r) for g in corner if g.pole.kind == 'conjugate']
def zeros_of(corner): return [(g.zero.hz, g.zero.r) for g in corner if g.zero.kind == 'conjugate']

hedz = body('ref/presets/P2k_013_talking_hedz.bin')[0]
Z = zeros_of(hedz)
def response(poles, zeros):
    h = np.ones_like(F, dtype=complex)
    for hz, r in poles: h /= pair(hz, r)
    for hz, r in zeros: h *= pair(hz, r)
    db = 20 * np.log10(np.abs(h) + 1e-12)
    return db - db[np.argmin(np.abs(F - 100))]

skels = [('hedz own poles', poles_of(hedz)),
         ('no poles (zeros alone)', []),
         ('dead_ringer c1', poles_of(body('ref/presets/P2k_008_dead_ringer.bin')[1])),
         ('klub_klassik c2', poles_of(body('ref/presets/P2k_005_klub_klassik.bin')[2])),
         ('dj_alkaline c0', poles_of(body('ref/presets/P2k_015_dj_alkaline.bin')[0]))]
a = np.loadtxt(glob.glob('recipes/vocal/dvtd/subject-1/s1-01-*/*.txt')[0], skiprows=1); m = (a[:, 0] > 1) & (a[:, 1] > 0)
skels.append(('mouth s1 bahn a', [(h, r_of_bw(b)) for h, b in peaks(a[m, 0], 20 * np.log10(a[m, 1]))]))
d = json.load(open('recipes/tfs/uiowa_cello_mf.tf.json'))
skels.append(('cello body', [(h, r_of_bw(b)) for h, b in peaks(np.array(d['freqs_hz']), np.array(d['mag_db']))]))

fig, axs = plt.subplots(len(skels), 1, figsize=(12, 2.2 * len(skels)), sharex=True, sharey=True)
for ax, (name, P) in zip(axs, skels):
    only_p = response(P, []); full = response(P, Z)
    ax.semilogx(F, only_p, color='#c44', lw=1, alpha=.6, label='poles only')
    ax.semilogx(F, full, color='k', lw=1.6, label='+ hedz zeros')
    ax.set_title(f"{name}: poles " + " ".join(f"{h:.0f}" for h, _ in sorted(P)), fontsize=9, loc='left')
    ax.grid(True, which='both', alpha=.25)
    top = slice(np.argmin(np.abs(F - 8000)), None)
    print(f"{name:26s} top octave poles-only {np.median(only_p[top]):7.1f} dB, with hedz zeros {np.median(full[top]):6.1f} dB; notch floor {full.min():6.1f} dB at {F[full.argmin()]:.0f} Hz")
axs[0].legend(fontsize=8); axs[0].set_ylim(-70, 40); axs[0].set_xlim(20, 20000)
fig.suptitle("Talking Hedz's six zeros held, the skeleton swapped underneath (level at 100 Hz)")
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/skeleton_swap.png', dpi=100)
