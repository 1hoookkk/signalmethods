from __future__ import annotations

import ctypes
import sys
import time
from pathlib import Path

import numpy as np
from scipy.optimize import least_squares, linear_sum_assignment

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "pyruntime"))
from arma_measure_lib import DATUM, lib, RT_DOUBLES

NUM_STAGES = 6
OPT_GRID = np.linspace(0.0, 1.0, 7)
N_FREQ = 96
VP_MAX = 9.2
VZ_MAX = 10.4
R_PENALTY = 0.9995

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int
lib.trench_stage_roots_from_words_at.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_double,
    ctypes.POINTER(ctypes.c_double)]
lib.trench_stage_roots_from_words_at.restype = ctypes.c_int

def probe(body: bytes, m: float, q: float, rate: float):
    c = (ctypes.c_double * RT_DOUBLES)()
    mr = ctypes.c_double(); un = ctypes.c_uint32(); nf = ctypes.c_uint32()
    buf = ctypes.create_string_buffer(body, 240)
    rc = lib.trench_packed_probe_at(buf, 240, m, q, rate, c, ctypes.byref(mr),
                                    ctypes.byref(un), ctypes.byref(nf))
    assert rc == 0
    return np.ctypeslib.as_array(c).reshape(6, 5).copy(), mr.value

def cascade_db(rows: np.ndarray, freqs: np.ndarray, rate: float):
    z1 = np.exp(-1j * 2.0 * np.pi * freqs / rate)
    z2 = z1 * z1
    b0 = rows[:, 0:1]; b1 = rows[:, 1:2]; b2 = rows[:, 2:3]
    a1 = rows[:, 3:4]; a2 = rows[:, 4:5]
    h = (b0 + b1 * z1[None] + b2 * z2[None]) / (1.0 + a1 * z1[None] + a2 * z2[None])
    return 20.0 * np.log10(np.maximum(np.abs(h).prod(axis=0), 1e-15))

def encode_row(roots5, rate: float):
    row = (ctypes.c_uint16 * 5)()
    if lib.trench_stage_words_from_roots_at(
            (ctypes.c_double * 5)(*roots5), rate, row) != 0:
        return None
    return list(row)

def pack(words120):
    out = ctypes.create_string_buffer(240)
    assert lib.trench_pack_body_from_corner_words(
        (ctypes.c_uint16 * 120)(*words120), 120, out) == 0
    return out.raw

def certify(body: bytes):
    p, mr = ctypes.c_int(), ctypes.c_double()
    fm, fq = ctypes.c_double(), ctypes.c_double()
    assert lib.trench_certify_body(
        ctypes.create_string_buffer(body, 240), 240, 33, 1.0,
        ctypes.byref(p), ctypes.byref(mr), ctypes.byref(fm),
        ctypes.byref(fq)) == 0
    return int(p.value), float(mr.value)

def sec_coord(r):
    if r is None or (r[1] <= 0 and r[3] <= 0):
        return None
    up = np.log2(max(r[0], 20.0)); vp = -20 * np.log10(max(1e-12, 1 - min(r[1], 1 - 1e-12)))
    uz = np.log2(max(r[2], 20.0)); vz = -20 * np.log10(max(1e-12, 1 - min(r[3], 1 - 1e-12)))
    return up, vp, uz, vz

def sec_dist(a, b):
    if a is None and b is None:
        return 0.0
    if a is None or b is None:
        return 10.0
    return (abs(a[0] - b[0]) + abs(a[1] - b[1]) / 12.0
            + 0.5 * (abs(a[2] - b[2]) + abs(a[3] - b[3]) / 12.0))

