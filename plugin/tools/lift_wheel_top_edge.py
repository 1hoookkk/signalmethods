#!/usr/bin/env python3
from __future__ import annotations

import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
STRIP = ROOT / "plugin" / "assets" / "trench_roller_strip.png"
SEARCH_DEPTH = 8
LIT_LUM = 130.0
EDGE_SHADE = 0.82

def main() -> None:
    im = Image.open(STRIP).convert("RGBA")
    backup = STRIP.with_suffix(".png.bak_pre_edge_lift")
    if not backup.exists():
        shutil.copy2(STRIP, backup)
    px = im.load()
    w, h = im.size
    lifted = 0
    for x in range(w):
        y_top = next((y for y in range(h) if px[x, y][3] > 200), None)
        if y_top is None:
            continue
        y_lit = None
        for y in range(y_top, min(h, y_top + SEARCH_DEPTH)):
            r, g, b, _ = px[x, y]
            if 0.299 * r + 0.587 * g + 0.114 * b >= LIT_LUM:
                y_lit = y
                break
        if y_lit is None or y_lit == y_top:
            continue
        rim = px[x, y_lit]
        for y in range(y_top, y_lit):
            a = px[x, y][3]
            px[x, y] = (
                round(rim[0] * EDGE_SHADE),
                round(rim[1] * EDGE_SHADE),
                round(rim[2] * EDGE_SHADE),
                a,
            )
            lifted += 1
    tmp = STRIP.with_name(STRIP.stem + "_tmp.png")
    im.save(tmp)
    tmp.replace(STRIP)
    print(f"lifted {lifted} px of ceiling band to rim light -> {STRIP.name}")

if __name__ == "__main__":
    main()
