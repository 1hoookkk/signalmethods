#!/usr/bin/env python3
"""cascade_ladder.py — where does a cascade's resonance actually accumulate?

DIAGNOSIS ONLY. Nothing here modifies a body, a radius, a level, a zero or a
word. It probes two bodies through the real packed runtime path and reports.

The question it answers: our compiled bodies read ~19 dB more resonant than the
ROM references they answer to, while every individual stage sits inside the
ROM's own measured range. So the resonance is not in the stages - it is in how
they compose. This walks the cascade one stage at a time:

    S1 -> S1..S2 -> S1..S3 -> ... -> S1..S6

and reports, for both bodies at every corner, how much resonance the composite
is carrying at each rung. The first rung where the two ladders separate is the
stage that owns the gap.

Then it tests the five candidate mechanisms, each with a number:

  A  pole-zero cancellation   for each pole, what do the OTHER five stages
                              contribute at that pole's frequency? A pole the
                              cascade pays for sits in a hole the others dig.
  B  adjacent reinforcement   how close are neighbouring poles, and do their
                              skirts add at each other's peaks?
  C  stage correspondence     are the lanes holding comparable roles at all?
  D  zero placement           each zero's interval to its OWN pole, and its
                              distance to the NEAREST FOREIGN pole.
  E  ordering / interpolation the cascade is a product, so stage order cannot
                              change |H|. Proven numerically by permuting.

Metric note: peak-above-DC is used throughout rather than absolute peak,
because it is invariant to the SCALE law - it measures shape, not level. It is
also rate-invariant: the packed words decode to the same coefficients whatever
rate you read them at, so only the Hz axis moves.

Usage: python tools/cascade_ladder.py OURS.body240 REFERENCE.body240 [rate_hz]
Writes <ours>_vs_<ref>_ladder.png beside the first body.
"""
from __future__ import annotations

import ctypes
import itertools
import math
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, RT_DOUBLES  # noqa: E402

GRID = np.geomspace(20.0, 20_000.0, 2400)
CORNERS = [("M0/Q0", 0.0, 0.0), ("M100/Q0", 1.0, 0.0),
           ("M0/Q100", 0.0, 1.0), ("M100/Q100", 1.0, 1.0)]
DIVERGE_DB = 6.0

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_char_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]

def stages(body: bytes, morph: float, q: float, rate: float):
    co = (ctypes.c_double * RT_DOUBLES)()
    mr, um, nm = ctypes.c_double(), ctypes.c_uint32(), ctypes.c_uint32()
    rc = lib.trench_packed_probe_at(body, len(body), morph, q, rate, co,
                                    ctypes.byref(mr), ctypes.byref(um),
                                    ctypes.byref(nm))
    if rc != 0:
        raise SystemExit(f"probe refused (rc={rc})")
    w = 2 * np.pi * GRID / rate
    z = np.exp(-1j * w)
    out = []
    for s in range(6):
        b0, b1, b2, a1, a2 = co[s * 5:s * 5 + 5]
        H = (b0 + b1 * z + b2 * z ** 2) / (1 + a1 * z + a2 * z ** 2)
        db = 20 * np.log10(np.abs(H) + 1e-30)
        dc = 20 * np.log10(abs((b0 + b1 + b2) / (1 + a1 + a2)) + 1e-30)
        pr = math.sqrt(abs(a2))
        phz = (math.acos(max(-1.0, min(1.0, -a1 / (2 * pr)))) / (2 * math.pi)
               * rate) if pr > 1e-9 else 0.0
        if abs(b0) > 1e-12:
            zr = math.sqrt(abs(b2 / b0))
            zhz = (math.acos(max(-1.0, min(1.0, -(b1 / b0) / (2 * zr))))
                   / (2 * math.pi) * rate) if zr > 1e-9 else 0.0
        else:
            zr, zhz = 0.0, 0.0
        out.append(dict(db=db, dc=dc, phase=np.unwrap(np.angle(H)),
                        pole_hz=phz, pole_r=pr, zero_hz=zhz, zero_r=zr))
    return out

def st(a: float, b: float) -> float:
    return 12.0 * math.log2(b / a) if a > 20.0 and b > 20.0 else float("nan")

def ladder(sts):
    cum = np.zeros_like(GRID)
    cumdc = 0.0
    rungs = []
    for s in sts:
        cum = cum + s["db"]
        cumdc += s["dc"]
        rungs.append(float(cum.max() - cumdc))
    return rungs, cum, cumdc

def report_corner(name, ours, ref, out_lines):
    ro, co_, dco = ladder(ours)
    rr, cr, dcr = ladder(ref)
    out_lines.append(f"\n  {name}")
    out_lines.append("    cumulative resonance (peak above DC, dB) as the "
                     "cascade is built up")
    out_lines.append("      rung        ours      ref     gap")
    first = None
    for i in range(6):
        gap = ro[i] - rr[i]
        mark = ""
        if first is None and abs(gap) >= DIVERGE_DB:
            first = i
            mark = "   <-- ladders separate here"
        out_lines.append(f"      S1..S{i+1}   {ro[i]:7.1f}  {rr[i]:7.1f}  "
                         f"{gap:+7.1f}{mark}")
    return first, ro, rr

