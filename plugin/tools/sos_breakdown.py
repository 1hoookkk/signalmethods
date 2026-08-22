#!/usr/bin/env python3
from __future__ import annotations

import csv
import ctypes
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, RT_DOUBLES  # noqa: E402

GRID = np.geomspace(20.0, 20_000.0, 700)
CORNER_NAMES = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]

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

def stage_curves(body: bytes, m: float, q: float, rate: float):
    z1 = np.exp(-1j * 2.0 * np.pi * GRID / rate)
    z2 = z1 * z1
    c = (ctypes.c_double * RT_DOUBLES)()
    mr = ctypes.c_double(); un = ctypes.c_uint32(); nf = ctypes.c_uint32()
    buf = ctypes.create_string_buffer(body, 240)
    if lib.trench_packed_probe_at(buf, 240, m, q, rate, c, ctypes.byref(mr),
                                  ctypes.byref(un), ctypes.byref(nf)) != 0 \
       or un.value or nf.value:
        return [np.full_like(GRID, -60.0)] * 6
    cc = np.ctypeslib.as_array(c).reshape(6, 5)
    out = []
    for b0, b1, b2, a1, a2 in cc:
        h = (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)
        out.append(20.0 * np.log10(np.maximum(np.abs(h), 1e-12)))
    return out

def corner_geometry(body: bytes, corner: int, rate: float):
    words = np.frombuffer(body, dtype="<u2").reshape(4, 6, 5)
    rows = []
    for s in range(6):
        w = (ctypes.c_uint16 * 5)(*words[corner, s])
        out = (ctypes.c_double * 5)()
        if lib.trench_stage_roots_from_words_at(w, rate, out) != 0:
            rows.append(None)
        else:
            rows.append(tuple(out))
    return rows

def main():
    body_path = Path(sys.argv[1]).resolve()
    rate = float(sys.argv[2]) if len(sys.argv) > 2 else 48_000.0
    body = body_path.read_bytes()
    assert len(body) == 240, f"{body_path.name}: {len(body)} bytes, want 240"

    corners = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)]
    out = body_path.with_name(body_path.stem + "_sos_breakdown.csv")
    with out.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["corner", "section", "pole_hz", "pole_r",
                    "zero_hz", "zero_r", "cumulative_sections",
                    "cum_gain_db_100hz", "cum_gain_db_1khz", "cum_gain_db_10khz"])
        for ci, (m, q) in enumerate(corners):
            curves = stage_curves(body, m, q, rate)
            geo = corner_geometry(body, ci, rate)
            for k in range(6):
                cum = np.sum(curves[:k + 1], axis=0)
                at = {hz: float(np.interp(hz, GRID, cum))
                      for hz in (100.0, 1000.0, 10_000.0)}
                g = geo[k]
                w.writerow([
                    CORNER_NAMES[ci], k + 1,
                    f"{g[0]:.2f}" if g else "", f"{g[1]:.4f}" if g else "",
                    f"{g[2]:.2f}" if g else "", f"{g[3]:.4f}" if g else "",
                    f"S1..S{k + 1}",
                    f"{at[100.0]:.3f}", f"{at[1000.0]:.3f}", f"{at[10_000.0]:.3f}"])

    print(f"breakdown: {out}")

if __name__ == "__main__":
    main()
