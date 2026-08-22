"""SCALE-only gain clamp — bring a body's cascade crown inside E-mu's envelope.

WHY
---
The XML->body240 path applies no gain budget, so a Q100 corner inherits
whatever the raw pole radii produce. Two shipping bodies came out far outside
anything E-mu ever authored:

    Crackle    122.0 dB      Low Rider  111.4 dB
    verbatim ROM max  44.1 dB (Contrary Bandpass)
    P2K corpus max    39.0 dB (LucifersQ)

That is not a hot preset, it is a mathematical ghost. The engine's AGC
(agc.rs, E-mu's FUN_1802c04e0) indexes a 16-entry table with
`floor(gain * |sample|) & 0xF`, so its domain ends at linear 16.0 =
+24.082 dBFS. Past that the index WRAPS to 0 and the multiplier becomes
1.0001 — a slight boost where limiting should be. A 122 dB crown wraps it
several times over.

WHAT THIS DOES
--------------
Per corner: if the cascade peak exceeds `ceiling_db`, multiply every section's
SCALE word by one shared factor so the peak lands exactly on the ceiling.
Corners already inside the ceiling are untouched, byte for byte.

SCALE is pure broadband level (b0). It cannot change spectral contrast, so
the SHAPE survives exactly — the crown still towers over the passband by its
authored amount, it just no longer leaves the AGC's domain.

Per CORNER, not globally: Crackle is +122 dB at M0Q100 and -3.5 dB at M0Q0.
One global trim would bury M0Q0 at -85 dB. SCALE is corner-level voicing gain
in the ROM too (SLOT_GRAMMAR: 26/33 bodies share one SCALE across all six
slots of a corner, differing between corners), so per-corner is also the
authentic move.

The words are edited directly via the minifloat table — no roots round-trip,
so a stage that will not decode to pole/zero geometry cannot break the clamp.

Usage:
  python tools/clamp_body_gain.py --scan                    # report only
  python tools/clamp_body_gain.py --scan --ceiling 40
  python tools/clamp_body_gain.py --apply BODY [BODY ...]
  python tools/clamp_body_gain.py --apply-over 40           # every roster body over
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
sys.path.insert(0, str(ROOT))
from joint_fit import cascade_db, probe  # noqa: E402
from pyruntime.packed_interp import decode as mf_decode  # noqa: E402

DEFAULT_CEILING_DB = 40.0
PROBE_RATE = 48_000.0
CORNERS = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)]
CORNER_NAMES = ["M0Q0", "M100Q0", "M0Q100", "M100Q100"]
SCALE_WORD = 4
STAGES = 6

_TABLE = None

def _table():
    global _TABLE
    if _TABLE is None:
        _TABLE = np.array([mf_decode(w) for w in range(65536)])
    return _TABLE

def nearest_word(target: float) -> int:
    return int(np.argmin(np.abs(_table() - target)))

def corner_peak_db(body: bytes, morph: float, q: float,
                   rate: float = PROBE_RATE) -> float:
    freqs = np.geomspace(30.0, min(19_200.0, rate * 0.45), 1024)
    rows, _ = probe(body, morph, q, rate)
    return float(cascade_db(rows, freqs, rate).max())

def scan(body: bytes, ceiling_db: float = DEFAULT_CEILING_DB):
    out = []
    for (m, q), name in zip(CORNERS, CORNER_NAMES):
        pk = corner_peak_db(body, m, q)
        out.append((name, pk, max(0.0, pk - ceiling_db)))
    return out

def clamp(body: bytes, ceiling_db: float = DEFAULT_CEILING_DB):
    words = list(struct.unpack("<120H", body))
    report = []

    for ci, ((m, q), name) in enumerate(zip(CORNERS, CORNER_NAMES)):
        before = corner_peak_db(body, m, q)
        if before <= ceiling_db:
            report.append((name, before, before))
            continue

        excess = before - ceiling_db
        per_section = 10.0 ** (-excess / 20.0 / STAGES)

        for si in range(STAGES):
            idx = ci * STAGES * 5 + si * 5 + SCALE_WORD
            words[idx] = nearest_word(_table()[words[idx]] * per_section)

        body = struct.pack("<120H", *words)
        report.append((name, before, corner_peak_db(body, m, q)))

    return struct.pack("<120H", *words), report

def shape_delta_db(a: bytes, b: bytes, morph: float, q: float) -> float:
    freqs = np.geomspace(30.0, 19_200.0, 512)
    ra, _ = probe(a, morph, q, PROBE_RATE)
    rb, _ = probe(b, morph, q, PROBE_RATE)
    da = cascade_db(ra, freqs, PROBE_RATE)
    db_ = cascade_db(rb, freqs, PROBE_RATE)
    return float(np.abs((da - da.max()) - (db_ - db_.max())).max())

def roster_bodies():
    inc = ROOT / "plugin" / "presets" / "PresetRoster.inc"
    pat = re.compile(r'TRENCH_PRESET\("(.+?)",\s*"(.+?)",\s*"(.+?)"\)')
    out = []
    for line in inc.read_text().splitlines():
        m = pat.match(line)
        if m and m.group(3) == "WORKHORSE":
            p = ROOT / "plugin" / "presets" / "bodies" / f"{m.group(2)}.body240"
            if p.exists():
                out.append((m.group(1), p))
    return out

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ceiling", type=float, default=DEFAULT_CEILING_DB)
    ap.add_argument("--scan", action="store_true", help="report the roster, change nothing")
    ap.add_argument("--apply", nargs="+", metavar="BODY", help="clamp these bodies")
    ap.add_argument("--apply-over", type=float, metavar="DB",
                    help="clamp every roster body whose peak exceeds DB")
    args = ap.parse_args()

    if args.scan:
        rows = []
        for name, path in roster_bodies():
            pk = max(p for _, p, _ in scan(path.read_bytes(), args.ceiling))
            rows.append((name, path, pk))
        rows.sort(key=lambda r: -r[2])
        print(f"{'preset':16s}{'peak dB':>9s}{'over':>8s}")
        print("-" * 34)
        for name, _, pk in rows:
            over = pk - args.ceiling
            print(f"{name:16s}{pk:9.1f}{(f'+{over:.1f}' if over > 0 else ''):>8s}")
        n = sum(1 for _, _, pk in rows if pk > args.ceiling)
        print("-" * 34)
        print(f"{n}/{len(rows)} over the {args.ceiling:.0f} dB ceiling")
        return

    targets = []
    if args.apply_over is not None:
        for name, path in roster_bodies():
            pk = max(p for _, p, _ in scan(path.read_bytes(), args.ceiling))
            if pk > args.apply_over:
                targets.append((name, path))
    elif args.apply:
        targets = [(Path(p).stem, Path(p)) for p in args.apply]
    else:
        ap.error("need --scan, --apply, or --apply-over")

    if not targets:
        print("nothing over the ceiling — no bodies changed")
        return

    for name, path in targets:
        original = path.read_bytes()
        clamped, report = clamp(original, args.ceiling)
        worst_shape = max(shape_delta_db(original, clamped, m, q) for m, q in CORNERS)
        print(f"{name}")
        for corner, before, after in report:
            note = "" if before == after else f"  ->{after:7.1f} dB"
            print(f"   {corner:10s}{before:8.1f} dB{note}")
        print(f"   shape delta (level removed): {worst_shape:.4f} dB")
        path.write_bytes(clamped)
        print(f"   written {path}")

if __name__ == "__main__":
    main()
