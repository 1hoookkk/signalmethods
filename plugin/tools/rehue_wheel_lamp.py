
from __future__ import annotations

import sys

import numpy as np
from PIL import Image

def solve_screen_layer(rgb: np.ndarray) -> np.ndarray:
    red = rgb[..., 0]
    denominator = np.maximum(1.0 - red, 1.0 / 255.0)
    layer = np.zeros_like(rgb, dtype=np.float32)
    layer[..., 1] = np.clip((rgb[..., 1] - red) / denominator, 0.0, 1.0)
    layer[..., 2] = np.clip((rgb[..., 2] - red) / denominator, 0.0, 1.0)
    strength = np.maximum(layer[..., 1], layer[..., 2])
    gate = np.clip((strength - 0.018) / 0.055, 0.0, 1.0)
    gate = gate * gate * (3.0 - 2.0 * gate)
    return layer * gate[..., None]

def main() -> None:
    src, dst, hexcol = sys.argv[1], sys.argv[2], sys.argv[3]
    lamp = np.array([int(hexcol.lstrip("#")[i : i + 2], 16) for i in (0, 2, 4)],
                    dtype=np.float32) / 255.0

    rgba = np.asarray(Image.open(src).convert("RGBA"), dtype=np.float32) / 255.0
    layer = solve_screen_layer(rgba[..., :3])

    strength = np.maximum(layer[..., 1], layer[..., 2])

    body = 1.0 - (1.0 - rgba[..., :3]) / np.maximum(1.0 - layer, 1.0 / 255.0)
    body = np.clip(body, 0.0, 1.0)
    glow = strength[..., None] * lamp[None, None, :]
    rgb = 1.0 - (1.0 - body) * (1.0 - glow)

    out = np.dstack((np.clip(rgb, 0.0, 1.0), rgba[..., 3]))
    Image.fromarray((out * 255.0 + 0.5).astype(np.uint8), "RGBA").save(dst)

    lit = strength > 0.02
    print(f"lamp pixels {lit.sum()} / {strength.size}  peak strength {strength.max():.3f}")

if __name__ == "__main__":
    main()
