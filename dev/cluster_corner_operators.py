import os
import json
import glob
import math
import numpy as np
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FS = 39062.5
NYQ = FS / 2.0

# Load Morpheus Decoded Cubes
with open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json"), "r", encoding="utf-8") as f:
    morpheus_json = json.load(f)

# Load P2K Architectures
p2k_files = sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json")))
p2k_json = [json.load(open(fn, "r", encoding="utf-8")) for fn in p2k_files]

def st_from_hz(hz):
    if hz <= 5.0:
        return -999.0
    return 12.0 * math.log2(hz / 440.0)

# Build edge list
edges = {"M": [], "Q": [], "T": []}

def extract_cube_edges(c, num_stages, cor, is_4corner):
    # M edges
    for q in (0, 1):
        for t in (0, 1):
            if (0, q, t) in cor and (1, q, t) in cor:
                c0, c1 = cor[(0, q, t)], cor[(1, q, t)]
                ds_p, dr_p, ds_z, dr_z = [], [], [], []
                for s in range(num_stages):
                    s0, s1 = c0[s], c1[s]
                    dsp = (s1["stp"] - s0["stp"]) if (s0["stp"] and s1["stp"]) else 0.0
                    drp = s1["rp"] - s0["rp"]
                    dsz = (s1["stz"] - s0["stz"]) if (s0["stz"] and s1["stz"]) else 0.0
                    drz = s1["rz"] - s0["rz"]
                    ds_p.append(dsp)
                    dr_p.append(drp)
                    ds_z.append(dsz)
                    dr_z.append(drz)
                edges["M"].append({
                    "name": c["name"], "q": q, "t": t,
                    "dsp": ds_p, "drp": dr_p, "dsz": ds_z, "drz": dr_z,
                    "vec": ds_p[:6] + [dr * 20.0 for dr in dr_p[:6]]
                })
    # Q edges
    for m in (0, 1):
        for t in (0, 1):
            if (m, 0, t) in cor and (m, 1, t) in cor:
                c0, c1 = cor[(m, 0, t)], cor[(m, 1, t)]
                ds_p, dr_p, ds_z, dr_z = [], [], [], []
                for s in range(num_stages):
                    s0, s1 = c0[s], c1[s]
                    dsp = (s1["stp"] - s0["stp"]) if (s0["stp"] and s1["stp"]) else 0.0
                    drp = s1["rp"] - s0["rp"]
                    dsz = (s1["stz"] - s0["stz"]) if (s0["stz"] and s1["stz"]) else 0.0
                    drz = s1["rz"] - s0["rz"]
                    ds_p.append(dsp)
                    dr_p.append(drp)
                    ds_z.append(dsz)
                    dr_z.append(drz)
                edges["Q"].append({
                    "name": c["name"], "m": m, "t": t,
                    "dsp": ds_p, "drp": dr_p, "dsz": ds_z, "drz": dr_z,
                    "vec": ds_p[:6] + [dr * 20.0 for dr in dr_p[:6]]
                })
    # T edges
    if not is_4corner:
        for m in (0, 1):
            for q in (0, 1):
                if (m, q, 0) in cor and (m, q, 1) in cor:
                    c0, c1 = cor[(m, q, 0)], cor[(m, q, 1)]
                    ds_p, dr_p, ds_z, dr_z = [], [], [], []
                    for s in range(num_stages):
                        s0, s1 = c0[s], c1[s]
                        dsp = (s1["stp"] - s0["stp"]) if (s0["stp"] and s1["stp"]) else 0.0
                        drp = s1["rp"] - s0["rp"]
                        dsz = (s1["stz"] - s0["stz"]) if (s0["stz"] and s1["stz"]) else 0.0
                        drz = s1["rz"] - s0["rz"]
                        ds_p.append(dsp)
                        dr_p.append(drp)
                        ds_z.append(dsz)
                        dr_z.append(drz)
                    edges["T"].append({
                        "name": c["name"], "m": m, "q": q,
                        "dsp": ds_p, "drp": dr_p, "dsz": ds_z, "drz": dr_z,
                        "vec": ds_p[:6] + [dr * 20.0 for dr in dr_p[:6]]
                    })

