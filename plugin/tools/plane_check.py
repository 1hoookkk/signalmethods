#!/usr/bin/env python3
"""plane_check.py — the acceptance test for the real authoring unit.

A preset is not two endpoints. It is:

    low-Morph pose + high-Morph pose
  + low-pose Q transformation + high-pose Q transformation
  + coherent interpolation between all four

So the test reads the five positions that define the plane:

    M0/Q0    M100/Q0    M0/Q100    M100/Q100    M50/Q50

and walks all four of its edges, 33 steps each, through the packed runtime at
the real rate:

    MORPH at low Q     M0/Q0    -> M100/Q0
    MORPH at high Q    M0/Q100  -> M100/Q100
    Q at low MORPH     M0/Q0    -> M0/Q100
    Q at high MORPH    M100/Q0  -> M100/Q100

EVERY EDGE CARRIES AN EXPECTATION, and it is read from the body's own recipe,
never assumed:

    ACTIVE   this edge is authored to move
    FLAT     this edge is authored NOT to move (a Q-collapsed body is a real
             authoring choice - DeepBouche's vowel is already at full tension
             at Q0, so E-MU left the Q axis alone and so do we)

A FLAT edge is allowed to be an exact duplicate. Only an ACTIVE edge that
fails to move is a failure. The expectation comes from `<body>_blueprint.json`
beside the body (its `q_corners.law`), or from --expect on the command line.

The five positions and four edges are the READABLE REPORT. They do not replace
the full-plane guarantee: `trench_certify_body` still runs the whole 33x33
packed plane, and that is what says the body is safe everywhere.

HARD FAILURES (non-zero exit), and nothing else:
  - packing or decoding fails
  - 33x33 certification fails
  - any probed point reports unstable or non-finite sections
  - the continuity rule finds a jump in the walk
  - an edge declared ACTIVE moves less than 0.5 dB RMS

Everything else - RMS movement, maximum local dB movement, largest step, total
walk, peak map, pole travel - is reported, not graded. Ears are the gate.

Plot law: fixed -60..+30 dB, dense packed evaluation, no autoranging.

Usage:
  python tools/plane_check.py BODY.body240 [rate_hz]
                              [--expect morph_lo,morph_hi,q_lo,q_hi]
  e.g. --expect ACTIVE,ACTIVE,FLAT,FLAT
Writes <body>_plane.png beside the body.
"""
from __future__ import annotations

import ctypes
import json
import math
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, RT_DOUBLES  # noqa: E402

GRID = np.geomspace(20.0, 20_000.0, 900)
STEPS = 33
MOVE_DB = 0.5
JUMP_SHARE = 0.25
JUMP_FLOOR_ST = 1.0

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_char_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_certify_body.argtypes = [
    ctypes.c_char_p, ctypes.c_size_t, ctypes.c_uint, ctypes.c_double,
    ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]

CORNERS = [("M0/Q0", 0.0, 0.0), ("M100/Q0", 1.0, 0.0),
           ("M0/Q100", 0.0, 1.0), ("M100/Q100", 1.0, 1.0),
           ("M50/Q50", 0.5, 0.5)]

EDGES = [("MORPH at low Q", (0.0, 0.0), (1.0, 0.0)),
         ("MORPH at high Q", (0.0, 1.0), (1.0, 1.0)),
         ("Q at low MORPH", (0.0, 0.0), (0.0, 1.0)),
         ("Q at high MORPH", (1.0, 0.0), (1.0, 1.0))]

FLAT_Q_LAWS = {"flat", "dead_q", "dead-q"}

class Failure(Exception):
    pass

def expectations(body_path: Path, override: str | None):
    if override:
        parts = [p.strip().upper() for p in override.split(",")]
        if len(parts) != 4 or any(p not in ("ACTIVE", "FLAT") for p in parts):
            raise SystemExit("--expect needs four values, ACTIVE or FLAT")
        return dict(zip([e[0] for e in EDGES], parts)), "--expect"

    bp = body_path.with_name(body_path.stem + "_blueprint.json")
    if bp.exists():
        d = json.loads(bp.read_text())
        law = str((d.get("q_corners") or {}).get("law", "")).split(":")[0].lower()
        q = "FLAT" if law in FLAT_Q_LAWS else "ACTIVE"
        return ({"MORPH at low Q": "ACTIVE", "MORPH at high Q": "ACTIVE",
                 "Q at low MORPH": q, "Q at high MORPH": q},
                f"{bp.name}  (q law '{law or 'unstated'}')")
    return ({e[0]: "ACTIVE" for e in EDGES},
            "no recipe beside the body - every edge assumed ACTIVE")

