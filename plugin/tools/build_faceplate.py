from pathlib import Path

import numpy as np
from PIL import Image

ASSETS = Path(__file__).resolve().parents[1] / "plugin" / "assets"
SOURCE = ASSETS / "df2_panel_beige_source.png"
DONOR = ASSETS / "df2_panel_beige.png.bak_pre_mixlens_20260809"
TARGET = ASSETS / "df2_panel_beige.png"
SIZE = (828, 1280)

src_full = np.asarray(Image.open(SOURCE).convert("RGB"), dtype=np.float32)

RECESSES = ((576, 687, 758, 757), (576, 847, 758, 921))
CLEAN_Y = 762
FEATHER = 4.0
for x0, y0, x1, y1 in RECESSES:
    h, w = y1 - y0, x1 - x0
    patch = src_full[CLEAN_Y : CLEAN_Y + h, x0:x1].copy()
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    edge = np.minimum.reduce([yy + 1, h - yy, xx + 1, w - xx])
    blend = np.clip(edge / FEATHER, 0.0, 1.0)[..., None]
    region = src_full[y0:y1, x0:x1]
    src_full[y0:y1, x0:x1] = region * (1.0 - blend) + patch * blend

src = Image.fromarray(np.clip(src_full, 0, 255).astype(np.uint8), "RGB").resize(
    SIZE, Image.Resampling.LANCZOS)
scaled = np.asarray(src, dtype=np.float32)

donor = np.asarray(Image.open(DONOR).convert("RGBA"), dtype=np.float32)
if donor.shape[:2] != scaled.shape[:2]:
    raise SystemExit(f"donor {donor.shape[:2]} vs scaled source {scaled.shape[:2]}")

W = np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
src_l = scaled @ W
donor_l = donor[..., :3] @ W
alpha = donor[..., 3]

with np.errstate(divide="ignore", invalid="ignore"):
    ratio = np.where(src_l > 1.0, donor_l / np.maximum(src_l, 1.0), 1.0)
ratio = np.clip(ratio, 0.0, 1.0)
ratio[alpha == 0] = 1.0
SX, SY = 828.0 / 994.0, 1280.0 / 1536.0
for x0, y0, x1, y1 in RECESSES:
    ratio[int(y0 * SY) : int(y1 * SY) + 1, int(x0 * SX) : int(x1 * SX) + 1] = 1.0

out = np.empty((*scaled.shape[:2], 4), dtype=np.float32)
out[..., :3] = scaled * ratio[..., None]
out[..., 3] = alpha

Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA").save(TARGET)
shadowed = (ratio < 0.98).sum()
print(f"plate rebuilt from master: {shadowed} px carry measured shadow, "
      f"min ratio {ratio.min():.2f}; apertures from donor alpha "
      f"({int((alpha == 0).sum())} px punched)")
