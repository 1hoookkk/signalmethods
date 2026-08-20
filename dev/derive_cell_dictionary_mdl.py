#!/usr/bin/env python3
"""
dev/derive_cell_dictionary_mdl.py
Pure Python + NumPy Vector Quantization & MDL Analysis for 2-Section Relational Cells.
Evaluates P2K (33 presets) and Morpheus (289 cubes) separately at 39,062.5 Hz datum.
"""

import sys
import os
import json
import math
import numpy as np
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
P2K_DIR = ROOT / "recipes" / "architectures"
MORPHEUS_JSON = ROOT / "ref" / "morpheus" / "cubes_decoded.json"
FS_DATUM = 39062.5

def hz_to_st(hz, ref=440.0):
    if isinstance(hz, (int, float)):
        return 12.0 * math.log2(max(hz, 1.0) / ref)
    return 12.0 * np.log2(np.maximum(hz, 1.0) / ref)

def extract_p2k_cells():
    cells = []
    for p in sorted(P2K_DIR.glob("*.json")):
        d = json.loads(p.read_text(encoding="utf-8"))
        corners = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
        for c_name in corners:
            sections = []
            for s in d.get("sections", []):
                slot = s.get("slot", 1)
                c_data = s.get("corners", {}).get(c_name, {})
                pole = c_data.get("pole") or {}
                zero = c_data.get("zero") or {}
                scale = c_data.get("scale", 1.0)
                if "hz" in pole and "hz" in zero:
                    ph, pr = pole["hz"], pole["r"]
                    zh, zr = zero["hz"], zero["r"]
                    if pr > 0.001 or zr > 0.001:
                        sections.append({
                            "slot": slot, "ph": ph, "pr": pr, "zh": zh, "zr": zr, "k": scale
                        })
            sections.sort(key=lambda s: s["ph"])
            for i in range(0, len(sections) - 1, 2):
                sa, sb = sections[i], sections[i+1]
                delta_A = hz_to_st(sa["zh"]) - hz_to_st(sa["ph"])
                delta_B = hz_to_st(sb["zh"]) - hz_to_st(sb["ph"])
                D = hz_to_st(sb["ph"]) - hz_to_st(sa["ph"])
                vec = [delta_A, delta_B, D, sa["pr"], sa["zr"], sb["pr"], sb["zr"], sa["k"], sb["k"]]
                cells.append(vec)
    return np.array(cells, dtype=np.float64)

def extract_morpheus_cells():
    cells = []
    data = json.loads(MORPHEUS_JSON.read_text(encoding="utf-8"))
    for cube in data.get("cubes", []):
        for corner in cube.get("corners", []):
            sections = []
            for s_idx, sec in enumerate(corner.get("sections", [])):
                p = sec.get("pole", {})
                z = sec.get("zero", {})
                ph, pr = p.get("hz", 0.0), p.get("r", 0.0)
                zh, zr = z.get("hz", 0.0), z.get("r", 0.0)
                if pr > 0.001 or zr > 0.001:
                    sections.append({
                        "slot": s_idx + 1, "ph": ph, "pr": pr, "zh": zh, "zr": zr
                    })
            sections.sort(key=lambda s: s["ph"])
            for i in range(0, len(sections) - 1, 2):
                sa, sb = sections[i], sections[i+1]
                delta_A = hz_to_st(sa["zh"]) - hz_to_st(sa["ph"])
                delta_B = hz_to_st(sb["zh"]) - hz_to_st(sb["ph"])
                D = hz_to_st(sb["ph"]) - hz_to_st(sa["ph"])
                vec = [delta_A, delta_B, D, sa["pr"], sa["zr"], sb["pr"], sb["zr"]]
                cells.append(vec)
    return np.array(cells, dtype=np.float64)

def run_kmeans_pure(X_train, X_test, k, n_init=5, max_iter=50):
    mu = np.mean(X_train, axis=0)
    sigma = np.std(X_train, axis=0) + 1e-6
    Z_train = (X_train - mu) / sigma
    Z_test = (X_test - mu) / sigma
    
    n_train = len(Z_train)
    best_inertia = float("inf")
    best_centroids = None
    
    for init in range(n_init):
        # Random initial centroids
        idx = np.random.choice(n_train, size=k, replace=False if n_train >= k else True)
        centroids = Z_train[idx].copy()
        
        for _ in range(max_iter):
            # Compute squared euclidean distances
            dists = np.sum((Z_train[:, None, :] - centroids[None, :, :]) ** 2, axis=2)
            labels = np.argmin(dists, axis=1)
            
            new_centroids = np.zeros_like(centroids)
            for j in range(k):
                mask = (labels == j)
                if np.any(mask):
                    new_centroids[j] = np.mean(Z_train[mask], axis=0)
                else:
                    new_centroids[j] = Z_train[np.random.randint(n_train)]
            if np.allclose(centroids, new_centroids, atol=1e-4):
                break
            centroids = new_centroids
            
        dists = np.sum((Z_train[:, None, :] - centroids[None, :, :]) ** 2, axis=2)
        inertia = np.sum(np.min(dists, axis=1))
        if inertia < best_inertia:
            best_inertia = inertia
            best_centroids = centroids
            
    # Project on test set
    test_dists = np.sum((Z_test[:, None, :] - best_centroids[None, :, :]) ** 2, axis=2)
    test_labels = np.argmin(test_dists, axis=1)
    
    centroids_phys = best_centroids * sigma + mu
    quantized_test = centroids_phys[test_labels]
    
    param_rmse = np.sqrt(np.mean((X_test - quantized_test) ** 2, axis=0))
    total_rmse = np.sqrt(np.mean((X_test - quantized_test) ** 2))
    
    return centroids_phys, test_labels, total_rmse, param_rmse

