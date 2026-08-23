import csv
import math

import numpy as np
from scipy.cluster.vq import kmeans2
from scipy.spatial.distance import cdist
from scipy.stats import chi2_contingency


def load():
    rows = [r for r in csv.DictReader(open("ref/sections.tsv"), delimiter="\t")
            if r["source"] == "p2k" and r["variant"] == "0" and int(r["body"][4:7]) <= 32]
    x, slot, corner = [], [], []
    for r in rows:
        if r["pole_kind"] != "conjugate":
            continue
        if r["zero_kind"] == "conjugate":
            zo, zw = float(r["zero_offset_oct"]), float(r["zero_bw_oct"])
        elif r["zero_kind"] == "real":
            zo, zw = (-6.0 if float(r["zero_real_a"]) > 0 else 6.0), 2.0
        else:
            zo, zw = 6.0, 4.0
        x.append([math.log2(float(r["pole_hz"])), float(r["pole_bw_oct"]), zo, zw])
        slot.append(int(r["section"]))
        corner.append(int(r["corner"]))
    return np.array(x), np.array(slot), np.array(corner), len(rows)


def silhouette(z, lab):
    d = cdist(z, z)
    out = []
    for i in range(len(z)):
        own = lab[i]
        a = d[i][lab == own]
        a = a[a > 0].mean() if (lab == own).sum() > 1 else 0.0
        b = min(d[i][lab == k].mean() for k in set(lab) if k != own)
        out.append((b - a) / max(a, b))
    return float(np.mean(out))


def mutual_info(t):
    p = t / t.sum()
    px, py = p.sum(1, keepdims=True), p.sum(0, keepdims=True)
    with np.errstate(divide="ignore", invalid="ignore"):
        return float(np.nansum(p * np.log2(p / (px * py))))


def main():
    x, slot, corner, total = load()
    mu, sd = x.mean(0), x.std(0)
    z = (x - mu) / sd
    print(f"{total} sections, {len(x)} conjugate-pole")
    for k in (2, 3, 4, 5, 6, 8, 10):
        _, lab = kmeans2(z, k, minit="++", seed=1)
        print(f"k={k:2d} silhouette {silhouette(z, lab):.3f}")
    c, lab = kmeans2(z, 4, minit="++", seed=1)
    for j in range(4):
        m = c[j] * sd + mu
        print(f"type {j}: n={int((lab == j).sum()):3d} pole {2 ** m[0]:6.0f} Hz width {m[1]:.2f} "
              f"zero offset {m[2]:+.2f} zero width {m[3]:.2f}")
    t = np.zeros((6, 4), int)
    for s, l in zip(slot, lab):
        t[s, l] += 1
    for s in range(6):
        print(f"S{s + 1} " + " ".join(f"{v:3d}" for v in t[s]) + f"  dominant {t[s].max() / t[s].sum():.2f}")
    h = -sum(p * math.log2(p) for p in (t.sum(0) / t.sum()) if p > 0)
    print(f"slot->type {mutual_info(t):.3f} of {h:.3f} bits; chi2 p={chi2_contingency(t)[1]:.2e}")
    tc = np.zeros((4, 4), int)
    for cc, l in zip(corner, lab):
        tc[cc, l] += 1
    print(f"corner->type {mutual_info(tc):.3f} bits")


if __name__ == "__main__":
    main()
