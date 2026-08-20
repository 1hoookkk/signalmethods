import os
import json
import glob
import math
import numpy as np
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FS = 39062.5
NYQ = FS / 2.0
IDLE_RAW = (1909, 2015)

def cents(a, b):
    return 1200.0 * math.log2(max(b, 1e-9) / max(a, 1e-9))

def st_from_hz(hz):
    if hz <= 5.0:
        return -999.0
    return 12.0 * math.log2(hz / 440.0)

# Load Morpheus Decoded Cubes
with open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json"), "r", encoding="utf-8") as f:
    morpheus_json = json.load(f)

# Load P2K Architectures
p2k_files = sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json")))
p2k_json = [json.load(open(fn, "r", encoding="utf-8")) for fn in p2k_files]

print(f"Loaded {len(morpheus_json['cubes'])} Morpheus cubes, {len(p2k_json)} P2K presets.")

# Structure cubes into standardized 8-corner format (or 4-corner for P2K)
cubes = []

for c in morpheus_json["cubes"]:
    name = c["name"]
    is_4corner = name.endswith(".4") or name.endswith(" 4")
    # corners: index = T2 | (Morph << 1) | (Freq << 2)
    # i.e. bit0=T, bit1=M, bit2=Q
    # So: C[m, q, t] = C[t | (m<<1) | (q<<2)]
    corner_dict = {}
    for ci, cor in enumerate(c["corners"]):
        t = ci & 1
        m = (ci >> 1) & 1
        q = (ci >> 2) & 1
        sections = []
        for s in cor["sections"]:
            raw = tuple(s["raw"])
            is_idle = raw[:2] == IDLE_RAW
            is_sentinel = (raw[1] == 2047 and raw[3] == 2047)
            fp = s["pole"]["hz"]
            rp = s["pole"]["r"]
            fz = s["zero"]["hz"]
            rz = s["zero"]["r"]
            sections.append({
                "fp": fp, "rp": rp, "fz": fz, "rz": rz,
                "stp": st_from_hz(fp) if not is_idle and fp > 10 else None,
                "stz": st_from_hz(fz) if rz > 0.05 and fz > 10 else None,
                "idle": is_idle, "sentinel": is_sentinel,
                "raw": raw
            })
        gain = cor.get("gain", 1.0)
        gain_db = 20.0 * math.log10(max(gain, 1e-6)) if isinstance(gain, (int, float)) else 0.0
        corner_dict[(m, q, t)] = {
            "sections": sections,
            "gain_db": gain_db
        }
    cubes.append({
        "name": name,
        "corpus": "morpheus",
        "stages": 7,
        "is_4corner": is_4corner,
        "corners": corner_dict
    })

for p in p2k_json:
    name = p["name"]
    corner_dict = {}
    # P2K corners: M0_Q0, M100_Q0, M0_Q100, M100_Q100
    p2k_map = {
        (0, 0, 0): "M0_Q0",
        (1, 0, 0): "M100_Q0",
        (0, 1, 0): "M0_Q100",
        (1, 1, 0): "M100_Q100"
    }
    for (m, q, t), cname in p2k_map.items():
        sections = []
        for s in p["sections"]:
            g = s["corners"][cname]
            def parse_root(r):
                if "pair" in r:
                    a, b = r["pair"]
                    hz = 0.3 if (a + b) >= 0 else NYQ
                    rad = min(math.sqrt(abs(a * b)), 0.99999)
                    return hz, rad, False
                return r["hz"], min(r["r"], 0.99999), False
            fp, rp, _ = parse_root(g["pole"])
            fz, rz, _ = parse_root(g["zero"])
            is_sentinel = (rp < 1e-5 and rz < 1e-5)
            sections.append({
                "fp": fp, "rp": rp, "fz": fz, "rz": rz,
                "stp": st_from_hz(fp) if fp > 10 else None,
                "stz": st_from_hz(fz) if rz > 0.05 and fz > 10 else None,
                "idle": False, "sentinel": is_sentinel,
                "raw": ()
            })
        corner_dict[(m, q, t)] = {
            "sections": sections,
            "gain_db": 0.0
        }
    cubes.append({
        "name": name,
        "corpus": "p2k",
        "stages": 6,
        "is_4corner": True,
        "corners": corner_dict
    })

