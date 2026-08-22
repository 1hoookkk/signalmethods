"""Which corner-index bit is the Transform 2 axis?

A ".4" filter is square, not cube -- the manual says it has no Transform 2 axis.
If that is encoded in the data, corners differing only in the T2 bit should be
equal, or near-equal.  Test all three bits against the 153 manual-labelled
bodies and see which hypothesis separates square from cube.
"""
import json, pathlib, struct, collections
import numpy as np

SP = pathlib.Path(r"C:\WINDOWS\TEMP\claude\C--Users-hooki-trench-native\c1331e8e-ca6d-4249-8dd3-5d4540d80e20\scratchpad")
names = {int(k): v for k, v in json.load(open(SP / "up_filters.json")).items()}
bodies = pathlib.Path(r"C:\Users\hooki\trench-authoring\ref\morpheus\bodies")
by_num = {}
for p in bodies.glob("*.body"):
    try: by_num[int(p.name.split("_")[0])] = p
    except ValueError: pass

rows = []
for num, name in sorted(names.items()):
    p = by_num.get(num)
    if p is None: continue
    raw = p.read_bytes()
    if len(raw) != 560: continue
    w = np.array(struct.unpack("<280H", raw), dtype=np.int64).reshape(8, 35)
    square = name.rstrip().endswith(".4") or name.rstrip().endswith(" 4")
    # for each axis bit, mean absolute word difference between paired corners
    d = {}
    exact = {}
    for bit in (1, 2, 4):
        pairs = [(c, c ^ bit) for c in range(8) if c < (c ^ bit)]
        diffs = [np.abs(w[a] - w[b]).mean() for a, b in pairs]
        d[bit] = float(np.mean(diffs))
        exact[bit] = all(np.array_equal(w[a], w[b]) for a, b in pairs)
    rows.append((num, name, square, d, exact))

print(f"{len(rows)} bodies\n")
print("== exact corner equality across each axis bit ==")
for bit in (1, 2, 4):
    sq = sum(1 for r in rows if r[2] and r[4][bit])
    cu = sum(1 for r in rows if not r[2] and r[4][bit])
    print(f"   bit {bit}: holds on {sq}/58 square, {cu}/95 cube")

print("\n== mean |word difference| across each axis bit, by label ==")
print(f'{"":10}{"bit1 (stride1)":>18}{"bit2 (stride2)":>18}{"bit4 (stride4)":>18}')
for lab, want in (("square", True), ("cube", False)):
    sub = [r for r in rows if r[2] == want]
    line = f'{lab:<10}'
    for bit in (1, 2, 4):
        v = np.array([r[3][bit] for r in sub])
        line += f'{np.median(v):18.1f}'
    print(line)

print("\n== per-body: which axis is flattest, by label ==")
for lab, want in (("square", True), ("cube", False)):
    sub = [r for r in rows if r[2] == want]
    c = collections.Counter(min(r[3], key=r[3].get) for r in sub)
    print(f"   {lab:<8} flattest axis: {dict(sorted(c.items()))}")

print("\n== how flat is the flattest axis? (median of min mean-diff) ==")
for lab, want in (("square", True), ("cube", False)):
    sub = [r for r in rows if r[2] == want]
    v = np.array([min(r[3].values()) for r in sub])
    print(f"   {lab:<8} median {np.median(v):8.1f}   n_zero {(v==0).sum()}/{len(v)}"
          f"   p10 {np.percentile(v,10):8.1f}")
