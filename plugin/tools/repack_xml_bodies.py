
from __future__ import annotations

import ctypes
import struct
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
sys.path.insert(0, str(ROOT / "tools"))

from arma_measure_lib import DATUM, lib
from joint_fit import (
    cascade_db, certify as certify_bytes, joint_refine,
    probe, surface_rms, NUM_STAGES,
)

BODY_DIR = ROOT / "plugin" / "presets" / "bodies"
FREQS = np.geomspace(40.0, 20_000.0, 256)

def decode_body(path: Path) -> tuple:
    raw = path.read_bytes()
    assert len(raw) == 240, f"{path.name}: {len(raw)} bytes"
    words = struct.unpack("<120H", raw)
    corner_roots = []
    corner_curves = []
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
        corner_curves.append((FREQS, cascade_db(rows, FREQS, DATUM)))
    return corner_roots, corner_curves

def repack_one(path: Path, dry: bool = False) -> dict:
    name = path.stem
    t0 = time.time()
    info = {"name": name}

    try:
        corner_roots, corner_curves = decode_body(path)
    except Exception as e:
        info["error"] = f"decode: {e}"
        return info

    body_before = path.read_bytes()
    c0, c1, c2, c3 = [np.interp(np.log(FREQS), np.log(g), d)
                        for g, d in corner_curves]
    tgt = {}
    for m in np.linspace(0, 1, 7):
        for q in np.linspace(0, 1, 7):
            tgt[(float(m), float(q))] = ((1 - m) * (1 - q) * c0
                                          + m * (1 - q) * c1
                                          + (1 - m) * q * c2
                                          + m * q * c3)
    rms_before = surface_rms(body_before, tgt, FREQS, DATUM)

    body_after, jinfo = joint_refine(
        corner_roots, corner_curves, rate=DATUM)

    if body_after is None:
        info["rms_before"] = rms_before
        info["error"] = "joint_refine returned None"
        return info

    ok, max_r = certify_bytes(body_after)
    rms_after = surface_rms(body_after, tgt, FREQS, DATUM)

    info.update({
        "rms_before": rms_before, "rms_after": rms_after,
        "certified": bool(ok), "max_radius": max_r,
        "winning_seed": jinfo.get("winning_seed", "?"),
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
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--single", type=str, default=None)
    args = ap.parse_args()

    if args.single:
        paths = [BODY_DIR / f"{args.single}.body240"]
    else:
        paths = sorted(BODY_DIR.glob("xml_*.body240"))

    if not paths:
        print("No xml_* bodies found.")
        return

    print(f"{'name':25s} {'before':>7s} {'after':>7s} {'cert':>5s} "
          f"{'max_r':>8s} {'seed':>18s} {'time':>6s} {'wrote':>6s}")
    print("-" * 105)

    for p in paths:
        info = repack_one(p, dry=args.dry_run)
        if "error" in info:
            print(f"{info['name']:25s} ERROR: {info['error']}")
        else:
            print(f"{info['name']:25s} {info.get('rms_before', 0):6.2f}dB "
                  f"{info.get('rms_after', 0):6.2f}dB "
                  f"{'PASS' if info.get('certified') else 'FAIL':>5s} "
                  f"{info.get('max_radius', 0):8.6f} "
                  f"{str(info.get('winning_seed', '?')):>18s} "
                  f"{info.get('seconds', 0):5.0f}s "
                  f"{'YES' if info.get('written') else 'no':>6s}")

if __name__ == "__main__":
    main()
