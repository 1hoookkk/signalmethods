import glob, math, os, sys, collections
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 44100.0
F = 20 * (1000.0) ** (np.arange(400) / 399)
w = 2 * np.pi * F / SR
def pair(hz, r): return 1 - 2 * r * math.cos(2 * math.pi * hz / SR) * np.exp(-1j * w) + r * r * np.exp(-2j * w)
bw = lambda r: -math.log(r) * SR / math.pi
def lvl(db): return db - db[np.argmin(np.abs(F - 100))]

def corners_at(ci):
    out = []
    for p in sorted(glob.glob('ref/presets/P2k_0*.bin')):
        name = os.path.basename(p)[4:-4]
        c = dl.decode_p2k_body(open(p, 'rb').read(), SR)[ci]
        poles = [(g.pole.hz, g.pole.r) for g in c if g.pole.kind == 'conjugate']
        aud = sorted(h for h, r in poles if h < 8000 and bw(r) < 1500)
        out.append({'name': name, 'poles': poles, 'aud': aud})
    return out

def same(a, b): return len(a) == len(b) and len(a) > 0 and all(abs(1200 * math.log2(x / y)) < 50 for x, y in zip(a, b))
def library(cs):
    parent = list(range(len(cs)))
    def find(i):
        while parent[i] != i: i = parent[i]
        return i
    for i in range(len(cs)):
        for j in range(i + 1, len(cs)):
            if same(cs[i]['aud'], cs[j]['aud']): parent[find(j)] = find(i)
    g = collections.defaultdict(list)
    for i in range(len(cs)): g[find(i)].append(i)
    shared = sorted([v for v in g.values() if len(v) >= 2], key=len, reverse=True)
    return shared, sum(1 for v in g.values() if len(v) == 1)

sets = [('M0 (corner 0, Q0)', corners_at(0)), ('M100 (corner 1, Q0)', corners_at(1))]
libs = [(t, cs) + library(cs) for t, cs in sets]
ncol = max(len(l[2]) for l in libs)
fig, axs = plt.subplots(2, ncol, figsize=(4.2 * ncol, 7), sharex=True, sharey=True)
for r, (title, cs, shared, ones) in enumerate(libs):
    print(f"{title}: {len(shared)} shared postures over {sum(len(s) for s in shared)} corners, {ones} one-offs")
    for k, grp in enumerate(shared):
        a = axs[r][k]
        for i in grp:
            casc = np.ones_like(F, dtype=complex)
            for hz, rr in cs[i]['poles']: casc /= pair(hz, rr)
            a.semilogx(F, lvl(20 * np.log10(np.abs(casc))), lw=1)
        hz = np.exp(np.mean([np.log(cs[i]['aud']) for i in grp], 0))
        a.set_title(f"{title[:4]} posture {k+1}: " + " ".join(f"{x:.0f}" for x in hz) + " Hz", fontsize=8)
        a.text(.02, .03, ", ".join(cs[i]['name'] for i in grp), transform=a.transAxes, fontsize=6)
        a.grid(True, which='both', alpha=.25)
        print(f"   posture {k+1}: " + " ".join(f"{x:.0f}" for x in hz) + " Hz  <- " + ", ".join(cs[i]['name'] for i in grp))
    for a in axs[r][len(shared):]: a.axis('off')
axs[0][0].set_xlim(20, 20000); axs[0][0].set_ylim(-70, 50)
fig.suptitle('skeleton library by corner: pole postures shared across bodies, M0 corners only (top) and M100 corners only (bottom), poles only')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/skeleton_library_m0_m100.png', dpi=100)

# cross: which M0 posture goes to which M100 posture in the same body
m0, m1 = sets[0][1], sets[1][1]
moved = [abs(1200 * math.log2(b / a)) for c0, c1 in zip(m0, m1) if len(c0['aud']) == len(c1['aud']) for a, b in zip(c0['aud'], c1['aud'])]
print(f"\nbodies with same audible pole count at M0 and M100: {sum(1 for c0, c1 in zip(m0, m1) if len(c0['aud']) == len(c1['aud']))}/33; of their poles, median move M0->M100 {np.median(moved):.0f} c, within 50 c {100*np.mean(np.array(moved) < 50):.0f}%")
