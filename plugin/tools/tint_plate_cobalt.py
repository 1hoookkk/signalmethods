#!/usr/bin/env python3
from __future__ import annotations

import shutil
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
PLATE = ROOT / "plugin" / "assets" / "df2_panel_beige.png"
BACKUP = PLATE.with_name("df2_panel_beige_pre_cobalt.png")

RAMP = [
    (0.00, (6, 8, 12)),
    (0.35, (24, 31, 46)),
    (0.65, (42, 53, 74)),
    (0.85, (66, 82, 110)),
    (1.00, (96, 115, 148)),
]

def ramp(lum: float) -> tuple[int, int, int]:
    for (s0, c0), (s1, c1) in zip(RAMP, RAMP[1:]):
        if lum <= s1:
            t = 0.0 if s1 == s0 else (lum - s0) / (s1 - s0)
            return tuple(int(a + (b - a) * t) for a, b in zip(c0, c1))
    return RAMP[-1][1]

def main() -> None:
    if len(sys.argv) > 1 and sys.argv[1] == "restore":
        if BACKUP.exists():
            shutil.copy2(BACKUP, PLATE)
            print("beige restored")
        return
    if not BACKUP.exists():
        shutil.copy2(PLATE, BACKUP)
    im = Image.open(BACKUP).convert("RGBA")
    px = im.load()
    w, h = im.size
    lut = [ramp(i / 255.0) for i in range(256)]
    for j in range(h):
        for i in range(w):
            r, g, b, a = px[i, j]
            lum = int(0.299 * r + 0.587 * g + 0.114 * b)
            cr, cg, cb = lut[lum]
            px[i, j] = (cr, cg, cb, a)
    tmp = PLATE.with_name(PLATE.stem + "_tmp.png")
    im.save(tmp)
    tmp.replace(PLATE)
    print(f"plate -> dark cobalt metal ({w}x{h}); restore with: "
          f"python tools/tint_plate_cobalt.py restore")

if __name__ == "__main__":
    main()