print(f"Constructed unified cube database: {len(cubes)} objects.")

# =========================================================================
# MINING TASK 1: RECURRENT CORNER SCAFFOLDS (Static Blocks)
# =========================================================================
print("\n" + "="*80)
print("MINING TASK 1: RECURRENT CORNER SCAFFOLDS (Fixed Stages Across Corners)")
print("="*80)

scaffold_census = Counter()
corner_fixed_counts = []

for c in cubes:
    # Check which stages stay completely static across ALL corners of the cube
    num_stages = c["stages"]
    all_corners = list(c["corners"].values())
    if len(all_corners) < 4:
        continue
    
    fixed_stages = []
    for s_idx in range(num_stages):
        base = all_corners[0]["sections"][s_idx]
        is_fixed = True
        for cor in all_corners[1:]:
            cur = cor["sections"][s_idx]
            # Compare pole and zero
            dfp = abs(cur["fp"] - base["fp"])
            drp = abs(cur["rp"] - base["rp"])
            dfz = abs(cur["fz"] - base["fz"])
            drz = abs(cur["rz"] - base["rz"])
            if dfp > 2.0 or drp > 0.005 or dfz > 2.0 or drz > 0.005:
                is_fixed = False
                break
        if is_fixed:
            fixed_stages.append(s_idx + 1)
    
    corner_fixed_counts.append(len(fixed_stages))
    if fixed_stages:
        scaffold_pattern = tuple(fixed_stages)
        scaffold_census[scaffold_pattern] += 1

print(f"Scaffold Analysis over {len(cubes)} objects:")
print(f"  Average fixed stages per cube: {np.mean(corner_fixed_counts):.2f} / 7 (or 6)")
print(f"  Cubes with >= 1 completely fixed scaffold stage: {sum(1 for x in corner_fixed_counts if x >= 1)} ({100*sum(1 for x in corner_fixed_counts if x >= 1)/len(cubes):.1f}%)")
print(f"  Cubes with >= 3 completely fixed scaffold stages: {sum(1 for x in corner_fixed_counts if x >= 3)} ({100*sum(1 for x in corner_fixed_counts if x >= 3)/len(cubes):.1f}%)")
print("\nTop 15 Most Common Fixed Stage Scaffolds (Stage positions that never move across any corner):")
for pattern, count in scaffold_census.most_common(15):
    pct = 100.0 * count / len(cubes)
    stages_str = ", ".join(f"S{s}" for s in pattern)
    print(f"  [{stages_str:25s}]: {count:3d} cubes ({pct:5.1f}%)")

# =========================================================================
# MINING TASK 2: RECURRENT ONE-BLOCK / TWO-BLOCK SUBSTITUTIONS
# =========================================================================
print("\n" + "="*80)
print("MINING TASK 2: RECURRENT CORNER-TO-CORNER SUBSTITUTIONS (Mutation Sparsity)")
print("="*80)

# For every single edge in every cube, how many stages actually move?
mutation_counts_by_axis = {"M": [], "Q": [], "T": []}
stage_mutation_freq = {"M": Counter(), "Q": Counter(), "T": Counter()}
one_block_subs = []

