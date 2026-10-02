from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

FS = 44_100.0
HOUSE_TOP = (10522.88, 351.19, 391.46, 936.85)
HOUSE_BOTTOM = (225.15, 124.39, 7221.20, 0.03)
ZERO_OCTAVES_ABOVE = 0.39
ZERO_RADIUS_BELOW = 0.03
PARKED = (22050.0, 1.0e9)


def radius_of(bw_hz: float) -> float:
    return math.exp(-math.pi * bw_hz / FS)


def bandwidth_of(radius: float) -> float:
    return -math.log(max(radius, 1e-9)) * FS / math.pi


def mask_zero(hz: float, bw_hz: float) -> tuple[float, float]:
    return hz * 2.0 ** ZERO_OCTAVES_ABOVE, bandwidth_of(radius_of(bw_hz) - ZERO_RADIUS_BELOW)


VOWEL_BANDWIDTHS = (60.0, 90.0, 150.0, 200.0, 250.0, 300.0)


def modes_of(obj: dict, category: str) -> list[tuple[float, float]]:
    rows = obj.get("formants") or obj.get("modes") or []
    picked = []
    for index, row in enumerate(rows):
        hz = row.get("frequency_hz")
        if not hz:
            continue
        bw = row.get("bandwidth_hz")
        if not bw and row.get("q"):
            bw = hz / row["q"]
        if not bw:
            bw = VOWEL_BANDWIDTHS[min(index, 5)] if category == "vowels" else max(2.0, hz / 500.0)
        picked.append((float(hz), float(bw), float(row.get("amplitude") or 1.0)))
    if category != "vowels" and len(picked) > 6:
        picked = sorted(picked, key=lambda m: -m[2])[:6]
    return sorted((hz, bw) for hz, bw, _ in picked)


def row_gain(row: tuple[float, float, float, float]) -> float:
    hz, bw, zero_hz, zero_bw = row
    rp = radius_of(bw)
    rz = radius_of(zero_bw)
    z1 = complex(math.cos(2.0 * math.pi * hz / FS), -math.sin(2.0 * math.pi * hz / FS))
    z2 = z1 * z1
    theta_z = 2.0 * math.pi * zero_hz / FS
    theta_p = 2.0 * math.pi * hz / FS
    peak = abs(1.0 - 2.0 * rz * math.cos(theta_z) * z1 + rz * rz * z2)
    peak /= abs(1.0 - 2.0 * rp * math.cos(theta_p) * z1 + rp * rp * z2)
    return max(-80.0, min(12.0, -20.0 * math.log10(max(peak, 1e-9))))


def build_rows(modes: list[tuple[float, float]], house: bool, mask: bool) -> list[tuple[float, float, float, float]]:
    voicing = modes[: 4 if house else 6]
    rows = []
    if house:
        rows.append(HOUSE_TOP)
    for hz, bw in voicing:
        zero = mask_zero(hz, bw) if mask else PARKED
        rows.append((hz, bw, zero[0], zero[1]))
    if house:
        rows.append(HOUSE_BOTTOM)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("table", type=Path)
    parser.add_argument("object_id", nargs="?")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--no-house", action="store_true")
    parser.add_argument("--no-mask", action="store_true")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args()
    data = json.loads(args.table.read_text(encoding="utf-8"))
    objects = data.get("objects", [])
    if args.list or not args.object_id:
        for obj in objects:
            print(obj.get("object_id"), "·", obj.get("label"), "·", len(modes_of(obj, data.get("category", ""))), "modes")
        return 0
    match = [o for o in objects if o.get("object_id") == args.object_id]
    if not match:
        print("no object", args.object_id)
        return 1
    obj = match[0]
    modes = modes_of(obj, data.get("category", ""))
    if not modes:
        print("object has no frequency/bandwidth rows")
        return 1
    house = not args.no_house and data.get("category") == "vowels"
    rows = build_rows(modes, house, not args.no_mask)
    out = args.out or args.table.with_name(f"{data.get('dataset', 'table')}_{args.object_id}.fbw")
    with open(out, "w", encoding="utf-8") as handle:
        handle.write(f"# {data.get('dataset')} {obj.get('label')}\n")
        for row in rows:
            handle.write("%.4f %.4f %.4f %.4f\n" % row)
    print("    pole Hz   pole BW   zero Hz   zero BW")
    for row in rows:
        print("%10.1f %9.1f %10.1f %9.1f" % row)
    print("->", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
