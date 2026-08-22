#!/usr/bin/env python3
from __future__ import annotations

import json
import math
import shutil
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
STRIP = ROOT / "plugin" / "assets" / "trench_knob_strip.png"
FRAME = 96
R_MIN, R_MAX = 8.0, 36.0

def lamp_from_theme() -> str:
    themes = json.loads((ROOT / "design" / "themes.json").read_text())
    theme = themes["themes"][themes["active"]]
    return theme.get("lamp", theme["accent"])

def main() -> None:
    lamp_hex = (sys.argv[1] if len(sys.argv) > 1 else lamp_from_theme()).lstrip("#")
    lamp = tuple(int(lamp_hex[i : i + 2], 16) for i in (0, 2, 4))
    lamp_lum = 0.299 * lamp[0] + 0.587 * lamp[1] + 0.114 * lamp[2]

    neutral = STRIP.with_name("trench_knob_strip_neutral.png")
    if not neutral.exists():
        shutil.copy2(STRIP, neutral)
    im = Image.open(neutral).convert("RGBA")
    frames = im.width // FRAME
    shutil.copy2(STRIP, STRIP.with_suffix(".png.bak_pre_pointer_lamp"))
    px = im.load()
    tinted = 0
    for f in range(frames - 1):
        x0 = f * FRAME
        for y in range(FRAME):
            for x in range(FRAME):
                r, g, b, a = px[x0 + x, y]
                if a < 200:
                    continue
                lum = 0.299 * r + 0.587 * g + 0.114 * b
                if lum < 150 or max(r, g, b) - min(r, g, b) > 60:
                    continue
                if not (R_MIN <= math.hypot(x - FRAME / 2, y - FRAME / 2) <= R_MAX):
                    continue
                s = min(1.6, lum / lamp_lum)
                px[x0 + x, y] = (
                    min(255, round(lamp[0] * s)),
                    min(255, round(lamp[1] * s)),
                    min(255, round(lamp[2] * s)),
                    a,
                )
                tinted += 1
    tmp = STRIP.with_name(STRIP.stem + "_tmp.png")
    im.save(tmp)
    tmp.replace(STRIP)
    print(f"baked pointer lamp #{lamp_hex} into {frames - 1}/{frames} frames "
          f"({tinted} px) -> {STRIP.name}; zero pose stays unlit")

if __name__ == "__main__":
    main()
