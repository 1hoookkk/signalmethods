
from pathlib import Path

import numpy as np
from PIL import Image
from scipy.ndimage import distance_transform_edt, gaussian_filter

assets = Path(__file__).resolve().parents[1] / "plugin" / "assets"
SOURCE = assets / "df2_panel_beige.png"
BACKUP = assets / "df2_panel_beige.png.bak_pre_wheelshadow_20260808"

rgba = np.asarray(Image.open(BACKUP).convert("RGBA"), dtype=np.float32)
plate = rgba[..., :3]
H, W = plate.shape[:2]
LX, LY = W / 1010.0, H / 1557.0

def rounded_rect(x0, y0, x1, y1, r):
    cx = np.clip(xx, x0 + r, x1 - r)
    cy = np.clip(yy, y0 + r, y1 - r)
    return (xx - cx) ** 2 + (yy - cy) ** 2 <= r * r

def half_oval(x0, x1, ybot, depth, strength, gamma):
    cx, a = 0.5 * (x0 + x1), 0.5 * (x1 - x0)
    r = np.sqrt(((xx - cx) / a) ** 2 + ((yy - ybot) / depth) ** 2)
    return np.where(yy > ybot,
                    np.clip(1.0 - r, 0.0, 1.0) ** gamma * strength, 0.0)

def seat(mask, source, reach, strength, gamma):
    d = distance_transform_edt(~source)
    rim = np.clip(1.0 - d / reach, 0.0, 1.0) ** gamma * strength
    return np.where(mask, 0.0, rim)

xx, yy = np.meshgrid(np.arange(W, dtype=np.float32), np.arange(H, dtype=np.float32))

def part(x0, y0, x1, y1, r, grow=0.0):
    x0, y0, x1, y1, r = x0 - grow, y0 - grow, x1 + grow, y1 + grow, r + grow
    bx, by = BIAS
    return (rounded_rect(x0, y0, x1, y1, r),
            rounded_rect(x0 + bx, y0 + by, x1 + bx, y1 + by, r * CORNER))

CORNER = 0.45

BIAS = (0.0, 2.0)

BEZEL, WELL_R, FALL = 10.0, 10.0, 0.28
WELLS = [(99.0, 562.0, 443.0, 626.0),
         (99.0, 698.0, 443.0, 764.0)]

REACH, STRENGTH, GAMMA = 5.5, 0.42, 1.8

FALL_STRENGTH, FALL_GAMMA = 0.45, 1.1

READOUTS = [part(576.0 * LX, y * LY, 776.0 * LX, (y + 80.0) * LY, 11.2)
            for y in (699.0, 860.0)]

WHEEL_DROPS = [half_oval(x0 - BEZEL, x1 + BEZEL, y1 + BEZEL,
                         FALL * (y1 - y0), FALL_STRENGTH, FALL_GAMMA)
               for (x0, y0, x1, y1) in WELLS]

shadow = np.maximum.reduce([seat(m, src, REACH, STRENGTH, GAMMA)
                            for m, src in READOUTS] + WHEEL_DROPS)

out = rgba.copy()
out[..., :3] = np.clip(plate * (1.0 - shadow)[..., None], 0.0, 255.0)

RING_A, RING_W, BOTTOM_FADE = 0.34, 5.0, 0.85
TOP_A, TOP_FRAC, TOP_GAMMA = 0.62, 0.29, 1.1
inside = np.zeros((H, W), dtype=bool)
for x0, y0, x1, y1 in WELLS:
    inside |= rounded_rect(x0, y0, x1, y1, WELL_R)
inside &= out[..., 3] == 0
d_wall = distance_transform_edt(inside)
for x0, y0, x1, y1 in WELLS:
    band = inside & (yy >= y0) & (yy <= y1)
    frac = np.clip((yy - y0) / (y1 - y0), 0.0, 1.0)
    ring = RING_A * np.clip(1.0 - d_wall / RING_W, 0.0, 1.0) ** 1.2
    ring *= 1.0 - BOTTOM_FADE * frac
    top_w = TOP_FRAC * (y1 - y0)
    cast = TOP_A * np.clip(1.0 - (yy - y0) / top_w, 0.0, 1.0) ** TOP_GAMMA
    well_shade = np.clip(ring + cast, 0.0, 0.75)
    out[..., 3] = np.where(band, np.rint(well_shade * 255.0), out[..., 3])
    out[..., :3] = np.where(band[..., None], 0.0, out[..., :3])

pending = assets / "df2_panel_beige.next.png"
Image.fromarray(np.rint(out).astype(np.uint8), "RGBA").save(pending)
pending.replace(SOURCE)

lit = shadow > 0.02
print(f"baked {int(lit.sum())} px of drop shadow, peak {shadow.max():.2f}; "
      f"plate mean {plate[lit].mean():.1f} -> {out[..., :3][lit].mean():.1f}")
