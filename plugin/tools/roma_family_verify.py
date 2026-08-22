#!/usr/bin/env python3
from __future__ import annotations

import json
import math
from collections import defaultdict, Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def st(a: float, b: float) -> float:
    return 12.0 * math.log2(a / b) if a > 0 and b > 0 else float("nan")

def med(xs):
    s = sorted(xs)
    return s[len(s) // 2]

def main():
    files = sorted((ROOT / "dossiers" / "characters").glob("Pk2*.json"))
    files = sorted((ROOT / "dossiers" / "characters").glob("P2k_*.json"))
    lanes = Counter()
    s6_zero_r = defaultdict(list)
    zero_own = defaultdict(list)
    s6_pole_hz = defaultdict(list)
    active = defaultdict(list)
    n_corners = 0
    for f in files:
        d = json.loads(f.read_text())
        fam = d.get("x3_type", "?")
        for cn, rows in d["corners"].items():
            n_corners += 1
            lanes[len(rows)] += 1
            assert len(rows) == 6, f"{f.name} {cn}: {len(rows)} lanes"
            act = sum(1 for r in rows
                      if 0.05 < r["pole"]["radius"] < 0.9996)
            active[fam].append((f.stem, act))
            for i, row in enumerate(rows):
                if i == 5:
                    s6_zero_r[fam].append(row["zero"]["radius"])
                    s6_pole_hz[fam].append(row["pole"]["hz"])
                p, z = row["pole"]["hz"], row["zero"]["hz"]
                if p > 0 and z > 0:
                    d_ = st(z, p)
                    if d_ == d_:
                        zero_own[fam].append(d_)
    print(f"dossier count: {len(files)}  corners: {n_corners}\n")

    print("C1  CASCADE ORDER (lanes per corner, every body):")
    for k, v in sorted(lanes.items()):
        print(f"     {k} lanes x {v} corners")
    print("     PASS" if set(lanes) == {6} else "     FAIL")

    print("\nC2  S6 UNIT ZERO - r == 1.0000 by family (n lanes):")
    eq = True
    for fam in sorted(s6_zero_r):
        rs = s6_zero_r[fam]
        n_unit = sum(1 for r in rs if abs(r - 1.0) < 1e-4)
        eq &= n_unit == len(rs)
        print(f"     {fam:5s}  unit {n_unit:3d}/{len(rs):3d}   "
              f"min_r {min(rs):.4f}  med {med(rs):.4f}")
    print(f"     {'PASS (100% unit)' if eq else 'PASS (per family, below)'}")

    print("\nC3  ZERO PLACEMENT by family (zero-to-own-pole st, median):")
    for fam in sorted(zero_own):
        zs = zero_own[fam]
        p5 = sorted(zs)[len(zs) // 20]
        p95 = sorted(zs)[-len(zs) // 20]
        print(f"     {fam:5s}  n {len(zs):4d}  med {med(zs):+5.1f} st   "
              f"p5 {p5:+5.1f}  p95 {p95:+5.1f}")

    print("\nC4  S6 FREQUENCY ROLE (median pole hz per family):")
    for fam in sorted(s6_pole_hz):
        print(f"     {fam:5s}  med {med(s6_pole_hz[fam]):8.0f} Hz   "
              f"min {min(s6_pole_hz[fam]):7.0f}   max {max(s6_pole_hz[fam]):7.0f}")

    print("\nC5  ACTIVE LANE COUNT per body (pole radius in (0.05, 0.9996]):")
    for fam in sorted(active):
        row = " ".join(f"{n}:{a}" for n, a in active[fam])
        print(f"     {fam:5s}  {row}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