for c in cubes:
    num_stages = c["stages"]
    cor = c["corners"]
    
    # Morph edges: (0,q,t) -> (1,q,t)
    for q in (0, 1):
        for t in (0, 1):
            if (0, q, t) in cor and (1, q, t) in cor:
                c0 = cor[(0, q, t)]
                c1 = cor[(1, q, t)]
                moved = []
                for s_idx in range(num_stages):
                    s0 = c0["sections"][s_idx]
                    s1 = c1["sections"][s_idx]
                    dfp = abs(s1["fp"] - s0["fp"])
                    drp = abs(s1["rp"] - s0["rp"])
                    dfz = abs(s1["fz"] - s0["fz"])
                    drz = abs(s1["rz"] - s0["rz"])
                    if dfp > 5.0 or drp > 0.01 or dfz > 5.0 or drz > 0.01:
                        moved.append(s_idx + 1)
                        stage_mutation_freq["M"][s_idx + 1] += 1
                mutation_counts_by_axis["M"].append(len(moved))
                if len(moved) == 1:
                    one_block_subs.append(("M", c["name"], moved[0], c0["sections"][moved[0]-1], c1["sections"][moved[0]-1]))

    # Q edges: (m,0,t) -> (m,1,t)
    for m in (0, 1):
        for t in (0, 1):
            if (m, 0, t) in cor and (m, 1, t) in cor:
                c0 = cor[(m, 0, t)]
                c1 = cor[(m, 1, t)]
                moved = []
                for s_idx in range(num_stages):
                    s0 = c0["sections"][s_idx]
                    s1 = c1["sections"][s_idx]
                    dfp = abs(s1["fp"] - s0["fp"])
                    drp = abs(s1["rp"] - s0["rp"])
                    dfz = abs(s1["fz"] - s0["fz"])
                    drz = abs(s1["rz"] - s0["rz"])
                    if dfp > 5.0 or drp > 0.01 or dfz > 5.0 or drz > 0.01:
                        moved.append(s_idx + 1)
                        stage_mutation_freq["Q"][s_idx + 1] += 1
                mutation_counts_by_axis["Q"].append(len(moved))
                if len(moved) == 1:
                    one_block_subs.append(("Q", c["name"], moved[0], c0["sections"][moved[0]-1], c1["sections"][moved[0]-1]))

    # Transform edges: (m,q,0) -> (m,q,1)
    if not c["is_4corner"]:
        for m in (0, 1):
            for q in (0, 1):
                if (m, q, 0) in cor and (m, q, 1) in cor:
                    c0 = cor[(m, q, 0)]
                    c1 = cor[(m, q, 1)]
                    moved = []
                    for s_idx in range(num_stages):
                        s0 = c0["sections"][s_idx]
                        s1 = c1["sections"][s_idx]
                        dfp = abs(s1["fp"] - s0["fp"])
                        drp = abs(s1["rp"] - s0["rp"])
                        dfz = abs(s1["fz"] - s0["fz"])
                        drz = abs(s1["rz"] - s0["rz"])
                        if dfp > 5.0 or drp > 0.01 or dfz > 5.0 or drz > 0.01:
                            moved.append(s_idx + 1)
                            stage_mutation_freq["T"][s_idx + 1] += 1
                    mutation_counts_by_axis["T"].append(len(moved))
                    if len(moved) == 1:
                        one_block_subs.append(("T", c["name"], moved[0], c0["sections"][moved[0]-1], c1["sections"][moved[0]-1]))

for axis in ("M", "Q", "T"):
    counts = mutation_counts_by_axis[axis]
    if not counts:
        continue
    c_hist = Counter(counts)
    print(f"\nAxis {axis} (Total Edges: {len(counts)}):")
    print(f"  Average stages mutated per edge: {np.mean(counts):.2f} / 7")
    for k in range(8):
        if k in c_hist:
            pct = 100.0 * c_hist[k] / len(counts)
            print(f"    {k} stages changed: {c_hist[k]:4d} edges ({pct:5.1f}%)")
    print("  Stage mutation probability:")
    for s_idx in range(1, 8):
        m_count = stage_mutation_freq[axis][s_idx]
        print(f"    S{s_idx}: {100.0 * m_count / len(counts):5.1f}%")

print(f"\nDiscovered {len(one_block_subs)} exact 1-block substitution edges in the corpus.")

# =========================================================================
# MINING TASK 3: MULTI-LANE CORNER OPERATORS & AXIS GRAMMARS
# =========================================================================
print("\n" + "="*80)
print("MINING TASK 3: MULTI-LANE CORNER OPERATORS & AXIS GRAMMARS")
print("="*80)

# For each axis, extract the exact delta profiles across all active stages
# Profile per stage: (delta_semitones_pole, delta_rp, delta_semitones_zero, delta_rz)
axis_operators = {"M": [], "Q": [], "T": []}

