#!/usr/bin/env python3
from __future__ import annotations

import ctypes
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, RT_DOUBLES  # noqa: E402

GRID = np.geomspace(30.0, 18_000.0, 512)
CORNERS = ["M0 Q0", "M100 Q0", "M0 Q100", "M100 Q100"]
CORNER_MQ = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)]

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

def active_stages(body: bytes) -> int:
    words = np.frombuffer(body, dtype="<u2").reshape(4, 6, 5)
    ident = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000]
    n = 0
    for s in range(6):
        if list(words[0, s]) != ident:
            n = s + 1
    return max(1, n)

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
    n_active = active_stages(body)
    name = body_path.stem

    fig, axes = plt.subplots(2, 2, figsize=(13, 9))
    cmap = plt.get_cmap("YlOrRd_r")
    colors = ["#7f1d1d", "#c96a54", "#14847a", "#2bd8c3"]

    ax = axes[0, 0]
    for s in range(n_active):
        m0 = stage_curves(body, 0.0, 0.0, rate)[s]
        m1 = stage_curves(body, 1.0, 0.0, rate)[s]
        offset = float(np.median(np.concatenate([m0, m1])))
        ax.semilogx(GRID, m0 - offset, color=colors[s % 4], lw=1.3, label=f"S{s+1} M0")
        ax.semilogx(GRID, m1 - offset, color=colors[s % 4], lw=1.0,
                    linestyle="--", alpha=0.7, label=f"S{s+1} M100")
    ax.axhline(0.0, color="0.85", lw=0.6)
    ax.set_title(f"ACTIVE STAGES ({n_active})  — solid M0, dashed M100", fontsize=10)
    ax.legend(fontsize=7, loc="best")
    ax.set_xlim(30, 18_000); ax.set_ylim(-45, 45)
    ax.grid(alpha=0.3, which="both")

    ax = axes[0, 1]
    for k in range(1, n_active + 1):
        cum0 = np.sum(stage_curves(body, 0.0, 0.0, rate)[:k], axis=0)
        cum1 = np.sum(stage_curves(body, 1.0, 0.0, rate)[:k], axis=0)
        offset = float(np.median(np.concatenate([cum0, cum1])))
        ax.semilogx(GRID, cum0 - offset, color=colors[(k - 1) % 4], lw=1.3,
                    label=f"S1..S{k} M0")
        ax.semilogx(GRID, cum1 - offset, color=colors[(k - 1) % 4], lw=1.0,
                    linestyle="--", alpha=0.7, label=f"S1..S{k} M100")
    ax.axhline(0.0, color="0.85", lw=0.6)
    ax.set_title("SIGNAL SO FAR  — S1..Sk summed, dashed = M100", fontsize=10)
    ax.legend(fontsize=7, loc="best")
    ax.set_xlim(30, 18_000); ax.set_ylim(-45, 45)
    ax.grid(alpha=0.3, which="both")

    ax = axes[1, 0]
    for (m, q), cname, col in zip(CORNER_MQ, CORNERS, colors):
        ax.semilogx(GRID, np.sum(stage_curves(body, m, q, rate)[:n_active], axis=0),
                    color=col, lw=1.3, label=cname)
    ax.legend(fontsize=7, loc="lower left")
    ax.set_title("THE FOUR CORNERS (active stages)", fontsize=10)
    ax.set_xlim(30, 18_000); ax.set_ylim(-60, 30)
    ax.grid(alpha=0.3, which="both")

    ax = axes[1, 1]
    geo = [corner_geometry(body, ci, rate) for ci in range(4)]
    for s in range(n_active):
        y = n_active - s
        for (ca, cb), alpha in (((0, 1), 0.95), ((2, 3), 0.35)):
            a, b = geo[ca][s], geo[cb][s]
            if a is None or b is None:
                continue
            fa, fb = max(a[0], 30.0), max(b[0], 30.0)
            ax.plot([fa, fb], [y + 0.16, y + 0.16], color="#7f1d1d", lw=1.2,
                    alpha=alpha, zorder=2)
            ax.plot(fa, y + 0.16, "o", color="#7f1d1d", ms=5, alpha=alpha)
            ax.plot(fb, y + 0.16, ">", color="#7f1d1d", ms=5, alpha=alpha)
            za, zb = max(a[2], 30.0), max(b[2], 30.0)
            ax.plot([za, zb], [y - 0.16, y - 0.16], color="#14847a", lw=1.2,
                    alpha=alpha, zorder=2)
            ax.plot(za, y - 0.16, "o", mfc="none", mec="#14847a", ms=5, alpha=alpha)
            ax.plot(zb, y - 0.16, ">", mfc="none", mec="#14847a", ms=5, alpha=alpha)
    ax.set_xscale("log"); ax.set_xlim(30, 18_000); ax.set_ylim(0.3, n_active + 0.7)
    ax.set_yticks([n_active - s for s in range(n_active)])
    ax.set_yticklabels([f"S{s+1}" for s in range(n_active)], fontsize=8)
    ax.grid(alpha=0.25, which="both", axis="x")
    ax.set_title("POLE/ZERO LANES (active): ● pole ○ zero, M0 → M100", fontsize=9)

    fig.suptitle(f"{name} — X3 runtime preset ({n_active} active stages, {rate:.0f} Hz)",
                 fontsize=13)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    out = body_path.with_name(body_path.stem + "_x3f.png")
    fig.savefig(out, dpi=110)
    print(f"plate: {out}")

if __name__ == "__main__":
    main()
