#!/usr/bin/env python3
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "dossiers" / "characters"
DST = ROOT / "recipes" / "architectures"
CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
UNIT_R = 0.9999

def st(a: float, b: float) -> float | None:
    if a and b and a > 0 and b > 0:
        return round(12.0 * math.log2(b / a), 2)
    return None

def main() -> int:
    DST.mkdir(parents=True, exist_ok=True)
    n = 0
    for f in sorted(SRC.glob("P2k_*.json")):
        d = json.loads(f.read_text())
        sections = []
        for s in range(6):
            lane = {cn: d["corners"][cn][s] for cn in CORNERS}
            sec = {
                "slot": s + 1,
                "pole_hz": {cn: lane[cn]["pole"]["hz"] for cn in CORNERS},
                "pole_r": {cn: lane[cn]["pole"]["radius"] for cn in CORNERS},
                "zero_hz": {cn: lane[cn]["zero"]["hz"] for cn in CORNERS},
                "zero_r": {cn: lane[cn]["zero"]["radius"] for cn in CORNERS},
                "carve_st": {cn: st(lane[cn]["pole"]["hz"],
                                    lane[cn]["zero"]["hz"])
                             for cn in CORNERS},
                "unit_zero": {cn: lane[cn]["zero"]["radius"] >= UNIT_R
                              for cn in CORNERS},
                "travel_st_Q0": st(lane["M0_Q0"]["pole"]["hz"],
                                   lane["M100_Q0"]["pole"]["hz"]),
                "q_lift_M0": round(lane["M0_Q100"]["pole"]["radius"]
                                   - lane["M0_Q0"]["pole"]["radius"], 6),
                "q_revoice_st_M0": st(lane["M0_Q0"]["pole"]["hz"],
                                      lane["M0_Q100"]["pole"]["hz"]),
                "scale_db": {cn: lane[cn].get("scale_db") for cn in CORNERS},
            }
            sections.append(sec)
        out = {
            "schema": "trench-architecture-v1",
            "index": d.get("index"),
            "name": d.get("name"),
            "x3_type": d.get("x3_type"),
            "datum_sr_hz": d.get("datum_sr_hz"),
            "source": str(f.relative_to(ROOT)).replace("\\", "/"),
            "sections": sections,
        }
        dst = DST / f.with_suffix(".json").name
        dst.write_text(json.dumps(out, indent=1))
        n += 1
    print(f"{n} architectures -> {DST.relative_to(ROOT)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
