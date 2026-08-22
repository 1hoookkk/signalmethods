#!/usr/bin/env python3
from __future__ import annotations

import ctypes
import itertools
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "pyruntime"))
from pyruntime.packed_interp import _packed_bilinear_reference, kernel_to_biquad

lib = ctypes.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))
lib.trench_stage_roots_from_words_at.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_double,
    ctypes.POINTER(ctypes.c_double)]
lib.trench_stage_roots_from_words_at.restype = ctypes.c_int
lib.trench_stage_words_from_roots_at.argtypes = [
    ctypes.POINTER(ctypes.c_double), ctypes.c_double,
    ctypes.POINTER(ctypes.c_uint16)]
lib.trench_stage_words_from_roots_at.restype = ctypes.c_int
lib.trench_certify_body.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint, ctypes.c_double,
    ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
lib.trench_certify_body.restype = ctypes.c_int

CORNER_KEYS = ["A", "B", "C", "D"]
GRID = np.geomspace(40.0, 18_000.0, 120)

def load_corner_roots(body: bytes, rate: float):
    words = np.frombuffer(body, dtype="<u2").reshape(4, 6, 5)
    corners = []
    for ci in range(4):
        stages = []
        for si in range(6):
            row = (ctypes.c_uint16 * 5)(*[int(w) for w in words[ci][si]])
            out = (ctypes.c_double * 5)()
            rc = lib.trench_stage_roots_from_words_at(row, rate, out)
            assert rc == 0, f"corner {ci} stage {si}: undecodable row"
            stages.append(list(out))
        corners.append(stages)
    return corners

def corner_words_from_roots(corners, rate: float):
    out = {}
    for key, stages in zip(CORNER_KEYS, corners):
        rows = []
        for p in stages:
            row = (ctypes.c_uint16 * 5)()
            rc = lib.trench_stage_words_from_roots_at(
                (ctypes.c_double * 5)(*p), rate, row)
            if rc != 0:
                return None
            rows.append(tuple(row))
        out[key] = rows
    return out

def route(corners, pole_perm, zero_perm):
    routed = [ [list(p) for p in c] for c in corners ]
    for ci in (1, 3):
        src = corners[ci]
        routed[ci] = [
            [src[pole_perm[s]][0], src[pole_perm[s]][1],
             src[zero_perm[s]][2], src[zero_perm[s]][3],
             src[pole_perm[s]][4]]
            for s in range(6)]
    return routed

Z = np.exp(-1j * 2.0 * np.pi * GRID)

def field_response(words, rate, positions):
    z = np.exp(-1j * 2.0 * np.pi * GRID / rate)
    z2 = z * z
    out = []
    for m, q in positions:
        coeffs = _packed_bilinear_reference(words, m, q)
        h = np.ones_like(z)
        ok = True
        for c in coeffs:
            b0, b1, b2, a1, a2 = kernel_to_biquad(c)
            if a2 >= 1.0:
                ok = False
            h *= (b0 + b1 * z + b2 * z2) / (1.0 + a1 * z + a2 * z2)
        out.append(20.0 * np.log10(np.maximum(np.abs(h), 1e-15)))
        if not ok:
            return None
    return np.array(out)

MID_POSITIONS = [(0.25, 0.0), (0.5, 0.0), (0.75, 0.0),
                 (0.5, 0.5), (0.5, 1.0)]