def probe(body: bytes, morph: float, q: float, rate: float):
    co = (ctypes.c_double * RT_DOUBLES)()
    mr, um, nm = ctypes.c_double(), ctypes.c_uint32(), ctypes.c_uint32()
    rc = lib.trench_packed_probe_at(body, len(body), morph, q, rate, co,
                                    ctypes.byref(mr), ctypes.byref(um),
                                    ctypes.byref(nm))
    if rc != 0:
        raise Failure(f"probe/decode refused at M{morph:.2f} Q{q:.2f} (rc={rc})")
    if um.value or nm.value:
        raise Failure(f"unstable/non-finite sections at M{morph:.2f} Q{q:.2f}"
                      f" (unstable mask {um.value:#08b}, "
                      f"non-finite mask {nm.value:#08b})")
    w = 2 * np.pi * GRID / rate
    z = np.exp(-1j * w)
    total = np.zeros_like(GRID)
    poles = []
    for s in range(6):
        b0, b1, b2, a1, a2 = co[s * 5:s * 5 + 5]
        total += 20 * np.log10(np.abs((b0 + b1 * z + b2 * z ** 2)
                                      / (1 + a1 * z + a2 * z ** 2)) + 1e-30)
        r = math.sqrt(abs(a2))
        hz = (math.acos(max(-1.0, min(1.0, -a1 / (2 * r)))) / (2 * math.pi)
              * rate) if r > 1e-9 else 0.0
        poles.append((hz, r))
    return total, poles

def peak_map(db: np.ndarray, n: int = 5):
    out = []
    for i in range(1, len(db) - 1):
        if db[i] >= db[i - 1] and db[i] > db[i + 1]:
            lo = min(db[max(0, i - 40):i]) if i > 0 else db[i]
            hi = min(db[i + 1:i + 41]) if i + 1 < len(db) else db[i]
            prom = db[i] - max(lo, hi)
            if prom > 3.0:
                out.append((GRID[i], db[i], prom))
    out.sort(key=lambda t: -t[2])
    return sorted(out[:n])

def walk(body: bytes, a, b, rate: float):
    curves, poles = [], []
    for i in range(STEPS):
        t = i / (STEPS - 1)
        db, p = probe(body, a[0] + (b[0] - a[0]) * t,
                      a[1] + (b[1] - a[1]) * t, rate)
        curves.append(db)
        poles.append(p)

    d = curves[-1] - curves[0]
    rms = float(np.sqrt(np.mean(d ** 2)))
    peak = float(np.max(np.abs(d)))

    walks, nets, steps_by_lane = [], [], []
    for s in range(6):
        track = [p[s][0] for p in poles]
        st = [abs(12 * math.log2(track[i + 1] / track[i]))
              for i in range(len(track) - 1)
              if track[i] > 20.0 and track[i + 1] > 20.0]
        walks.append(sum(st))
        steps_by_lane.append(max(st) if st else 0.0)
        nets.append(12 * math.log2(track[-1] / track[0])
                    if track[0] > 20.0 and track[-1] > 20.0 else 0.0)
    lead = int(np.argmax(walks))
    return dict(curves=curves, rms=rms, peak=peak, lane=lead + 1,
                travel=nets[lead], path=walks[lead],
                biggest=steps_by_lane[lead])