def mechanism_tests(name, ours, ref, out_lines):
    out_lines.append(f"\n  {name}  —  what the other five stages do at each "
                     f"pole (test A)")
    out_lines.append("      body   stage   pole Hz   isolated peak   others at "
                     "that pole   net")
    for label, sts in (("ours", ours), ("ref ", ref)):
        for i, s in enumerate(sts):
            if s["pole_hz"] < 20.0:
                continue
            k = int(np.argmin(np.abs(GRID - s["pole_hz"])))
            iso = s["db"][k] - s["dc"]
            others = sum(o["db"][k] - o["dc"] for j, o in enumerate(sts) if j != i)
            out_lines.append(f"      {label}    S{i+1}   {s['pole_hz']:8.0f}   "
                             f"{iso:+10.1f}      {others:+12.1f}   "
                             f"{iso + others:+7.1f}")

    out_lines.append(f"\n  {name}  —  zero placement (test D)")
    out_lines.append("      body   stage   own zero    to own pole   to NEAREST "
                     "foreign pole   zero r / pole r")
    for label, sts in (("ours", ours), ("ref ", ref)):
        for i, s in enumerate(sts):
            if s["zero_hz"] < 20.0 or s["pole_hz"] < 20.0:
                continue
            own = st(s["pole_hz"], s["zero_hz"])
            foreign = [abs(st(o["pole_hz"], s["zero_hz"]))
                       for j, o in enumerate(sts)
                       if j != i and o["pole_hz"] > 20.0]
            near = min(foreign) if foreign else float("nan")
            out_lines.append(f"      {label}    S{i+1}   {s['zero_hz']:8.0f}   "
                             f"{own:+9.1f} st   {near:14.1f} st        "
                             f"{s['zero_r']:.3f} / {s['pole_r']:.3f}")

    out_lines.append(f"\n  {name}  —  adjacent pole spacing (test B)")
    for label, sts in (("ours", ours), ("ref ", ref)):
        hz = sorted(s["pole_hz"] for s in sts if s["pole_hz"] > 20.0)
        gaps = [f"{st(hz[i], hz[i+1]):.1f}" for i in range(len(hz) - 1)]
        out_lines.append(f"      {label}  poles {', '.join(f'{h:.0f}' for h in hz)}"
                         f"  |  spacing st: {', '.join(gaps)}")

def ordering_test(ours, out_lines):
    sts = stages(ours, 0.0, 0.0, 48_000.0)
    base = sum(s["db"] for s in sts)
    worst = 0.0
    for perm in itertools.islice(itertools.permutations(range(6)), 12):
        tot = sum(sts[i]["db"] for i in perm)
        worst = max(worst, float(np.max(np.abs(tot - base))))
    out_lines.append(f"\n  TEST E — stage ordering: 12 permutations of the six "
                     f"stages change the magnitude response by at most "
                     f"{worst:.2e} dB.")
    out_lines.append("      Ordering cannot cause the gap: the cascade is a "
                     "product and dB add commutatively.")

def main() -> int:
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    p_ours, p_ref = Path(sys.argv[1]), Path(sys.argv[2])
    rate = float(sys.argv[3]) if len(sys.argv) > 3 else 48_000.0
    ours_b, ref_b = p_ours.read_bytes(), p_ref.read_bytes()

    lines = [f"CASCADE LADDER — {p_ours.name}  vs  {p_ref.name}   "
             f"(probed at {rate:.0f} Hz, packed runtime path)"]
    firsts = {}
    ladders = {}
    for name, m, q in CORNERS:
        so = stages(ours_b, m, q, rate)
        sr = stages(ref_b, m, q, rate)
        first, ro, rr = report_corner(name, so, sr, lines)
        firsts[name] = first
        ladders[name] = (ro, rr)
    worst = max(CORNERS, key=lambda c: abs(ladders[c[0]][0][5] - ladders[c[0]][1][5]))
    so = stages(ours_b, worst[1], worst[2], rate)
    sr = stages(ref_b, worst[1], worst[2], rate)
    mechanism_tests(worst[0], so, sr, lines)
    ordering_test(ours_b, lines)

    lines.append("\n  FIRST DIVERGENT RUNG PER CORNER")
    for name, _m, _q in CORNERS:
        f = firsts[name]
        lines.append(f"      {name:<10s} "
                     + (f"S{f+1}" if f is not None else "never separates"))
    print("\n".join(lines))

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        return 0
    fig, ax = plt.subplots(2, 4, figsize=(19, 8))
    fig.suptitle(f"cumulative resonance ladder — {p_ours.name} (solid) vs "
                 f"{p_ref.name} (dashed)")
    for k, (name, m, q) in enumerate(CORNERS):
        ro, rr = ladders[name]
        a = ax[0][k]
        a.plot(range(1, 7), ro, "o-", color="#c96a54", label="ours")
        a.plot(range(1, 7), rr, "s--", color="#14847a", label="reference")
        a.set_title(f"{name}  resonance after S1..Sn", fontsize=10)
        a.set_xlabel("stages included")
        a.set_ylabel("peak above DC (dB)")
        a.grid(alpha=0.3)
        if k == 0:
            a.legend(fontsize=8)
        so = stages(ours_b, m, q, rate)
        sr = stages(ref_b, m, q, rate)
        b = ax[1][k]
        cum = np.zeros_like(GRID)
        for s in so:
            cum = cum + s["db"]
            b.semilogx(GRID, cum, color="#c96a54", alpha=0.45, lw=0.9)
        cum = np.zeros_like(GRID)
        for s in sr:
            cum = cum + s["db"]
            b.semilogx(GRID, cum, color="#14847a", alpha=0.45, lw=0.9, ls="--")
        b.set_ylim(-60, 30)
        b.grid(alpha=0.3)
        b.set_title(f"{name}  cumulative curves", fontsize=10)
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    out = p_ours.with_name(f"{p_ours.stem}_vs_{p_ref.stem}_ladder.png")
    fig.savefig(out, dpi=110)
    print(f"\n  plate: {out}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
