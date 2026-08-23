import math, re, sys
import numpy as np
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

SR = 44100.0
bw = lambda r: -math.log(max(r, 1e-12)) * SR / math.pi
groups = []
for line in open('dev/pca/skeleton_library.txt'):
    if re.match(r'posture (\d+):', line): groups.append([])
    m = re.match(r'\s+(\S+) c(\d)', line)
    if m: groups[-1].append((m.group(1), int(m.group(2))))

def corner_words(body, ci):
    raw = dl.parse_p2k_bytes(open(f'ref/presets/P2k_{body}.bin', 'rb').read())[ci]
    geo = [dl.geometry_from_words_at(w, SR) for w in raw]
    aud, zw = [], []
    for w, g in zip(raw, geo):
        if g.pole.kind == 'conjugate' and g.pole.hz < 8000 and bw(g.pole.r) < 1500:
            aud.append((g.pole.hz, w[2], w[3]))
        if g.zero.kind == 'conjugate': zw.append((w[0], w[1]))
    return sorted(aud), zw

fig, axs = plt.subplots(9, 3, figsize=(19, 24))
for gi, members in enumerate(groups):
    data = [corner_words(b, c) for b, c in members]
    names = [f"{b[:12]} c{c}{' Q100' if c >= 2 else ''}" for b, c in members]
    for col, (title, pick) in enumerate([('pole FREQUENCY word', 1), ('pole RADIUS word', 2), ('zero words (mag)', None)]):
        a = axs[gi][col]
        for yi, ((aud, zw), nm) in enumerate(zip(data, names)):
            xs = [w[pick] for w in aud] if pick else [z[0] for z in zw]
            a.scatter(xs, [yi] * len(xs), s=42, marker='|', linewidths=2.4,
                      color=plt.cm.tab10(yi % 10))
        if pick == 1:
            cols_ = list(zip(*[[w[1] for w in d[0]] for d in data if len(d[0]) == len(data[0][0])]))
            ident = sum(1 for c in cols_ if len(set(c)) == 1)
            a.set_title(f"posture {gi+1} — {title}: {ident}/{len(cols_)} lanes word-identical", fontsize=9, loc='left')
        else:
            a.set_title(f"posture {gi+1} — {title}", fontsize=9, loc='left')
        a.set_yticks(range(len(names))); a.set_yticklabels(names if col == 0 else [], fontsize=7)
        a.set_xlim(0, 66000); a.grid(True, axis='x', alpha=.25); a.invert_yaxis()
axs[8][0].set_xlabel('packed u16 word — identical words form perfect columns', fontsize=8)
axs[8][1].set_xlabel('radius word — moves with the corner (Q dressing)', fontsize=8)
axs[8][2].set_xlabel('zero frequency words — vocal family shares, others do not', fontsize=8)
fig.suptitle('the nine postures on the packed lattice: each row a corner, each stroke a stored word — recurrence is byte-level, not approximate')
fig.tight_layout(); fig.savefig('dev/e2e/mask_proof/posture_packed_words.png', dpi=85)
print('ok')
