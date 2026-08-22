from __future__ import annotations

import math
import sys
from pathlib import Path

from .core import (K_FLOOR, LANES, OCT_PER_K, RATE, ROOT, Sheet, describe,
                   pair_geometry, probe)

DEFAULT = ROOT / "ref" / "presets" / "P2k_013_talking_hedz.bin"


def selftest() -> int:
    raw = DEFAULT.read_bytes()
    sheet = Sheet.from_body(raw)
    print(f"read {DEFAULT.name}")
    print()
    for end in (0, 1):
        print(f"MORPH {end * 100}")
        for s in range(LANES):
            r = sheet.row(s, end)
            ks = "  ".join(f"{r.k(i):5.2f}" for i in range(4))
            print(f"  S{s+1}  k[{ks}]  {describe(r, RATE)}")
        print()
    built = sheet.body()
    same = built[:120] == raw[:120]
    print(f"body: {len(built)} bytes   both endpoints bit-identical: {same}")
    print(f"encoder floor: k = {K_FLOOR:.4f}")
    r = sheet.row(1, 0)
    before = r.k(2)
    r.set_k(2, before + 1.0 / OCT_PER_K)
    p0 = pair_geometry(before, r.k(3), RATE)
    p1 = r.pole(RATE)
    moved = math.log2(p1[0] / p0[0]) if p0 and p1 else float("nan")
    print(f"one k1 unit-octave step moved the pole {moved:+.3f} octaves")
    r.set_k(2, before)
    ok = (same and probe(built, 0.0, RATE) is not None
          and abs(abs(moved) - 1.0) < 0.15)
    print("PASS" if ok else "FAIL")
    return 0 if ok else 1


def main() -> int:
    args = sys.argv[1:]
    if "--selftest" in args:
        return selftest()
    from .app import run_ui
    given = next((a for a in args if not a.startswith("-")), None)
    raw = (Path(given) if given else DEFAULT).read_bytes()
    return run_ui(Sheet.from_body(raw), raw)


if __name__ == "__main__":
    raise SystemExit(main())
