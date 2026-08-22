#!/usr/bin/env python3
from __future__ import annotations

import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def st(a: float, b: float) -> float:
    return 12.0 * math.log2(a / b) if a > 0 and b > 0 else float("nan")

def main() -> int:
    min_gain = float(sys.argv[1]) if len(sys.argv) > 1 else -60.0
    files = sorted((ROOT / "dossiers" / "characters").glob("P2k_*.json"))
    per_fam = {}
    for f in files:
        d = json.loads(f.read_text())
        fam = d.get("x3_type", "?")
        for cn, rows in d["corners"].items():
            poles = []
            for i, r in enumerate(rows):
                p = r["pole"]["hz"]
                z = r["zero"]["hz"]
                gain = r.get("scale_db", 0.0)
                poles.append((i + 1, p, z, gain))
            for lane, own_hz, z_hz, gain in poles:
                if z_hz <= 0:
                    continue
                if gain < min_gain:
                    continue
                own = st(z_hz, own_hz) if own_hz > 0 else float("nan")
                foreign = [st(z_hz, p) for slot, p, _z, _g in poles
                           if slot != lane and p > 0]
                foreign = [x for x in foreign if x == x and abs(x) < 90.0]
                near = min(foreign) if foreign else float("nan")
                hit = per_fam.setdefault(fam, [])
                hit.append((own, near, lane, own_hz, z_hz, gain))
    def med(xs): return sorted(xs)[len(xs) // 2]

    all_own = [o for fam in per_fam.values() for o, *_ in fam
               if o == o]
    all_near = [n for fam in per_fam.values() for _o, n, *_ in fam
                if n == n]
    print(f"== zero parking across {len(files)} ROM bodies, "
          f"content row (scale >= {min_gain:.0f} dB) ==")
    print(f"\nzero-to-OWN-pole   n={len(all_own):5d}  "
          f"median {med(all_own):+5.1f} st")
    lo = [x for x in all_own if x == x and x < 3.0]
    hi = [x for x in all_own if x == x and x >= 8.0]
    print(f"  razor/small  (< +3 st): {len(lo):4d}  "
          f"({100*len(lo)/max(1,len(all_own)):.0f}%)   "
          f"parked-high (+8 st): {len(hi):4d}  "
          f"({100*len(hi)/max(1,len(all_own)):.0f}%)")
    print(f"\nzero-to-nearest-FOREIGN-pole   n={len(all_near):5d}  "
          f"median {med(all_near):+5.1f} st")
    near3 = [x for x in all_near if x < 3.0]
    neg = [x for x in all_near if x < 0.5]
    print(f"  inside the 3.0 st collision zone: {len(near3):4d}  "
          f"({100*len(near3)/max(1,len(all_near)):.0f}%)   "
          f"closer than 0.5 st: {len(neg):3d}")

    print(f"\nBY FAMILY (own-pole offset / nearest-foreign-pole / n):")
    for fam in sorted(per_fam):
        rows = per_fam[fam]
        os_ = [r[0] for r in rows if r[0] == r[0]]
        ns_ = [r[1] for r in rows if r[1] == r[1]]
        n3 = [r for r in ns_ if r < 3.0]
        nneg = [r for r in ns_ if r < 0.5]
        print(f"  {fam:5s}  n={len(rows):3d}  "
              f"own {med(os_):+5.1f} st  "
              f"foreign {med(ns_):+5.1f} st   "
              f"<3 st {len(n3):3d}   <0.5 st {len(nneg):3d}")

    wood = [r for r in all_own if r == r and r < 3.0]
    print(f"\nThe {len(wood)} razor/small-own-offset zeros are the SHARPENING "
          f"species, not carpentry. The true collision carve set (foreign "
          f"< 3 st) is small and family-determined - see the per-family block.")
    return 0
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