def main() -> int:
    args = [a for a in sys.argv[1:]]
    override = None
    if "--expect" in args:
        i = args.index("--expect")
        override = args[i + 1]
        del args[i:i + 2]
    if not args:
        raise SystemExit(__doc__)
    path = Path(args[0])
    rate = float(args[1]) if len(args) > 1 else 48_000.0
    body = path.read_bytes()
    if len(body) != 240:
        raise SystemExit(f"{path.name}: {len(body)} bytes, not 240")

    failures: list[str] = []
    expect, source = expectations(path, override)

    print(f"\n== {path.name}   fs {rate:.0f} Hz ==")
    print(f"  expectations from {source}")

    ok, maxr = ctypes.c_int(), ctypes.c_double()
    fm, fq = ctypes.c_double(), ctypes.c_double()
    rc = lib.trench_certify_body(body, 240, STEPS, 1.0, ctypes.byref(ok),
                                 ctypes.byref(maxr), ctypes.byref(fm),
                                 ctypes.byref(fq))
    if rc != 0:
        failures.append(f"certification call failed (rc={rc})")
    elif ok.value != 1:
        failures.append(f"33x33 certification FAILED at morph {fm.value:.2f} "
                        f"q {fq.value:.2f}")
    print(f"  certify {STEPS}x{STEPS} packed plane "
          f"{'PASS' if rc == 0 and ok.value == 1 else 'FAIL'}   "
          f"max pole radius {maxr.value:.6f}")

    try:
        print("\n  THE FIVE POSITIONS")
        for name, m, q in CORNERS:
            db, _ = probe(body, m, q, rate)
            feats = "  ".join(f"{hz:6.0f} Hz ({lvl:+5.1f})"
                              for hz, lvl, _ in peak_map(db))
            print(f"    {name:<10s} peak {db.max():+6.1f} dB   {feats}")

        print("\n  THE FOUR EDGES")
        results = {}
        for name, a, b in EDGES:
            w = walk(body, a, b, rate)
            results[name] = w
            exp = expect[name]
            moves = w["rms"] >= MOVE_DB
            if exp == "ACTIVE":
                verdict = "PASS  " if moves else "FAIL  "
                if not moves:
                    failures.append(f"{name}: declared ACTIVE but moves "
                                    f"{w['rms']:.2f} dB RMS (< {MOVE_DB})")
            else:
                verdict = "PASS  " if not moves else "NOTE  "
            if w["path"] > JUMP_FLOOR_ST and w["biggest"] > JUMP_SHARE * w["path"]:
                failures.append(f"{name}: jump - one step carries "
                                f"{w['biggest']:.2f} st of a {w['path']:.2f} st walk")
                verdict = "FAIL  "
            tail = ("" if exp == "ACTIVE" or not moves
                    else "   (declared FLAT but moves)")
            print(f"    {name:<16s} {exp:<6s} {verdict} "
                  f"move {w['rms']:6.2f} dB rms, {w['peak']:6.1f} dB peak   "
                  f"lane S{w['lane']} travels {w['travel']:+7.1f} st "
                  f"(walk {w['path']:5.1f} st, largest step {w['biggest']:.2f} st)"
                  + tail)

        q_edges = [expect["Q at low MORPH"], expect["Q at high MORPH"]]
        if all(e == "FLAT" for e in q_edges):
            print("\n  Q plane: FLAT BY DESIGN")
    except Failure as e:
        failures.append(str(e))
        results = {}

    if failures:
        print(f"\n  ACCEPTANCE: FAIL")
        for f in failures:
            print(f"    - {f}")
    else:
        print(f"\n  ACCEPTANCE: PASS   "
              f"{sum(1 for v in expect.values() if v == 'ACTIVE')} active edge(s) "
              f"carry travel, {sum(1 for v in expect.values() if v == 'FLAT')} "
              f"flat edge(s) hold as authored")

    if results:
        try:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
        except ImportError:
            return 1 if failures else 0

        fig, ax = plt.subplots(2, 3, figsize=(16, 8))
        fig.suptitle(f"{path.name} - the authored plane  ({rate:.0f} Hz)")
        for k, (name, m, q) in enumerate(CORNERS):
            db, _ = probe(body, m, q, rate)
            a = ax[0][k] if k < 3 else ax[1][k - 3]
            a.semilogx(GRID, db, color="#c96a54", lw=1.4)
            a.set_title(name, fontsize=10)
            a.set_ylim(-60, 30)
            a.grid(alpha=0.25)
        a = ax[1][2]
        for (name, _s, _e), col in zip(EDGES, ["#c96a54", "#14847a",
                                               "#4a7c9b", "#7b9e6b"]):
            w = results[name]
            for i, c in enumerate(w["curves"]):
                a.semilogx(GRID, c, color=col, alpha=0.15 + 0.5 * i / (STEPS - 1),
                           lw=0.7)
        a.set_title("all four edges", fontsize=10)
        a.set_ylim(-60, 30)
        a.grid(alpha=0.25)
        fig.tight_layout(rect=[0, 0, 1, 0.96])
        out = path.with_name(path.stem + "_plane.png")
        fig.savefig(out, dpi=110)
        print(f"  plate: {out}")

    return 1 if failures else 0

if __name__ == "__main__":
    raise SystemExit(main())
