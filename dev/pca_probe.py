"""Two questions about PCA on this corpus.

1. Do the RESPONSES have low-dimensional structure per family?
2. Does the section INDEX mean the same thing across bodies?  PCA over geometry
   silently assumes it does; the project's correspondence rule says never assume.
"""
import json, pathlib, collections
import numpy as np

ROOT = pathlib.Path(r"C:\Users\hooki\trench-native\ref\morpheus_decoded")
index = json.load(open(ROOT / "index.json"))

by_fam = collections.defaultdict(list)
for row in index["filters"]:
    by_fam[row["family"]].append(row)

FAMS = ["DIPTHONGS", "FLANGERS", "STANDARD", "EQUALIZATION FILTERS", "COMPLEX FILTERS"]

print("== 1. PCA over corner responses (mean-removed, per family) ==")
print(f'{"family":<24}{"corners":>8}   cumulative explained variance, PC1..PC6')
resp_cache = {}
for fam in FAMS:
    curves = []
    for row in by_fam.get(fam, []):
        doc = json.load(open(ROOT / row["path"]))
        for c in doc["corner_data"]:
            v = np.asarray(c["response_db"])
            if np.all(np.isfinite(v)):
                curves.append(v)
    if len(curves) < 8:
        continue
    X = np.asarray(curves)
    X = X - X.mean(axis=1, keepdims=True)      # remove per-curve level
    X = X - X.mean(axis=0, keepdims=True)      # remove family mean shape
    resp_cache[fam] = X
    s = np.linalg.svd(X, compute_uv=False)
    ev = np.cumsum(s**2) / np.sum(s**2)
    print(f'{fam:<24}{len(curves):>8}   ' + "  ".join(f"{100*ev[i]:5.1f}%" for i in range(6)))

print("\n== 2. does section index mean the same thing across bodies? ==")
print("   pole frequency at each section index, per family (geometric)")
for fam in FAMS:
    rows = by_fam.get(fam, [])
    if not rows:
        continue
    per_sec = collections.defaultdict(list)
    for row in rows:
        doc = json.load(open(ROOT / row["path"]))
        c0 = doc["corner_data"][0]
        for sec in c0["sections"]:
            if sec["pole"]["kind"] == "conjugate":
                per_sec[sec["section"]].append(sec["pole"]["hz"])
    print(f'\n   {fam}')
    print(f'      {"sec":>3} {"n":>4} {"geo-median Hz":>14} {"geo-sd (octaves)":>18} {"min..max":>22}')
    for s in sorted(per_sec):
        v = np.asarray([x for x in per_sec[s] if x > 1])
        if len(v) < 4:
            continue
        lg = np.log2(v)
        print(f'      {s:>3} {len(v):>4} {2**np.median(lg):>14.0f} {lg.std():>18.2f}'
              f' {v.min():>10.0f}..{v.max():<10.0f}')