def main():
    body_path = Path(sys.argv[1])
    rate = float(sys.argv[2]) if len(sys.argv) > 2 else 44_100.0
    n_candidates = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    body = body_path.read_bytes()
    assert len(body) == 240
    corners = load_corner_roots(body, rate)
    name = body_path.stem
    outdir = ROOT / "evidence" / f"route_{name}"
    outdir.mkdir(parents=True, exist_ok=True)

    perms = list(itertools.permutations(range(6)))
    ident = tuple(range(6))

    def screen(family):
        scored = []
        for perm in perms:
            routed = (route(corners, perm, ident) if family == "pole"
                      else route(corners, ident, perm))
            words = corner_words_from_roots(routed, rate)
            if words is None:
                continue
            resp = field_response(words, rate, MID_POSITIONS)
            if resp is None:
                continue
            peak = float(resp.max())
            if peak > 40.0:
                continue
            scored.append((perm, resp))
        return scored

    print(f"screening pole routes...", flush=True)
    pole_ok = screen("pole")
    print(f"  {len(pole_ok)}/720 stable")
    print(f"screening zero routes...", flush=True)
    zero_ok = screen("zero")
    print(f"  {len(zero_ok)}/720 stable")

    def top_distinct(scored, k=24):
        base = next(r for p, r in scored if p == ident)
        ranked = sorted(scored, key=lambda pr: -float(
            np.sqrt(np.mean((pr[1] - base) ** 2))))
        keep = [(ident, base)] + [pr for pr in ranked if pr[0] != ident][:k]
        return keep

    pole_top = top_distinct(pole_ok)
    zero_top = top_distinct(zero_ok)
    print(f"joint evaluation: {len(pole_top)} x {len(zero_top)} routes")

    joint = []
    for pp, _ in pole_top:
        for zp, _ in zero_top:
            routed = route(corners, pp, zp)
            words = corner_words_from_roots(routed, rate)
            if words is None:
                continue
            resp = field_response(words, rate, MID_POSITIONS)
            if resp is None or float(resp.max()) > 40.0:
                continue
            joint.append((pp, zp, resp))
    print(f"  {len(joint)} stable joint routes")

    flat = np.array([r.ravel() for _, _, r in joint])
    idx_ident = next(i for i, (pp, zp, _) in enumerate(joint)
                     if pp == ident and zp == ident)
    chosen = [idx_ident]
    while len(chosen) < n_candidates + 1 and len(chosen) < len(joint):
        dists = np.min(
            [np.sqrt(np.mean((flat - flat[c]) ** 2, axis=1)) for c in chosen],
            axis=0)
        chosen.append(int(np.argmax(dists)))

    report = [f"route search on {body_path.name} at {rate:.0f} Hz",
              f"stable joint routes: {len(joint)}", ""]
    fig, axes = plt.subplots(len(chosen), len(MID_POSITIONS),
                             figsize=(3.2 * len(MID_POSITIONS),
                                      2.4 * len(chosen)), squeeze=False)
    for row, ji in enumerate(chosen):
        pp, zp, resp = joint[ji]
        tag = "identity" if ji == idx_ident else f"cand{row}"
        routed = route(corners, pp, zp)
        words = corner_words_from_roots(routed, rate)
        wflat = [w for key in CORNER_KEYS for st in words[key] for w in st]
        packed = b"".join(int(w).to_bytes(2, "little") for w in wflat)
        buf = ctypes.create_string_buffer(packed, 240)
        p, mr = ctypes.c_int(), ctypes.c_double()
        fm, fq = ctypes.c_double(), ctypes.c_double()
        cert = (lib.trench_certify_body(buf, 240, 33, 1.0, ctypes.byref(p),
                                        ctypes.byref(mr), ctypes.byref(fm),
                                        ctypes.byref(fq)) == 0 and p.value == 1)
        out = outdir / f"{name}__{tag}.body240"
        out.write_bytes(packed)
        report.append(f"{tag}: poles {pp} zeros {zp} "
                      f"certified={'PASS' if cert else 'FAIL'} "
                      f"max_r={mr.value:.6f}")
        for col, (m, q) in enumerate(MID_POSITIONS):
            ax = axes[row][col]
            ax.semilogx(GRID, joint[idx_ident][2][col], lw=0.8, alpha=0.5,
                        color="gray")
            ax.semilogx(GRID, resp[col], lw=1.2)
            ax.set_ylim(-60, 30)
            ax.grid(True, which="both", alpha=0.25)
            if row == 0:
                ax.set_title(f"M{m*100:.0f} Q{q*100:.0f}", fontsize=9)
            if col == 0:
                ax.set_ylabel(tag, fontsize=9)
    fig.suptitle(f"{name} — journey candidates (gray = current route)")
    fig.tight_layout()
    fig.savefig(outdir / "route_candidates.png", dpi=110)

    (outdir / "report.txt").write_text("\n".join(report))
    print("\n".join(report))
    print(f"\ncandidates -> {outdir}")

if __name__ == "__main__":
    main()
