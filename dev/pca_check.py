"""Is the low dimensionality real, or an artefact of counting 8 correlated
corners per body?  Redo with one corner per body, and split .4 from cube."""
import json, pathlib, collections
import numpy as np

ROOT = pathlib.Path(r"C:\Users\hooki\trench-native\ref\morpheus_decoded")
index = json.load(open(ROOT / "index.json"))
by_fam = collections.defaultdict(list)
for row in index["filters"]:
    by_fam[row["family"]].append(row)


def pca(curves):
    X = np.asarray(curves)
    X = X - X.mean(axis=1, keepdims=True)
    X = X - X.mean(axis=0, keepdims=True)
    s = np.linalg.svd(X, compute_uv=False)
    return np.cumsum(s**2) / np.sum(s**2)


FAMS = ["DIPTHONGS", "FLANGERS", "STANDARD", "COMPLEX FILTERS"]
print(f'{"family":<20}{"mode":<20}{"n":>5}   PC1     PC2     PC3')
for fam in FAMS:
    rows = by_fam.get(fam, [])
    variants = {
        "all corners": lambda d: [c["response_db"] for c in d["corner_data"]],
        "corner 0 only": lambda d: [d["corner_data"][0]["response_db"]],
    }
    for label, pick in variants.items():
        curves = []
        for row in rows:
            doc = json.load(open(ROOT / row["path"]))
            for v in pick(doc):
                v = np.asarray(v)
                if np.all(np.isfinite(v)):
                    curves.append(v)
        if len(curves) < 6:
            continue
        ev = pca(curves)
        print(f'{fam:<20}{label:<20}{len(curves):>5}   ' +
              "  ".join(f"{100*ev[i]:5.1f}%" for i in range(3)))
    # split by geometry, corner 0 only
    for geo in ("square", "cube"):
        curves = []
        for row in rows:
            if row["geometry"] != geo:
                continue
            doc = json.load(open(ROOT / row["path"]))
            v = np.asarray(doc["corner_data"][0]["response_db"])
            if np.all(np.isfinite(v)):
                curves.append(v)
        if len(curves) < 6:
            continue
        ev = pca(curves)
        print(f'{fam:<20}{"  " + geo + ", corner 0":<20}{len(curves):>5}   ' +
              "  ".join(f"{100*ev[i]:5.1f}%" for i in range(3)))
    print()