for c in morpheus_json["cubes"]:
    cor = {}
    for ci, cr in enumerate(c["corners"]):
        t, m, q = ci & 1, (ci >> 1) & 1, (ci >> 2) & 1
        secs = []
        for s in cr["sections"]:
            fp, rp, fz, rz = s["pole"]["hz"], s["pole"]["r"], s["zero"]["hz"], s["zero"]["r"]
            secs.append({
                "fp": fp, "rp": rp, "fz": fz, "rz": rz,
                "stp": st_from_hz(fp) if fp > 10 else None,
                "stz": st_from_hz(fz) if rz > 0.05 and fz > 10 else None
            })
        cor[(m, q, t)] = secs
    is_4corner = c["name"].endswith(".4") or c["name"].endswith(" 4")
    extract_cube_edges(c, 7, cor, is_4corner)

for p in p2k_json:
    cor = {}
    p2k_map = {(0,0,0): "M0_Q0", (1,0,0): "M100_Q0", (0,1,0): "M0_Q100", (1,1,0): "M100_Q100"}
    for (m, q, t), cname in p2k_map.items():
        secs = []
        for s in p["sections"]:
            g = s["corners"][cname]
            def pr(r):
                if "pair" in r:
                    a, b = r["pair"]
                    return (0.3 if (a+b)>=0 else NYQ, min(math.sqrt(abs(a*b)), 0.9999))
                return (r["hz"], min(r["r"], 0.9999))
            fp, rp = pr(g["pole"])
            fz, rz = pr(g["zero"])
            secs.append({
                "fp": fp, "rp": rp, "fz": fz, "rz": rz,
                "stp": st_from_hz(fp) if fp > 10 else None,
                "stz": st_from_hz(fz) if rz > 0.05 and fz > 10 else None
            })
        cor[(m, q, t)] = secs
    extract_cube_edges(p, 6, cor, True)

print("Edges gathered: M={}, Q={}, T={}".format(len(edges["M"]), len(edges["Q"]), len(edges["T"])))

def simple_kmeans(X, k=5, max_iters=100, seed=42):
    rng = np.random.default_rng(seed)
    indices = rng.choice(len(X), size=k, replace=False)
    centers = X[indices].copy()
    for _ in range(max_iters):
        # Assign clusters
        dists = np.linalg.norm(X[:, None, :] - centers[None, :, :], axis=2)
        labels = np.argmin(dists, axis=1)
        # Update centers
        new_centers = np.zeros_like(centers)
        for i in range(k):
            members = X[labels == i]
            if len(members) > 0:
                new_centers[i] = members.mean(axis=0)
            else:
                new_centers[i] = X[rng.integers(0, len(X))]
        if np.allclose(centers, new_centers, atol=1e-4):
            break
        centers = new_centers
    return labels, centers

# Cluster operators on each axis
for axis in ("M", "Q", "T"):
    ax_edges = edges[axis]
    X = np.array([e["vec"] for e in ax_edges])
    
    k = 5
    labels, centers = simple_kmeans(X, k=k, seed=42)
    
    print("\n" + "="*80)
    print(f"CANONICAL CORNER OPERATORS ON AXIS: {axis} (k={k} Archetypal Gestures)")
    print("="*80)
    
    counts = Counter(labels)
    for clus_id, cnt in counts.most_common():
        pct = 100.0 * cnt / len(ax_edges)
        members = [ax_edges[i] for i in range(len(ax_edges)) if labels[i] == clus_id]
        
        # Calculate median shifts per stage
        med_dsp = [np.median([m["dsp"][s] for m in members]) for s in range(6)]
        med_drp = [np.median([m["drp"][s] for m in members]) for s in range(6)]
        med_dsz = [np.median([m["dsz"][s] for m in members]) for s in range(6)]
        med_drz = [np.median([m["drz"][s] for m in members]) for s in range(6)]
        
        sample_names = list({m["name"] for m in members})[:8]
        
        print(f"\n[GESTURE {axis}{clus_id+1:02d}] Frequency: {cnt:4d} edges ({pct:5.1f}%)")
        print(f"  Example Cubes: {', '.join(sample_names)}")
        print("  Stage Delta Profile (Median Shifts):")
        for s in range(6):
            dsp_str = f"{med_dsp[s]:+6.1f} st" if abs(med_dsp[s]) > 0.5 else "   0.0 st"
            drp_str = f"{med_drp[s]:+6.3f} r" if abs(med_drp[s]) > 0.005 else "  0.000 r"
            dsz_str = f"{med_dsz[s]:+6.1f} st" if abs(med_dsz[s]) > 0.5 else "   0.0 st"
            drz_str = f"{med_drz[s]:+6.3f} r" if abs(med_drz[s]) > 0.005 else "  0.000 r"
            print(f"    S{s+1}: Pole: {dsp_str:10s} {drp_str:10s} | Zero: {dsz_str:10s} {drz_str:10s}")
