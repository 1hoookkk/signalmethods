
from __future__ import annotations

import ctypes
import struct
import sys
import time
from pathlib import Path

import numpy as np
from scipy.optimize import least_squares

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
sys.path.insert(0, str(ROOT / "tools"))

from arma_measure_lib import DATUM, lib
from joint_fit import (
    NUM_STAGES, OPT_GRID, VP_MAX, VZ_MAX, R_PENALTY,
    build_x, x_to_body, blend_targets,
    cascade_db, certify as certify_bytes, probe, surface_rms,
)

_CORNER_EDGES = [(0, 1), (0, 2), (1, 3), (2, 3)]
_TRAJ_WEIGHT = 0.5

def decode_body(path: Path):
    raw = path.read_bytes()
    assert len(raw) == 240
    words = struct.unpack("<120H", raw)
    corner_roots, corner_curves = [], []
    freqs = np.geomspace(40.0, 20_000.0, 256)
    for c in range(4):
        roots_c = []
        for s in range(6):
            row = words[c * 30 + s * 5: c * 30 + s * 5 + 5]
            row_arr = (ctypes.c_uint16 * 5)(*row)
            out = (ctypes.c_double * 5)()
            assert lib.trench_stage_roots_from_words_at(row_arr, DATUM, out) == 0
            roots_c.append([float(out[i]) for i in range(5)])
        corner_roots.append(roots_c)
        rows, _ = probe(raw, float(c & 1), float(c >> 1 & 1), DATUM)
        corner_curves.append((freqs, cascade_db(rows, freqs, DATUM)))
    return corner_roots, corner_curves, freqs

def repack_one(path: Path, dry: bool = False):
    name = path.stem
    t0 = time.time()
    info = {"name": name}

    try:
        corner_roots, corner_curves, freqs = decode_body(path)
    except Exception as e:
        info["error"] = f"decode: {e}"
        return info

    body_before = path.read_bytes()
    tgt = blend_targets(corner_curves, freqs)
    rms_before = surface_rms(body_before, tgt, freqs, DATUM)

    x0, scales = build_x(corner_roots, DATUM)
    n_freq = len(freqs)
    n_surface = len(tgt) * n_freq
    n_traj = len(_CORNER_EDGES) * NUM_STAGES * 4

    lo = np.concatenate([np.tile([np.log2(25.0), 0.05, np.log2(25.0), 0.0], 24),
                         [-24.0] * 4])
    hi = np.concatenate([np.tile([np.log2(DATUM * 0.49), VP_MAX,
                                  np.log2(DATUM * 0.49), VZ_MAX], 24),
                         [24.0] * 4])

    def residual(x, scales):
        body = x_to_body(x, scales, DATUM)
        if body is None:
            return np.full(n_surface + len(tgt) + n_traj, 60.0)
        outs, pen = [], []
        for (m, q), t in tgt.items():
            rows, mr = probe(body, m, q, DATUM)
            outs.append(cascade_db(rows, freqs, DATUM) - t)
            pen.append(4000.0 * max(0.0, mr - R_PENALTY))
        e = np.concatenate(outs)
        e = e - e.mean()
        traj = []
        w = np.sqrt(_TRAJ_WEIGHT)
        for a, b in _CORNER_EDGES:
            for s in range(NUM_STAGES):
                ia = 4 * (a * NUM_STAGES + s)
                ib = 4 * (b * NUM_STAGES + s)
                for k in range(4):
                    traj.append(w * (x[ia + k] - x[ib + k]))
        return np.concatenate([e, np.array(pen), np.array(traj)])

    sol = least_squares(
        residual, np.clip(x0, lo, hi), args=(scales,),
        bounds=(lo, hi), method="trf", diff_step=0.004,
        max_nfev=80, xtol=1e-10, ftol=1e-8)

    body_after = x_to_body(sol.x, scales, DATUM)
    if body_after is None:
        info["rms_before"] = rms_before
        info["error"] = "x_to_body returned None after refinement"
        return info

    ok, max_r = certify_bytes(body_after)
    rms_after = surface_rms(body_after, tgt, freqs, DATUM)
    loss = float(np.sqrt((residual(sol.x, scales)[:n_surface] ** 2).mean()))

    info.update({
        "rms_before": rms_before, "rms_after": rms_after,
        "loss": loss, "certified": bool(ok), "max_radius": max_r,
        "seconds": time.time() - t0,
    })

    if not dry and ok:
        bak = path.with_suffix(".body240.bak")
        if not bak.exists():
            path.rename(bak)
        path.write_bytes(body_after)
        info["written"] = True
    else:
        info["written"] = False

    return info

def main():
    import argparse, os
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--pattern", type=str, default="xml_*")
    ap.add_argument("--single", type=str, default=None)
    args = ap.parse_args()

    body_dir = ROOT / "plugin" / "presets" / "bodies"
    if args.single:
        paths = [body_dir / f"{args.single}.body240"]
    else:
        paths = sorted(body_dir.glob(f"{args.pattern}.body240"))

    if not paths:
        print(f"No {args.pattern}.body240 bodies found.")
        return

    print(f"{'name':28s} {'before':>7s} {'after':>7s} {'loss':>7s} "
          f"{'cert':>5s} {'max_r':>8s} {'time':>6s} {'wrote':>6s}")
    print("-" * 95)

    for p in paths:
        info = repack_one(p, dry=args.dry_run)
        if "error" in info:
            print(f"{info['name']:28s} ERROR: {info['error']}")
        else:
            print(f"{info['name']:28s} {info.get('rms_before', 0):6.2f}dB "
                  f"{info.get('rms_after', 0):6.2f}dB "
                  f"{info.get('loss', 0):6.2f}dB "
                  f"{'PASS' if info.get('certified') else 'FAIL':>5s} "
                  f"{info.get('max_radius', 0):8.6f} "
                  f"{info.get('seconds', 0):5.0f}s "
                  f"{'YES' if info.get('written') else 'no':>6s}")

if __name__ == "__main__":
    main()
