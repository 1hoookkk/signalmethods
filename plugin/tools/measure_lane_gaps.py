#!/usr/bin/env python3
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main() -> None:
    gaps, voices = [], []
    for f in sorted((ROOT / "dossiers" / "characters").glob("P2k_*.json")):
        d = json.loads(f.read_text())
        for cn in ("M0_Q0", "M100_Q0"):
            poles = sorted(p["pole"]["hz"] for p in d["corners"][cn]
                           if p["pole"]["hz"] and p["pole"]["hz"] > 20
                           and p["pole"]["radius"] >= 0.5)
            voices.append(len(poles))
            gaps.extend(12 * math.log2(b / a)
                        for a, b in zip(poles, poles[1:]))
    gaps.sort()
    n = len(gaps)
    print(f"{n} adjacent-lane gaps, {len(voices)} corners, "
          f"mean {sum(voices) / len(voices):.1f} voices/corner")
    for name, lo, hi in (("touching  <= 2 st", 0, 2),
                         ("cluster   2-5 st", 2, 5),
                         ("spread    5-12 st", 5, 12),
                         ("wide      1-2 oct", 12, 24),
                         ("frame gap > 2 oct", 24, 1e9)):
        c = sum(lo < g <= hi for g in gaps) if lo else sum(g <= hi for g in gaps)
        print(f"  {name:18s} {c:4d}  ({100 * c / n:.0f}%)")
    print(f"median {gaps[n // 2]:.1f} st")

if __name__ == "__main__":
    main()
