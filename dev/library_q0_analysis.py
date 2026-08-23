import glob, os, sys, collections, itertools
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl

bodies = {}
for path in sorted(glob.glob('ref/presets/P2k_0*.bin')):
    name = os.path.basename(path)[4:-4]
    raw = dl.parse_p2k_bytes(open(path, 'rb').read())
    bodies[name] = raw

IDL = [(1450, 2047), (1909, 2015)]
def is_pad(w): return (w[2], w[3]) in ((1450, 2047), (1909, 2015)) or w[3] >= 2015 and w[2] in (1450, 1909)

corners = {(n, ci): bodies[n][ci] for n in bodies for ci in (0, 1)}
lab = lambda k: f"{k[0]} c{k[1]}"

def dupe_report(title, sig, keys=None):
    g = collections.defaultdict(list)
    for k in (keys or corners): g[sig(corners[k])].append(k)
    d = sorted([v for v in g.values() if len(v) > 1], key=len, reverse=True)
    cross = [v for v in d if len({k[0] for k in v}) > 1]
    print(f"\n{title}: {len(d)} groups / {sum(len(v) for v in d)} of {len(keys or corners)} corners ({len(cross)} cross-body)")
    for v in d: print("   " + ", ".join(lab(k) for k in v))
    return d

dupe_report("A. packed pole halves (sorted mag,rsq)", lambda st: tuple(sorted((w[2], w[3]) for w in st)))
dupe_report("D. complete corners (all words)", lambda st: tuple(tuple(w) for w in st))

lanes = collections.defaultdict(list)
poleh = collections.defaultdict(list)
for k, st in corners.items():
    for si, w in enumerate(st):
        t = tuple(w)
        if (w[2], w[3]) in ((1450, 2047), (1909, 2015)): continue
        lanes[t].append((k, si))
        poleh[(w[2], w[3])].append((k, si))
full_shared = {t: v for t, v in lanes.items() if len({p[0][0] for p in v}) > 1}
pole_shared = {t: v for t, v in poleh.items() if len({p[0][0] for p in v}) > 1}
print(f"\nB. exact full lanes (all 5 words, pads excluded): {len(lanes)} distinct; {len(full_shared)} recur across bodies, covering {sum(len(v) for v in full_shared.values())} lane-slots")
for t, v in sorted(full_shared.items(), key=lambda x: -len(x[1]))[:10]:
    g = dl.geometry_from_words_at(list(t), 44100.0)
    ph = f"{g.pole.hz:.0f}Hz" if g.pole.kind == 'conjugate' else 'real'
    print(f"   pole {ph} words{t}: " + ", ".join(f"{lab(k)}S{si+1}" for k, si in v))
print(f"\nC. partial lanes - pole half only (w2,w3): {len(poleh)} distinct; {len(pole_shared)} recur across bodies covering {sum(len(v) for v in pole_shared.values())} lane-slots")
top = sorted(pole_shared.items(), key=lambda x: -len(x[1]))[:12]
for t, v in top:
    g = dl.pair_geometry_at(dl.minifloat_decode(t[0]), dl.minifloat_decode(t[1]), 44100.0)
    ph = f"{g.hz:6.0f}Hz" if g.kind == 'conjugate' else f"real {g.root_a:.2f}/{g.root_b:.2f}"
    print(f"   {ph} x{len(v):2d} in {len({p[0][0] for p in v}):2d} bodies")

pair_sig = {}
for n in bodies:
    pair_sig[n] = (tuple(sorted((w[2], w[3]) for w in bodies[n][0])), tuple(sorted((w[2], w[3]) for w in bodies[n][1])))
g = collections.defaultdict(list)
for n, s in pair_sig.items(): g[s].append(n)
d = [v for v in g.values() if len(v) > 1]
print(f"\nE. morph-pair combinations (c0 pole-half + c1 pole-half): {len(d)} shared pairs")
for v in d: print("   " + ", ".join(v))
c0sig = {n: pair_sig[n][0] for n in bodies}; c1sig = {n: pair_sig[n][1] for n in bodies}
swap = [(a, b) for a in bodies for b in bodies if a != b and c0sig[a] == c1sig[b]]
print(f"   cross-role reuse (one body's c0 pole-half = another's c1): {len(swap)}")
for a, b in swap: print(f"     {a} c0 == {b} c1")
