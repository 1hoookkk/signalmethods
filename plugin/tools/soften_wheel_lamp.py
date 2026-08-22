import sys

import numpy as np
from PIL import Image, ImageFilter

STRIP = r"C:\Users\hooki\trench-native\plugin\plugin\assets\trench_roller_strip.png"

BLEED = float(sys.argv[1]) if len(sys.argv) > 1 else 3.0
AMOUNT = float(sys.argv[2]) if len(sys.argv) > 2 else 0.28

im = Image.open(STRIP).convert("RGBA")
a = np.array(im).astype(np.float32)
rgb = a[..., :3]

value = rgb.max(axis=2)
chroma = np.where(value > 0, (value - rgb.min(axis=2)) / np.maximum(value, 1.0), 0.0)
lamp = (a[..., 3] > 200) & (chroma > 0.30) & (value > 45)
if not lamp.any():
    raise SystemExit("no lamp pixels found - run retint_roller_glow.py first")

layer = np.zeros_like(rgb)
layer[lamp] = rgb[lamp]
blur = np.stack([
    np.array(Image.fromarray(layer[..., c].astype(np.uint8))
             .filter(ImageFilter.GaussianBlur(BLEED))).astype(np.float32)
    for c in range(3)
], axis=2)

reach = np.array(Image.fromarray((lamp * 255).astype(np.uint8))
                 .filter(ImageFilter.GaussianBlur(BLEED))).astype(np.float32) / 255.0
gate = np.clip(reach * 2.2, 0.0, 1.0)[..., None]
out = 255.0 - (255.0 - rgb) * (255.0 - blur * AMOUNT * gate) / 255.0
a[..., :3] = np.clip(out, 0, 255)

Image.fromarray(a.astype(np.uint8), "RGBA").save(STRIP)
print(f"softened {int(lamp.sum())} lamp px: {BLEED}px bleed at {AMOUNT:.2f}; body and alpha unchanged")
