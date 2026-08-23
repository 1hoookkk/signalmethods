import glob, json, math, os, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 44100.0
F = 20 * (1000.0) ** (np.arange(300) / 299)
w = 2 * np.pi * F / SR
def pole_db(hz, bw):
    r = math.exp(-math.pi * bw / SR); t = 2 * math.pi * hz / SR
    return -20 * np.log10(np.abs(1 - 2 * r * math.cos(t) * np.exp(-1j * w) + r * r * np.exp(-2j * w)))
bw_of_r = lambda r: -math.log(r) * SR / math.pi

skel = []
ph = json.load(open('recipes/tables/phonetic_formants.json'))
for k, v in ph['vowels'].items():
    skel.append(('phonetic', f"{v['ipa']} {k}", [(v[f'f{i}'], v[f'b{i}']) for i in range(1, 6) if v.get(f'f{i}')]))
for p in sorted(glob.glob('recipes/poses/*.json')):
    d = json.load(open(p)); skel.append(('pose', d['name'], [(f['hz'], f['bandwidth_hz']) for f in d['formants']]))
dv = json.load(open('recipes/tables/dvtd_formants.json'))
for m in dv['mouths'][::4]:
    skel.append(('dvtd', m['name'], [(pk['hz'], pk['bandwidth_hz']) for pk in m['peaks']]))
ab = json.load(open('recipes/tables/acoustic_body_resonances.json'))
for k, v in ab['instruments'].items():
    skel.append(('instrument', v['name'], [(m['fp'], bw_of_r(m['rp'])) for m in v['modes_hz']]))
bells = json.load(open('recipes/tables/inharmonic_bell_ratios.json'))
for k, v in bells['archetypes'].items():
    skel.append(('bell @220', v['name'], [(220 * r, bw_of_r(rr)) for r, rr in zip(v['ratios'], v['default_radii']) if 220 * r < 18000]))
sy = json.load(open('recipes/tables/synth_filter_archetypes.json'))
for k, v in sy['archetypes'].items():
    skel.append(('synth', v['name'], [(s['pole_hz'], bw_of_r(s['pole_r'])) for s in v['stages'] if s.get('pole_hz')]))
for p in sorted(glob.glob('recipes/hero/*.body')):
    if os.path.getsize(p) != 240: continue
    c0 = dl.decode_p2k_body(open(p, 'rb').read(), SR)[0]
    skel.append(('hero', os.path.basename(p)[:-5], [(g.pole.hz, bw_of_r(g.pole.r)) for g in c0 if g.pole.kind == 'conjugate' and g.pole.r > 0.5]))

print(f"{len(skel)} candidate skeletons")
for src in sorted({s[0] for s in skel}):
    print(f"  {src}: {sum(1 for s in skel if s[0]==src)}")
cols = 6; rows = math.ceil(len(skel) / cols)
fig, axs = plt.subplots(rows, cols, figsize=(22, 2.4 * rows), sharex=True, sharey=True); axs = axs.ravel()
colors = {'phonetic': '#c44', 'pose': '#d80', 'dvtd': '#4a4', 'instrument': '#26a', 'bell @220': '#a3c', 'synth': '#888', 'hero': '#000'}
for a, (src, name, poles) in zip(axs, skel):
    tot = np.zeros_like(F)
    for hz, bw in poles:
        d = pole_db(hz, bw); d -= d[0]; tot += d
        a.semilogx(F, d, color=colors[src], lw=.6, alpha=.5)
    a.semilogx(F, tot, color='k', lw=1.4)
    a.set_title(f"[{src}] {name[:28]}  " + " ".join(f"{h:.0f}" for h, _ in sorted(poles)), fontsize=7)
    a.grid(True, which='both', alpha=.2)
for a in axs[len(skel):]: a.axis('off')
axs[0].set_xlim(20, 20000); axs[0].set_ylim(-40, 60)
fig.suptitle('recipes/ as templates: every measured resonance set as poles only (thin = each pole, black = cascade), 44.1 kHz, level removed at 20 Hz')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/recipe_templates.png', dpi=80)
