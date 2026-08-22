#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TAKES = ROOT / "evidence" / "x3_takes"
BYPASS = TAKES / "_source" / "bypass_pinknoise.wav"
CAND = ROOT / "bodies" / "candidates"
PY = sys.executable

def corners_distinct(set_dir: Path) -> int:
    names = [f"{set_dir.name}_m0q0.wav", f"{set_dir.name}_m100q0.wav",
             f"{set_dir.name}_m0q100.wav", f"{set_dir.name}_m100q100.wav"]
    hashes = set()
    for n in names:
        p = set_dir / n
        if not p.exists():
            return 0
        hashes.add(hash(p.read_bytes()))
    return len(hashes)

def main() -> int:
    missing = []
    for set_dir in sorted(TAKES.glob("X3_*")):
        if not set_dir.is_dir():
            continue
        short = set_dir.name[len("X3_"):]
        if (CAND / f"CR_{short}.body240").exists():
            continue
        missing.append((set_dir, short))

    if not missing:
        print("no missing capture sets - batch complete")
        return 0

    print(f"batch: {len(missing)} sets missing CR_* bodies")
    failures = []
    for set_dir, short in missing:
        n = corners_distinct(set_dir)
        if n < 4:
            failures.append(f"{short}: {n}/4 distinct corners - needs recapture")
            continue
        name = f"CR_{short}"
        specs = [f"diff:{set_dir / f'{set_dir.name}_{suffix}.wav'}|{BYPASS}"
                 for suffix in ("m0q0", "m100q0", "m0q100", "m100q100")]
        print(f"\n=== {name} ===")
        r = subprocess.run([PY, str(ROOT / "tools" / "make_body.py"), name, *specs])
        if r.returncode != 0:
            failures.append(f"{name}: make_body failed ({r.returncode})")
            continue
        r2 = subprocess.run([PY, str(ROOT / "tools" / "inspect_body.py"),
                             str(CAND / f"{name}.body240")])
        if r2.returncode != 0:
            failures.append(f"{name}: inspect_body failed ({r2.returncode})")
        breakdown = subprocess.run(
            [PY, str(ROOT / "tools" / "sos_breakdown.py"),
             str(CAND / f"{name}.body240")])
        if breakdown.returncode != 0:
            failures.append(f"{name}: sos_breakdown failed ({breakdown.returncode})")

    print("\n=== batch summary ===")
    if failures:
        print("needs attention:")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("all missing sets produced, certified, and inspected")
    return 0

if __name__ == "__main__":
    sys.exit(main())
