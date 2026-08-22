
from pathlib import Path

import numpy as np
from PIL import Image

assets = Path(__file__).resolve().parents[1] / "plugin" / "assets"
SOURCE = assets / "trench_roller_strip.png.bak_pre_graphite_20260807"

KNEE = 0.28
DIFFUSE = 0.54
SPEC = 1.80
TINT = np.array((0.94, 0.99, 1.08), dtype=np.float32)

source = np.asarray(Image.open(SOURCE).convert("RGBA"), dtype=np.uint8)
if source.shape[1] % 417 != 0:
    raise RuntimeError(
        f"{SOURCE.name} is {source.shape[1]}px wide - not a 417-frame SS3 strip."
    )

rgb = source[..., :3].astype(np.float32)
alpha = source[..., 3]

peak = rgb.max(axis=2)
chroma = np.where(peak > 0, (peak - rgb.min(axis=2)) / np.maximum(peak, 1.0), 0.0)
lamp = np.clip((chroma - 0.20) / 0.15, 0.0, 1.0)
body = 1.0 - lamp

lum = rgb @ np.array((0.299, 0.587, 0.114), dtype=np.float32)
v = lum / 255.0
shaped = np.minimum(v, KNEE) * DIFFUSE + np.maximum(v - KNEE, 0.0) * SPEC
scale = np.where(lum > 1.0, shaped * 255.0 / np.maximum(lum, 1.0), DIFFUSE)
darker = np.clip(rgb * scale[..., None] * TINT, 0.0, 255.0)

out = source.copy()
out[..., :3] = np.rint(rgb + (darker - rgb) * body[..., None]).astype(np.uint8)
out[..., 3] = alpha

pending = assets / "trench_roller_strip.next.png"
Image.fromarray(out, "RGBA").save(pending)
pending.replace(assets / "trench_roller_strip.png")

was = lum[(alpha > 200) & (chroma <= 0.20)]
now = (out[..., :3].astype(np.float32) @ np.array((0.299, 0.587, 0.114)))[
    (alpha > 200) & (chroma <= 0.20)
]
print(f"body median L {np.median(was):.0f} -> {np.median(now):.0f}, "
      f"p99 {np.percentile(was, 99):.0f} -> {np.percentile(now, 99):.0f}")
