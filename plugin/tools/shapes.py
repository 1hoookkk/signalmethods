from __future__ import annotations

import json
import math

from ffi import ROOT

DOSSIERS = ROOT / "dossiers" / "characters"
CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]

def collect() -> list[dict]:
    shapes: list[dict] = []
    seen: dict[tuple, dict] = {}
    for f in sorted(DOSSIERS.glob("P2k_*.json")):
        d = json.loads(f.read_text())
        name = d.get("name") or f.stem
        for cn in CORNERS:
            for s, row in enumerate(d["corners"][cn]):
                pole, zero = row.get("pole") or {}, row.get("zero") or {}
                ph = float(pole.get("hz") or 0.0)
                zh = float(zero.get("hz") or 0.0)
                if ph <= 20.0 or zh <= 20.0:
                    continue
                carve = 12.0 * math.log2(zh / ph)
                zr = float(zero.get("radius") or 0.0)
                key = (round(carve, 1), round(zr, 3))
                if key in seen:
                    seen[key]["count"] += 1
                    continue
                rec = dict(carve_st=carve, zero_r=zr,
                           src=f"{name} S{s + 1} {cn}", count=1)
                seen[key] = rec
                shapes.append(rec)
    shapes.sort(key=lambda r: r["carve_st"])
    return shapes
