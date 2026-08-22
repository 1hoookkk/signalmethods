
from pathlib import Path

import numpy as np
from PIL import Image

assets = Path(__file__).resolve().parents[1] / "plugin" / "assets"
SOURCE = assets / "trench_knob_strip.png"
BACKUP = assets / "trench_knob_strip.png.bak_pre_graphite_20260807"

GAMMA = 1.45
TINT = np.array((0.94, 0.99, 1.08), dtype=np.float32)

src = np.asarray(Image.open(BACKUP).convert("RGBA"), dtype=np.uint8)
if src.shape[1] % 96 != 0:
    raise RuntimeError(f"{BACKUP.name} is {src.shape[1]}px wide - not a 96px filmstrip.")

rgb = src[..., :3].astype(np.float32)
alpha = src[..., 3]
lum = rgb @ np.array((0.299, 0.587, 0.114), dtype=np.float32)

neutral = np.repeat(lum[..., None], 3, axis=2) * TINT
deepened = np.clip(np.power(np.clip(neutral, 0.0, 255.0) / 255.0, GAMMA) * 255.0, 0.0, 255.0)

out = src.copy()
out[..., :3] = np.rint(deepened).astype(np.uint8)
out[..., 3] = alpha

pending = assets / "trench_knob_strip.next.png"
Image.fromarray(out, "RGBA").save(pending)
pending.replace(SOURCE)

body = alpha > 200
old = rgb[body]
new = out[..., :3].astype(np.float32)[body]
print(f"R-B {(old[:, 0] - old[:, 2]).mean():+.2f} -> {(new[:, 0] - new[:, 2]).mean():+.2f}, "
      f"mean L {lum[body].mean():.1f} -> "
      f"{(new @ np.array((0.299, 0.587, 0.114))).mean():.1f}")