for c in cubes:
    num_stages = c["stages"]
    cor = c["corners"]
    
    # Morph
    for q in (0, 1):
        for t in (0, 1):
            if (0, q, t) in cor and (1, q, t) in cor:
                c0 = cor[(0, q, t)]
                c1 = cor[(1, q, t)]
                op = []
                for s_idx in range(num_stages):
                    s0 = c0["sections"][s_idx]
                    s1 = c1["sections"][s_idx]
                    # Compute semitone shifts if live
                    dsp = 0.0
                    if s0["stp"] is not None and s1["stp"] is not None:
                        dsp = s1["stp"] - s0["stp"]
                    drp = s1["rp"] - s0["rp"]
                    dsz = 0.0
                    if s0["stz"] is not None and s1["stz"] is not None:
                        dsz = s1["stz"] - s0["stz"]
                    drz = s1["rz"] - s0["rz"]
                    op.append({
                        "stage": s_idx + 1,
                        "dsp": dsp, "drp": drp,
                        "dsz": dsz, "drz": drz,
                        "idle": s0["idle"] and s1["idle"]
                    })
                dgain = c1["gain_db"] - c0["gain_db"]
                axis_operators["M"].append({"cube": c["name"], "q": q, "t": t, "op": op, "dgain": dgain})

    # Frequency / Q
    for m in (0, 1):
        for t in (0, 1):
            if (m, 0, t) in cor and (m, 1, t) in cor:
                c0 = cor[(m, 0, t)]
                c1 = cor[(m, 1, t)]
                op = []
                for s_idx in range(num_stages):
                    s0 = c0["sections"][s_idx]
                    s1 = c1["sections"][s_idx]
                    dsp = 0.0
                    if s0["stp"] is not None and s1["stp"] is not None:
                        dsp = s1["stp"] - s0["stp"]
                    drp = s1["rp"] - s0["rp"]
                    dsz = 0.0
                    if s0["stz"] is not None and s1["stz"] is not None:
                        dsz = s1["stz"] - s0["stz"]
                    drz = s1["rz"] - s0["rz"]
                    op.append({
                        "stage": s_idx + 1,
                        "dsp": dsp, "drp": drp,
                        "dsz": dsz, "drz": drz,
                        "idle": s0["idle"] and s1["idle"]
                    })
                dgain = c1["gain_db"] - c0["gain_db"]
                axis_operators["Q"].append({"cube": c["name"], "m": m, "t": t, "op": op, "dgain": dgain})

    # Transform
    if not c["is_4corner"]:
        for m in (0, 1):
            for q in (0, 1):
                if (m, q, 0) in cor and (m, q, 1) in cor:
                    c0 = cor[(m, q, 0)]
                    c1 = cor[(m, q, 1)]
                    op = []
                    for s_idx in range(num_stages):
                        s0 = c0["sections"][s_idx]
                        s1 = c1["sections"][s_idx]
                        dsp = 0.0
                        if s0["stp"] is not None and s1["stp"] is not None:
                            dsp = s1["stp"] - s0["stp"]
                        drp = s1["rp"] - s0["rp"]
                        dsz = 0.0
                        if s0["stz"] is not None and s1["stz"] is not None:
                            dsz = s1["stz"] - s0["stz"]
                        drz = s1["rz"] - s0["rz"]
                        op.append({
                            "stage": s_idx + 1,
                            "dsp": dsp, "drp": drp,
                            "dsz": dsz, "drz": drz,
                            "idle": s0["idle"] and s1["idle"]
                        })
                    dgain = c1["gain_db"] - c0["gain_db"]
                    axis_operators["T"].append({"cube": c["name"], "m": m, "q": q, "op": op, "dgain": dgain})

