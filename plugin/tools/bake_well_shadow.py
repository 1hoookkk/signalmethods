
from __future__ import annotations

import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
PLATE = ROOT / "plugin" / "assets" / "df2_panel_beige.png"
BACKUP = PLATE.with_suffix(".png.bak_pre_wellshadow_20260806")

PANEL_SIZE = (828, 1280)
OPENING_X = (97, 445)
WELLS_Y = ((562, 627), (698, 764))

LIP_RISE = 7
LIP_STRENGTH = 0.62
INNER_DROP = 5
INNER_STRENGTH = 0.45
BLUR = 1.6

def shadow_mask(size: tuple[int, int]) -> np.ndarray:
    mask = Image.new("F", size, 0.0)
    draw = ImageDraw.Draw(mask)
    x0, x1 = OPENING_X

    for top, bottom in WELLS_Y:
        for i in range(LIP_RISE):
            y = top - i
            a = LIP_STRENGTH * (1.0 - i / LIP_RISE) ** 1.5
            draw.line([(x0, y), (x1, y)], fill=a)
        for i in range(INNER_DROP):
            y = top + i
            a = INNER_STRENGTH * (1.0 - i / INNER_DROP)
            draw.line([(x0, y), (x1, y)], fill=max(a, 0.0))

    mask = mask.filter(ImageFilter.GaussianBlur(BLUR))
    return 1.0 - np.asarray(mask, dtype=np.float32)

def main() -> int:
    preview = "--preview" in sys.argv
    plate = Image.open(PLATE).convert("RGBA")
    if plate.size != PANEL_SIZE:
        raise SystemExit(f"{PLATE.name} is {plate.size}, expected {PANEL_SIZE} - "
                         "the measured geometry does not apply.")

    rgba = np.asarray(plate, dtype=np.float32) / 255.0
    factor = shadow_mask(plate.size)[..., None]
    rgb = np.clip(rgba[..., :3] * factor, 0.0, 1.0)
    out = Image.fromarray(
        (np.dstack((rgb, rgba[..., 3])) * 255.0 + 0.5).astype(np.uint8), "RGBA")

    if preview:
        crop = (OPENING_X[0] - 20, WELLS_Y[0][0] - 30, OPENING_X[1] + 20, WELLS_Y[1][1] + 30)
        before, after = plate.crop(crop), out.crop(crop)
        strip = Image.new("RGBA", (before.width, before.height * 2 + 8), (0, 0, 0, 255))
        strip.paste(before, (0, 0))
        strip.paste(after, (0, before.height + 8))
        dst = ROOT / "well_shadow_preview.png"
        strip.resize((strip.width * 2, strip.height * 2), Image.NEAREST).save(dst)
        print(f"preview -> {dst} (top = current, bottom = baked)")
        return 0

    if not BACKUP.exists():
        shutil.copy2(PLATE, BACKUP)
        print(f"backed up -> {BACKUP.name}")
    out.save(PLATE)
    darkened = int((factor[..., 0] < 0.999).sum())
    print(f"baked well recess into {PLATE.name} ({darkened} px darkened)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