def assign(corner_roots):
    coords = [[sec_coord(r) for r in c] for c in corner_roots]
    order = [list(range(6))]
    for ci in (1, 2):
        cost = np.array([[sec_dist(coords[0][i], coords[ci][j])
                          for j in range(6)] for i in range(6)])
        _, col = linear_sum_assignment(cost)
        order.append(list(col))
    cost = np.zeros((6, 6))
    for slot in range(6):
        b = coords[1][order[1][slot]]
        c = coords[2][order[2][slot]]
        for j in range(6):
            cost[slot, j] = sec_dist(b, coords[3][j]) + sec_dist(c, coords[3][j])
    _, col = linear_sum_assignment(cost)
    order.append(list(col))
    return [[corner_roots[ci][order[ci][s]] for s in range(6)]
            for ci in range(4)]

LANE_TEMPLATES = {
    "vowel": [
        [8200.0, 0.975, 700.0, 0.94],
        [900.0, 0.980, 1130.0, 0.946],
        [1570.0, 0.978, 1980.0, 0.960],
        [2350.0, 0.995, 2960.0, 0.972],
        [4600.0, 0.948, 7750.0, 0.86],
        [210.0, 0.994, 6400.0, 0.9999],
    ],
    "phaser": [
        [250.0, 0.80, 250.0, 0.98],
        [500.0, 0.80, 500.0, 0.98],
        [1000.0, 0.80, 1000.0, 0.98],
        [2000.0, 0.80, 2000.0, 0.98],
        [4000.0, 0.80, 4000.0, 0.98],
        [8000.0, 0.80, 8000.0, 0.9995],
    ],
    "sweep": [
        [1000.0, 0.85, 8000.0, 0.90],
        [1000.0, 0.90, 8000.0, 0.90],
        [1000.0, 0.97, 8000.0, 0.90],
        [4000.0, 0.70, 12000.0, 0.90],
        [6000.0, 0.70, 14000.0, 0.90],
        [150.0, 0.85, 16000.0, 0.9995],
    ],
    "wah": [
        [150.0, 0.80, 60.0, 0.95],
        [800.0, 0.97, 300.0, 0.90],
        [1500.0, 0.90, 1000.0, 0.90],
        [3000.0, 0.85, 2200.0, 0.90],
        [5000.0, 0.80, 6000.0, 0.90],
        [250.0, 0.85, 8000.0, 0.9995],
    ],
}

_measured = (Path(__file__).resolve().parents[1] / "analysis"
             / "lane_grammar_20260804" / "lane_templates_measured.json")
if _measured.exists():
    import json
    for _g, _v in json.loads(_measured.read_text()).items():
        LANE_TEMPLATES[_g] = _v["template"]

def template_coords(name):
    return [sec_coord(list(r) + [1.0]) for r in LANE_TEMPLATES[name]]

def template_seed(corner_roots, name):
    tmpl = template_coords(name)
    out = []
    for c in corner_roots:
        coords = [sec_coord(r) for r in c]
        cost = np.array([[sec_dist(tmpl[slot], coords[j]) for j in range(6)]
                         for slot in range(6)])
        _, col = linear_sum_assignment(cost)
        out.append([c[int(j)] for j in col])
    return out

def apply_template_order(x, scales, name):
    tmpl = template_coords(name)
    lanes = []
    for s in range(6):
        cs = []
        for c in range(4):
            up, vp, uz, vz = x[4 * (c * 6 + s): 4 * (c * 6 + s) + 4]
            k = 20.0 / np.log(10.0)
            cs.append((up, vp * k, uz, vz * k))
        lanes.append(cs)
    cost = np.array([[sum(sec_dist(tmpl[slot], lc) for lc in lanes[lane])
                      for lane in range(6)] for slot in range(6)])
    _, col = linear_sum_assignment(cost)
    x2 = x.copy()
    scales2 = list(scales)
    for c in range(4):
        for slot in range(6):
            lane = int(col[slot])
            src, dst = 4 * (c * 6 + lane), 4 * (c * 6 + slot)
            x2[dst:dst + 4] = x[src:src + 4]
            scales2[c * 6 + slot] = scales[c * 6 + lane]
    return x2, scales2, list(map(int, col))

_ROM_LIB = None

