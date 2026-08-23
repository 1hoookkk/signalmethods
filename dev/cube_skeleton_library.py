import json, math, collections
import numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 39062.5
F = 20 * (800.0) ** (np.arange(300) / 299)
w = 2 * np.pi * F / SR
e1, e2 = np.exp(-1j * w), np.exp(-2j * w)
def pair(hz, r): return 1 - 2 * r * math.cos(2 * np.pi * hz / SR) * e1 + r * r * e2
bw = lambda r: -math.log(r) * SR / math.pi
def level(db): return db - db[np.argmin(np.abs(F - 100))]

cubes = json.load(open('ref/morpheus/cubes_decoded.json'))['cubes']
corners = []
for c in cubes:
    for ci, corner in enumerate(c['corners']):
        poles = [(s['pole']['hz'], s['pole']['r']) for s in corner['sections']]
        aud = sorted(h for h, r in poles if r > 0.001 and h < 8000 and bw(r) < 1500)
        if len(aud) >= 2:
            corners.append({'name': c['name'], 'ci': ci, 'poles': poles, 'aud': aud})

def same(a, b):
    return len(a) == len(b) and all(abs(1200 * math.log2(x / y)) < 50 for x, y in zip(a, b))

parent = list(range(len(corners)))
def find(i):
    while parent[i] != i: parent[i] = parent[parent[i]]; i = parent[i]
    return i
by_len = collections.defaultdict(list)
for i, c in enumerate(corners): by_len[len(c['aud'])].append(i)
for ids in by_len.values():
    for a in range(len(ids)):
        for b in range(a + 1, len(ids)):
            if same(corners[ids[a]]['aud'], corners[ids[b]]['aud']): parent[find(ids[b])] = find(ids[a])
groups = collections.defaultdict(list)
for i in range(len(corners)): groups[find(i)].append(i)
post = [g for g in groups.values() if len({corners[i]['name'] for i in g}) >= 2]
post.sort(key=lambda g: (-len({corners[i]['name'] for i in g}), -len(g)))
ones = sum(1 for g in groups.values() if len(g) == 1)
print(f"{len(corners)} cube corners with >=2 audible poles; {len(post)} postures shared across >=2 cubes covering {sum(len(g) for g in post)} corners; {ones} one-offs")

lines = []
for k, g in enumerate(post):
    hz = np.exp(np.mean([np.log(corners[i]['aud']) for i in g], 0))
    names = collections.Counter(corners[i]['name'] for i in g)
    lines.append(f"posture {k+1}: {len(names)} cubes, {len(g)} corners, poles " + " ".join(f"{h:.0f}" for h in hz) + " Hz")
    for i in g: lines.append(f"   {corners[i]['name']} c{corners[i]['ci']}")
open('dev/e2e/mask_proof/cube_skeleton_library.txt', 'w').write("\n".join(lines))
print("\n".join(l for l in lines if l.startswith('posture'))[:3000])

top = post[:16]
cols = 4; rows = math.ceil(len(top) / cols)
fig, axs = plt.subplots(rows, cols, figsize=(18, 3.3 * rows)); axs = axs.ravel()
for a, g in zip(axs, top):
    names = collections.Counter(corners[i]['name'] for i in g)
    for i in g:
        h = np.ones_like(F, dtype=complex)
        for hz, r in corners[i]['poles']:
            if r > 0.001: h /= pair(hz, r)
        a.semilogx(F, level(20 * np.log10(np.abs(h))), lw=1)
    hz = np.exp(np.mean([np.log(corners[i]['aud']) for i in g], 0))
    a.set_title(f"{len(names)} cubes, {len(g)} corners: " + " ".join(f"{x:.0f}" for x in hz) + " Hz", fontsize=9)
    a.text(.02, .03, ", ".join(f"{n}({c})" for n, c in names.most_common(5)), transform=a.transAxes, fontsize=6)
    a.set_xlim(20, 16000); a.set_ylim(-80, 60); a.grid(True, which='both', alpha=.3)
for a in axs[len(top):]: a.axis('off')
fig.suptitle(f'the cube skeleton library: pole postures shared across corners of different cubes ({len(post)} total, top {len(top)}), poles only, level removed')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/cube_skeleton_library.png', dpi=100)

fig, axs = plt.subplots(len(top), 8, figsize=(22, 1.6 * len(top)), sharex=True, sharey=True)
for row, g in zip(axs, top):
    c = corners[g[0]]
    casc = np.ones_like(F, dtype=complex)
    for si, (hz, r) in enumerate(c['poles']):
        a = row[si]
        if r > 0.001:
            h = 1 / pair(hz, r); casc *= h
            a.semilogx(F, level(20 * np.log10(np.abs(h))), color='#8b0000', lw=1)
        a.grid(True, which='both', alpha=.2)
    row[7].semilogx(F, level(20 * np.log10(np.abs(casc))), color='k', lw=1)
    row[0].set_ylabel(f"{c['name'][:16]} c{c['ci']}", fontsize=7, rotation=0, ha='right', va='center')
for si in range(7): axs[0][si].set_title(f'S{si+1}', fontsize=9)
axs[0][7].set_title('poles cascade', fontsize=9)
axs[0][0].set_ylim(-60, 50); axs[0][0].set_xlim(20, 16000)
fig.suptitle('the cube skeletons, section by section: each pole pair on its own (zeros removed), and the pole cascade')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/cube_skeleton_sections.png', dpi=90)
