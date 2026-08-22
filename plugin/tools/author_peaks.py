#!/usr/bin/env python3
"""Author a body from a table of peaks. Front door for the real compiler.

You write frequencies. It writes a .body240 into bodies/candidates/, which the
plugin hot-scans. Packing delegates to trench-core's body-from-geometry - no
packed math here.

Spec format (plain text). Blank lines and # comments ignored:

    name  vowel_space
    M0Q0      325/60  700/90  2530/140  3500/200  4950/250
    M100Q0    450/60  800/90  2830/140  3500/200  4950/250
    M0Q100    400/60 1600/90  2700/140  3300/200  4900/250
    M100Q100  800/60 1150/90  2800/140  3500/200  4950/250

Each entry is  HZ/BANDWIDTH  or  HZ/BANDWIDTH/PEAK_DB  (peak defaults to 18 dB).
Up to 6 per corner; short rows are padded with inert stages.

Why pole and zero sit at the SAME frequency: that is a parametric peak. Away
from the peak the pole and zero cancel and the response returns to flat -
which is the authored Morpheus law ("the response at high frequencies is
essentially flat"). Parking zeros at DC instead is what makes a lowpass.

    python tools/author_peaks.py my_body.txt
    python tools/author_peaks.py my_body.txt --out-dir bodies/candidates
"""
from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPILER = ROOT / "target" / "release" / "body-from-geometry.exe"
if not COMPILER.exists():
    COMPILER = ROOT / "target" / "release" / "body-from-geometry"

SR = 39_062.5
CORNERS = ["M0Q0", "M100Q0", "M0Q100", "M100Q100"]
NUM_STAGES = 6
DEFAULT_PEAK_DB = 18.0
INERT = {"pole_hz": 1000.0, "pole_r": 0.5, "zero_hz": 1000.0, "zero_r": 0.5, "scale": 1.0}

def radius_for_bandwidth(bw_hz: float) -> float:
    r = 1.0 - math.pi * bw_hz / SR
    return min(max(r, 0.05), 0.9995)

def zero_radius_for_peak(pole_r: float, peak_db: float) -> float:
    lift = 10.0 ** (peak_db / 20.0)
    zr = 1.0 - (1.0 - pole_r) * lift
    return min(max(zr, 0.0), 0.9995)

def parse_spec(text: str):
    name = None
    corners: dict[str, list[dict]] = {}
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        head, *rest = line.split()
        key = head.rstrip(":")
        if key.lower() == "name":
            if not rest:
                sys.exit(f"line {lineno}: 'name' needs a value")
            name = rest[0]
            continue
        if key not in CORNERS:
            sys.exit(f"line {lineno}: unknown corner {key!r}; expected one of {', '.join(CORNERS)}")
        if not rest:
            sys.exit(f"line {lineno}: corner {key} has no peaks")
        if len(rest) > NUM_STAGES:
            sys.exit(f"line {lineno}: corner {key} has {len(rest)} peaks, max {NUM_STAGES}")
        stages = []
        for tok in rest:
            parts = tok.split("/")
            if len(parts) not in (2, 3):
                sys.exit(f"line {lineno}: {tok!r} must be HZ/BANDWIDTH or HZ/BANDWIDTH/PEAK_DB")
            try:
                hz, bw = float(parts[0]), float(parts[1])
                peak_db = float(parts[2]) if len(parts) == 3 else DEFAULT_PEAK_DB
            except ValueError:
                sys.exit(f"line {lineno}: {tok!r} has a non-numeric field")
            if not 20.0 <= hz <= SR * 0.45:
                sys.exit(f"line {lineno}: {hz} Hz out of range (20 .. {SR * 0.45:.0f})")
            pr = radius_for_bandwidth(bw)
            stages.append({
                "pole_hz": hz, "pole_r": pr,
                "zero_hz": hz, "zero_r": zero_radius_for_peak(pr, peak_db),
                "scale": 1.0,
            })
        stages += [dict(INERT) for _ in range(NUM_STAGES - len(stages))]
        corners[key] = stages
    missing = [c for c in CORNERS if c not in corners]
    if missing:
        sys.exit(f"spec is missing corners: {', '.join(missing)}")
    if not name:
        sys.exit("spec needs a 'name' line")
    return name, corners

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("spec", help="peak table (see module docstring)")
    ap.add_argument("--out-dir", default="bodies/candidates",
                    help="where the .body240 lands (default: bodies/candidates, hot-scanned)")
    args = ap.parse_args()

    if not COMPILER.exists():
        sys.exit(f"compiler missing: {COMPILER}\n"
                 "build: cargo build --release -p trench-core --bin body-from-geometry")

    name, corners = parse_spec(Path(args.spec).read_text(encoding="utf-8"))
    geometry = {"name": name, "corners": [corners[c] for c in CORNERS]}

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    geo_path = out_dir / f"{name}.geometry.json"
    body_path = out_dir / f"{name}.body240"
    geo_path.write_text(json.dumps(geometry, indent=1), encoding="utf-8")

    result = subprocess.run([str(COMPILER), str(geo_path), str(body_path)],
                            capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"body-from-geometry failed:\n{result.stdout}{result.stderr}")

    print(f"wrote {body_path} ({body_path.stat().st_size} bytes)")
    for corner_name in CORNERS:
        live = [s for s in corners[corner_name] if s["zero_r"] != s["pole_r"]]
        peaks = "  ".join(f"{s['pole_hz']:.0f}" for s in live)
        print(f"  {corner_name:9} {peaks}")
    print("\nthe plugin hot-scans bodies/candidates - reload the body list to hear it")

if __name__ == "__main__":
    main()
