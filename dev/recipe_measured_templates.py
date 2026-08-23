import glob, json, math, os
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 44100.0
F = 20 * (1000.0) ** (np.arange(300) / 299)
w = 2 * np.pi * F / SR
def pole_db(hz, bw):
    r = math.exp(-math.pi * bw / SR); t = 2 * math.pi * hz / SR
    return -20 * np.log10(np.abs(1 - 2 * r * math.cos(t) * np.exp(-1j * w) + r * r * np.exp(-2j * w)))

def peaks(f, db, lo=60, hi=12000, prom=6.0, maxn=6):
    f, db = np.asarray(f), np.asarray(db)
    m = (f >= lo) & (f <= hi); f, db = f[m], db[m]
    k = max(3, len(f) // 100)
    sm = np.convolve(db, np.ones(k) / k, mode='same')
    out = []
    for i in range(1, len(sm) - 1):
        if sm[i] >= sm[i-1] and sm[i] > sm[i+1]:
            l = i
            while l > 0 and sm[l] > sm[i] - prom: l -= 1
            r = i
            while r < len(sm) - 1 and sm[r] > sm[i] - prom: r += 1
            if sm[l] <= sm[i] - prom and sm[r] <= sm[i] - prom:
                l3 = i
                while l3 > 0 and sm[l3] > sm[i] - 3: l3 -= 1
                r3 = i
                while r3 < len(sm) - 1 and sm[r3] > sm[i] - 3: r3 += 1
                out.append((sm[i], f[i], max(f[r3] - f[l3], 10.0)))
    out.sort(reverse=True)
    return [(h, b) for _, h, b in out[:maxn]]

items = []
for p in sorted(glob.glob('recipes/tfs/*.tf.json')):
    d = json.load(open(p)); items.append(('tf', os.path.basename(p)[:-8], np.array(d['freqs_hz']), np.array(d['mag_db'])))
for p in sorted(glob.glob('recipes/vocal/dvtd/subject-*/*/*-vvtf-measured.txt')):
    a = np.loadtxt(p, skiprows=1)
    f, mag = a[:, 0], a[:, 1]
    m = (f > 1) & (mag > 0)
    items.append(('mouth', os.path.basename(os.path.dirname(p)), f[m], 20 * np.log10(mag[m])))

cols = 6; rows = math.ceil(len(items) / cols)
fig, axs = plt.subplots(rows, cols, figsize=(22, 2.4 * rows), sharex=True); axs = axs.ravel()
print(f"{len(items)} measured transfer functions")
for a, (src, name, f, db) in zip(axs, items):
    ref = np.interp(300, f, db)
    a.semilogx(f, db - ref, color='#999', lw=.8)
    pk = peaks(f, db)
    tot = np.zeros_like(F)
    for hz, bw in pk: d = pole_db(hz, bw); d -= d[0]; tot += d
    if pk: a.semilogx(F, tot - np.interp(300, F, tot), color='#c44' if src == 'tf' else '#4a4', lw=1.3)
    a.set_title(f"[{src}] {name[:26]}  " + " ".join(f"{h:.0f}" for h, _ in sorted(pk)), fontsize=7)
    a.set_xlim(20, 20000); a.set_ylim(-50, 40); a.grid(True, which='both', alpha=.2)
    print(f"  {src:5s} {name:28s} " + " ".join(f"{h:.0f}/{b:.0f}" for h, b in sorted(pk)))
for a in axs[len(items):]: a.axis('off')
fig.suptitle('recipes/ measured transfer functions (grey) and the skeleton read off each — peaks >= 6 dB prominence, width at -3 dB (colour = poles only)')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/recipe_measured_templates.png', dpi=80)
