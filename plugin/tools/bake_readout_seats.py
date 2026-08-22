#!/usr/bin/env python3
from __future__ import annotations

import shutil
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageEnhance

ROOT = Path(__file__).resolve().parent.parent
PLATE = ROOT / "plugin" / "assets" / "df2_panel_beige.png"
PRISTINE = PLATE.with_name("df2_panel_beige_pre_readout_seats.png")

SRC_W, SRC_H = 1010.0, 1557.0
ED_W, ED_H = 326.0, 503.0
SEATS = [
    (586.0, 705.0, 182.0, 64.0, 21.0),
    (586.0, 873.5, 182.0, 64.0, 21.0),
    (230.0, 139.0, 675.0, 68.0, 14.0),
]
OFF_X_ED, OFF_Y_ED = 2.0, 3.0
SHADOW_STRENGTH = 0.50
MICRO_CONTRAST = 0.18

def main() -> None:
    if not PRISTINE.exists():
        shutil.copy2(PLATE, PRISTINE)
    im = Image.open(PRISTINE).convert("RGBA")
    rgb = im.convert("RGB")
    rgb = rgb.filter(ImageFilter.UnsharpMask(radius=3, percent=int(MICRO_CONTRAST * 100), threshold=2))
    rgb = ImageEnhance.Contrast(rgb).enhance(1.0 + MICRO_CONTRAST * 0.25)
    im = Image.merge("RGBA", (*rgb.split(), im.split()[3]))
    w, h = im.size
    sx, sy = w / SRC_W, h / SRC_H
    ex, ey = w / ED_W, h / ED_H

    _ = (sx, sy, ex, ey, SEATS, OFF_X_ED, OFF_Y_ED, SHADOW_STRENGTH, ImageDraw)

    tmp = PLATE.with_name(PLATE.stem + "_tmp.png")
    im.save(tmp)
    tmp.replace(PLATE)
    print(f"plate micro-contrast {MICRO_CONTRAST} baked (readout seats NUKED "
          f"2026-08-10 — the contact is code-drawn) -> {PLATE.name}")

if __name__ == "__main__":
    main()
