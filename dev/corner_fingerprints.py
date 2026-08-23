import glob, os, sys, collections
sys.path.insert(0, 'dev/cell_dictionary'); import decode_lib as dl

corners = {}
for path in sorted(glob.glob('ref/presets/P2k_0*.bin')):
    name = os.path.basename(path)[4:-4]
    raw = dl.parse_p2k_bytes(open(path, 'rb').read())
    for ci in range(4):
        corners[(name, ci)] = raw[ci]

def dupes(sig):
    g = collections.defaultdict(list)
    for k, stages in corners.items(): g[sig(stages)].append(k)
    return sorted([v for v in g.values() if len(v) > 1], key=len, reverse=True)

freq  = dupes(lambda st: tuple(sorted(w[2] for w in st)))
polesk= dupes(lambda st: tuple(sorted((w[2], w[3]) for w in st)))
full  = dupes(lambda st: tuple(tuple(w) for w in st))

lab = lambda k: f"{k[0]} c{k[1]}"
for title, groups in [("LEVEL 1 - frequency skeleton (sorted pole freq words)", freq),
                      ("LEVEL 2 - pole skeleton (freq+radius words)", polesk),
                      ("LEVEL 3 - full corner (all packed words)", full)]:
    ncorners = sum(len(g) for g in groups)
    cross = [g for g in groups if len({k[0] for k in g}) > 1]
    print(f"\n{title}: {len(groups)} duplicate groups covering {ncorners}/132 corners; {len(cross)} groups span >1 body")
    for g in groups:
        tag = "CROSS-BODY" if len({k[0] for k in g}) > 1 else "same-body"
        print(f"  [{tag}] " + ", ".join(lab(k) for k in g))