def rom_stage_library(rate: float):
    global _ROM_LIB
    if _ROM_LIB is not None:
        return _ROM_LIB
    import struct
    from pathlib import Path
    rom_dir = Path(__file__).resolve().parents[1] / "evidence" / "p2k_inspect"
    stages = []
    for p in sorted(rom_dir.glob("P2k_*.body240")):
        raw = p.read_bytes()
        if len(raw) != 240:
            continue
        words = struct.unpack("<120H", raw)
        for s in range(24):
            row = (ctypes.c_uint16 * 5)(*words[s * 5:s * 5 + 5])
            out = (ctypes.c_double * 5)()
            if lib.trench_stage_roots_from_words_at(row, rate, out) == 0:
                r = list(out)
                if r[1] > 0 or r[3] > 0:
                    stages.append(r)
    _ROM_LIB = [(r, sec_coord(r)) for r in stages]
    return _ROM_LIB

def rom_snap_seed(corner_roots, rate: float):
    lib_stages = rom_stage_library(rate)
    if not lib_stages:
        return None
    snapped = []
    for c in corner_roots:
        row = []
        for r in c:
            coord = sec_coord(r)
            if coord is None:
                row.append(r)
                continue
            best = min(lib_stages, key=lambda t: sec_dist(coord, t[1]))
            row.append([best[0][0], best[0][1], best[0][2], best[0][3],
                        r[4] if r is not None else best[0][4]])
        snapped.append(row)
    return snapped

def build_x(corner_roots, rate: float):
    x, scales = [], []
    for c in range(4):
        for s in range(6):
            r = corner_roots[c][s]
            if r is None or (r[1] <= 0 and r[3] <= 0):
                partner = next((corner_roots[k][s] for k in range(4)
                                if corner_roots[k][s] is not None
                                and corner_roots[k][s][1] > 0), None)
                hz = partner[0] if partner else 1000.0
                r = [hz, 0.2, hz, 0.1, 1.0]
            up = np.log2(np.clip(r[0], 25.0, rate * 0.49))
            vp = min(-np.log(max(1e-9, 1.0 - min(r[1], 0.99999))), VP_MAX)
            uz = np.log2(np.clip(max(r[2], 25.0), 25.0, rate * 0.49))
            vz = min(-np.log(max(1e-9, 1.0 - min(r[3], 0.99999))), VZ_MAX)
            x += [up, vp, uz, vz]
            scales.append(r[4])
    x += [0.0, 0.0, 0.0, 0.0]
    return np.array(x), scales

def x_to_body(x, scales, rate: float):
    lim_lo = np.log2(rate / 2048.0)
    lim_hi = np.log2(rate * 0.49)
    words = []
    gains = x[-4:]
    k = 0
    for c in range(4):
        g = 10.0 ** (gains[c] / 20.0 / 6.0)
        for s in range(6):
            up, vp, uz, vz = x[4 * k: 4 * k + 4]
            k += 1
            rp = 1.0 - np.exp(-min(vp, VP_MAX))
            rz = 1.0 - np.exp(-min(vz, VZ_MAX))
            hz_p = float(2.0 ** np.clip(up, lim_lo, lim_hi))
            hz_z = float(2.0 ** np.clip(uz, lim_lo, lim_hi))
            base = scales[c * 6 + s]
            row = None
            for back in range(6):
                r5 = [hz_p, rp * (1.0 - 1e-4) ** back, hz_z,
                      rz * (1.0 - 1e-4) ** back, base * g]
                row = encode_row(r5, rate)
                if row is not None:
                    break
            if row is None:
                return None
            words += row
    return pack(words)

def blend_targets(corner_curves, freqs):
    t = [np.interp(np.log(freqs), np.log(g), d) for g, d in corner_curves]
    tgt = {}
    for q in OPT_GRID:
        for m in OPT_GRID:
            tgt[(float(m), float(q))] = ((1 - m) * (1 - q) * t[0]
                                         + m * (1 - q) * t[1]
                                         + (1 - m) * q * t[2]
                                         + m * q * t[3])
    return tgt