# Compute Axis Behavior Characteristics
for axis, ops in axis_operators.items():
    if not ops:
        continue
    all_dsp = [s["dsp"] for entry in ops for s in entry["op"] if not s["idle"] and abs(s["dsp"]) > 0.1]
    all_drp = [s["drp"] for entry in ops for s in entry["op"] if not s["idle"] and abs(s["drp"]) > 0.001]
    all_dsz = [s["dsz"] for entry in ops for s in entry["op"] if not s["idle"] and abs(s["dsz"]) > 0.1]
    all_drz = [s["drz"] for entry in ops for s in entry["op"] if not s["idle"] and abs(s["drz"]) > 0.001]
    
    print(f"\nAxis {axis} Statistical Profile (n={len(ops)} edges):")
    print(f"  Pole Freq Delta: Median={np.median(all_dsp):+.2f} st, IQR={np.percentile(all_dsp, 75)-np.percentile(all_dsp, 25):.2f} st, Mean |dsp|={np.mean(np.abs(all_dsp)):.2f} st")
    print(f"  Pole Radius Delta: Median={np.median(all_drp):+.4f}, Mean |drp|={np.mean(np.abs(all_drp)):.4f}")
    print(f"  Zero Freq Delta: Median={np.median(all_dsz):+.2f} st, Mean |dsz|={np.mean(np.abs(all_dsz)):.2f} st")
    print(f"  Zero Radius Delta: Median={np.median(all_drz):+.4f}, Mean |drz|={np.mean(np.abs(all_drz)):.4f}")

# =========================================================================
# MINING TASK 4: COMPOSITIONALITY & LINEARITY AUDIT
# =========================================================================
print("\n" + "="*80)
print("MINING TASK 4: COMPOSITIONALITY & PARALLEL EDGE CONFORMANCE")
print("="*80)

# Check parallelogram closure on 2D planes: C11 - (C00 + Delta_M + Delta_Q)
# Check parallelepiped closure on 3D cubes: C111 - (C000 + Delta_M + Delta_Q + Delta_T)

plane_closure_errors = []
cube_closure_errors = []

for c in cubes:
    cor = c["corners"]
    # 2D plane at T=0: C000, C100, C010, C110
    if (0,0,0) in cor and (1,0,0) in cor and (0,1,0) in cor and (1,1,0) in cor:
        c00 = cor[(0,0,0)]
        c10 = cor[(1,0,0)]
        c01 = cor[(0,1,0)]
        c11 = cor[(1,1,0)]
        
        # Measure error in frequency semitones and radius
        max_st_err = 0.0
        max_r_err = 0.0
        for s in range(c["stages"]):
            p00, p10, p01, p11 = c00["sections"][s], c10["sections"][s], c01["sections"][s], c11["sections"][s]
            if p00["stp"] is not None and p10["stp"] is not None and p01["stp"] is not None and p11["stp"] is not None:
                pred_st = p00["stp"] + (p10["stp"] - p00["stp"]) + (p01["stp"] - p00["stp"])
                err_st = abs(p11["stp"] - pred_st)
                if err_st > max_st_err:
                    max_st_err = err_st
            pred_r = p00["rp"] + (p10["rp"] - p00["rp"]) + (p01["rp"] - p00["rp"])
            err_r = abs(p11["rp"] - pred_r)
            if err_r > max_r_err:
                max_r_err = err_r
        plane_closure_errors.append((c["name"], max_st_err, max_r_err))

print(f"2D Plane Compositionality Closure (C110 == C000 + O_M + O_Q):")
print(f"  Total 2D faces tested: {len(plane_closure_errors)}")
pure_linear_planes = sum(1 for _, st_err, r_err in plane_closure_errors if st_err < 0.5 and r_err < 0.01)
print(f"  Strictly Additive Planes (Max Error < 0.5 semitones & < 0.01 radius): {pure_linear_planes} / {len(plane_closure_errors)} ({100*pure_linear_planes/len(plane_closure_errors):.1f}%)")
print(f"  Median Maximum Pole Frequency Deviation: {np.median([x[1] for x in plane_closure_errors]):.2f} semitones")
print(f"  Median Maximum Pole Radius Deviation: {np.median([x[2] for x in plane_closure_errors]):.4f}")

print("\n" + "="*80)
print("MINING COMPLETE.")
print("="*80)