def analyze_corpus_mdl(name, cells, is_p2k=False):
    print(f"\n================================================================================")
    print(f" CORPUS CELL DICTIONARY & MDL ANALYSIS: {name.upper()}")
    print(f" Total Extracted 2-Section Cells: {len(cells):,}")
    print(f" Feature Vector: [δA, δB, D, rpA, rzA, rpB, rzB" + (", kA, kB]" if is_p2k else "]"))
    print(f"================================================================================")
    
    np.random.seed(42)
    n = len(cells)
    perm = np.random.permutation(n)
    split = n // 2
    X_train, X_test = cells[perm[:split]], cells[perm[split:]]
    
    print(f" Train Split: {len(X_train):,} cells | Held-out Test Split: {len(X_test):,} cells\n")
    
    k_list = [2, 4, 8, 16, 32, 64, 128]
    if len(X_train) >= 256:
        k_list.append(256)
        
    n_test = len(X_test)
    n_params = X_train.shape[1]
    
    print(f"{'K (Cells)':<10} | {'Param RMSE':<12} | {'δA err (st)':<12} | {'δB err (st)':<12} | {'D err (st)':<12} | {'r_p err':<10} | {'MDL Bits / Cell'}")
    print("-" * 88)
    
    results = []
    for k in k_list:
        centroids, test_labels, total_rmse, param_rmse = run_kmeans_pure(X_train, X_test, k)
        
        # Bits calculation
        bits_model = k * n_params * 16.0
        mse = np.mean((X_test - centroids[test_labels]) ** 2)
        residual_bits_per_param = max(0.5, 0.5 * math.log2(2.0 * math.pi * math.e * max(mse, 1e-8)))
        bits_data = n_test * math.log2(k) + n_test * n_params * residual_bits_per_param
        bits_per_cell = (bits_model + bits_data) / n_test
        
        d_A_err = param_rmse[0]
        d_B_err = param_rmse[1]
        D_err = param_rmse[2]
        rp_err = (param_rmse[3] + param_rmse[5]) / 2.0
        
        print(f"{k:<10} | {total_rmse:10.4f}   | {d_A_err:10.2f}st | {d_B_err:10.2f}st | {D_err:10.2f}st | {rp_err:8.4f}   | {bits_per_cell:12.1f}")
        results.append((k, total_rmse, bits_per_cell, centroids))
        
    print("-" * 88)
    best_k, best_rmse, best_bits, best_dict = min(results, key=lambda x: x[2])
    print(f"\n🏆 Optimal MDL Dictionary Size for {name}: K = {best_k} cells (Bits/Cell: {best_bits:.1f}, RMSE: {best_rmse:.4f})")
    
    print(f"\nTop 5 Empirical Cell Centers (from K={best_k} Dictionary):")
    print(f"{'Cell ID':<8} | {'δA (st)':<10} | {'δB (st)':<10} | {'D (st)':<10} | {'rpA':<8} | {'rzA':<8} | {'rpB':<8} | {'rzB':<8}")
    print("-" * 80)
    for c_i in range(min(5, best_k)):
        c = best_dict[c_i]
        print(f"Cell_{c_i:<4} | {c[0]:+8.2f}st | {c[1]:+8.2f}st | {c[2]:+8.2f}st | {c[3]:.4f}   | {c[4]:.4f}   | {c[5]:.4f}   | {c[6]:.4f}")
    print("-" * 80)

def main():
    p2k_cells = extract_p2k_cells()
    morpheus_cells = extract_morpheus_cells()
    
    analyze_corpus_mdl("P2K (33 Presets)", p2k_cells, is_p2k=True)
    analyze_corpus_mdl("Morpheus (289 Cubes)", morpheus_cells, is_p2k=False)

if __name__ == "__main__":
    main()