def surface_rms(body, tgt, freqs, rate: float):
    errs = []
    for (m, q), t in tgt.items():
        rows, _ = probe(body, m, q, rate)
        errs.append(cascade_db(rows, freqs, rate) - t)
    e = np.concatenate(errs)
    e = e - e.mean()
    return float(np.sqrt((e ** 2).mean()))

def joint_refine(corner_roots, corner_curves, rate: float = DATUM,
                 iters=120, probe_iters=12, seed=3,
                 targets=None, template=None, lanes=None):
    import os
    effort = max(1, int(os.environ.get("TRENCH_JOINT_EFFORT", "1")))
    n_random = 3 * effort
    iters = iters * effort
    probe_iters = probe_iters * effort
    n_freq = N_FREQ if effort < 2 else 144
    lo_f = max(g[0] for g, _ in corner_curves)
    hi_f = min(g[-1] for g, _ in corner_curves)
    freqs = np.geomspace(lo_f, hi_f, n_freq)
    if targets is not None:
        freqs, tgt = targets
    else:
        tgt = blend_targets(corner_curves, freqs)
    dense_freqs = np.geomspace(lo_f, hi_f, 1024)
    if targets is not None:
        dense_tgt = {k: np.interp(np.log(dense_freqs), np.log(freqs), v)
                     for k, v in tgt.items()}
    else:
        dense_tgt = blend_targets(corner_curves, dense_freqs)
    _CORNER_EDGES = [(0, 1), (0, 2), (1, 3), (2, 3)]
    _TRAJ_WEIGHT = float(os.environ.get("TRENCH_TRAJ_WEIGHT", "0.5"))
    _TRAJ_TERMS = len(_CORNER_EDGES) * NUM_STAGES * 4
    n_pen = len(tgt) + _TRAJ_TERMS

    def residual(x, scales):
        body = x_to_body(x, scales, rate)
        if body is None:
            return np.full(len(tgt) * len(freqs) + n_pen, 60.0)
        outs, pen = [], []
        for (m, q), t in tgt.items():
            rows, mr = probe(body, m, q, rate)
            outs.append(cascade_db(rows, freqs, rate) - t)
            pen.append(4000.0 * max(0.0, mr - R_PENALTY))
        e = np.concatenate(outs)
        e = e - e.mean()
        traj = []
        w = np.sqrt(_TRAJ_WEIGHT)
        for a, b in _CORNER_EDGES:
            for s in range(NUM_STAGES):
                ia = 4 * (a * NUM_STAGES + s)
                ib = 4 * (b * NUM_STAGES + s)
                traj.append(w * (x[ia]     - x[ib]))
                traj.append(w * (x[ia + 1] - x[ib + 1]))
                traj.append(w * (x[ia + 2] - x[ib + 2]))
                traj.append(w * (x[ia + 3] - x[ib + 3]))
        return np.concatenate([e, np.array(pen), np.array(traj)])

    if lanes is None:
        lo = np.concatenate([np.tile([np.log2(25.0), 0.05, np.log2(25.0), 0.0], 24),
                             [-24.0] * 4])
        hi = np.concatenate([np.tile([np.log2(rate * 0.49), VP_MAX,
                                      np.log2(rate * 0.49), VZ_MAX], 24),
                             [24.0] * 4])
    else:
        assert len(lanes) == NUM_STAGES, "one lane box per slot"
        lo_rows, hi_rows = [], []
        nyq = np.log2(rate * 0.49)
        for _c in range(4):
            for s_i in range(NUM_STAGES):
                p_lo, p_hi = lanes[s_i]
                u_lo, u_hi = np.log2(max(p_lo, 25.0)), min(np.log2(max(p_hi, p_lo + 1)), nyq)
                z_lo = min(u_lo + 4.0 / 12.0, nyq)
                z_hi = min(u_hi + 9.0 / 12.0, nyq)
                lo_rows.append([u_lo, 0.05, z_lo, 0.0])
                hi_rows.append([u_hi, VP_MAX, z_hi, VZ_MAX])
        lo = np.concatenate([np.array(lo_rows).ravel(), [-24.0] * 4])
        hi = np.concatenate([np.array(hi_rows).ravel(), [24.0] * 4])

    def fit(x0, scales, budget):
        return least_squares(residual, np.clip(x0, lo, hi), args=(scales,),
                             bounds=(lo, hi), method="trf", diff_step=0.004,
                             max_nfev=budget, xtol=1e-10, ftol=1e-8)

    rng = np.random.default_rng(seed)
    seeds = ([("lane-locked", corner_roots)] if lanes is not None
             else [("fitted-order", corner_roots), ("hungarian", assign(corner_roots))])
    if template is not None:
        seeds.insert(0, ("template", template_seed(corner_roots, template)))
    snapped = rom_snap_seed(corner_roots, rate)
    if snapped is not None:
        seeds.append(("rom-snap", snapped))
        seeds.append(("rom-snap-hungarian", assign(snapped)))
    for k in range(n_random):
        perm_roots = [list(corner_roots[0])]
        for c in range(1, 4):
            p = rng.permutation(6)
            perm_roots.append([corner_roots[c][int(i)] for i in p])
        seeds.append((f"random{k}", perm_roots))

    t0 = time.time()
    probes = []
    for label, roots in seeds:
        x0, scales = build_x(roots, rate)
        sol = fit(x0, scales, probe_iters * 101)
        r = float(np.sqrt((residual(sol.x, scales)[:-n_pen] ** 2).mean()))
        probes.append((r, label, sol.x, scales))
    probes.sort(key=lambda t: t[0])
    _, label, x_best, scales = probes[0]
    sol = fit(x_best, scales, iters * 101)

    def dense_rms(x):
        body = x_to_body(x, scales, rate)
        if body is None:
            return 1e9, None
        errs = {}
        for (m, q), t in dense_tgt.items():
            rows, _ = probe(body, m, q, rate)
            errs[(m, q)] = cascade_db(rows, dense_freqs, rate) - t
        e = np.concatenate(list(errs.values()))
        return float(np.sqrt(((e - e.mean()) ** 2).mean())), errs

    rounds = []
    prev, errs = dense_rms(sol.x)
    rounds.append(prev)
    for _ in range(3):
        if errs is None:
            break
        worst = np.max(np.abs(np.stack(list(errs.values()))), axis=0)
        order = np.argsort(worst)[::-1]
        newpts = []
        for i in order:
            fc = dense_freqs[i]
            cand = (np.concatenate([freqs, np.array(newpts)])
                    if newpts else freqs)
            if np.all(np.abs(np.log2(fc / cand)) > 0.01):
                newpts.append(fc)
            if len(newpts) >= 24:
                break
        if not newpts:
            break
        freqs = np.unique(np.concatenate([freqs, np.array(newpts)]))
        if targets is not None:
            tgt = {k: np.interp(np.log(freqs), np.log(dense_freqs), v)
                   for k, v in dense_tgt.items()}
        else:
            tgt = blend_targets(corner_curves, freqs)
        sol = fit(sol.x, scales, max(iters // 2, 24) * 101)
        cur, errs = dense_rms(sol.x)
        rounds.append(cur)
        if cur > prev * 0.98:
            break
        prev = cur

    loss = float(np.sqrt((residual(sol.x, scales)[:-n_pen] ** 2).mean()))
    x_final, scales_final, slot_perm = sol.x, scales, list(range(6))
    if template is not None:
        x_final, scales_final, slot_perm = apply_template_order(
            sol.x, scales, template)
    body = x_to_body(x_final, scales_final, rate)
    info = dict(winning_seed=label, slot_perm=slot_perm,
                probe_losses={l: float(r) for r, l, _, _ in probes},
                loss_after=loss, dense_rms_rounds=rounds,
                seconds=time.time() - t0,
                freqs=freqs, targets=tgt)
    return body, info
