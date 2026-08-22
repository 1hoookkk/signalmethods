"""Recolour the locked DF2 one-tooth wheel without changing its geometry.

The retained master is the accepted 128 x 139x31 cycle-10 strip from
df2-workstation.  Its cyan field was measured from E-mu BITMAP4331_2.bmp.
This tool solves that screen-blended field back out, recomposes it with the
selected two-colour lamp, and appends frame 0 as the byte-identical E-mu wrap
sentinel (frame 128).  The dark saucer body and every rib stay unchanged.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image


ASSETS = Path(__file__).resolve().parents[1] / "plugin" / "assets"
SOURCE = ASSETS / "trench_roller_cycle10_master_128_139x31.png"
TARGET = ASSETS / "trench_roller_strip.png"
FRAME_WIDTH = 139
FRAME_HEIGHT = 31
SOURCE_FRAME_COUNT = 128
OUTPUT_FRAME_COUNT = 129


def colour(hex_value: str) -> np.ndarray:
    value = hex_value.strip().lstrip("#")
    if len(value) != 6:
        raise SystemExit(f"expected RRGGBB, got {hex_value!r}")
    return np.array([int(value[i : i + 2], 16) for i in (0, 2, 4)], dtype=np.float32) / 255.0


def smoothstep(edge0: float, edge1: float, value: np.ndarray) -> np.ndarray:
    t = np.clip((value - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def solve_screen_layer(source_rgb: np.ndarray) -> np.ndarray:
    """Recover the master's cyan screen layer using red as neutral baseline."""
    red = source_rgb[..., 0]
    denominator = np.maximum(1.0 - red, 1.0 / 255.0)
    layer = np.zeros_like(source_rgb, dtype=np.float32)
    layer[..., 1] = np.clip((source_rgb[..., 1] - red) / denominator, 0.0, 1.0)
    layer[..., 2] = np.clip((source_rgb[..., 2] - red) / denominator, 0.0, 1.0)
    strength = np.maximum(layer[..., 1], layer[..., 2])
    gate = smoothstep(0.018, 0.073, strength)
    return layer * gate[..., None]


def recolour(source: np.ndarray, falloff: np.ndarray, core: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    rgb = source[..., :3]
    old_layer = solve_screen_layer(rgb)
    strength = np.maximum(old_layer[..., 1], old_layer[..., 2])

    # Invert the original screen blend only where the measured field exists.
    body = 1.0 - (1.0 - rgb) / np.maximum(1.0 - old_layer, 1.0 / 255.0)
    body = np.clip(body, 0.0, 1.0)

    # Preserve the measured field's brightness.  The two supplied colours set
    # hue/chroma: dim transmission is the deep shade and the hottest centre
    # rolls into the pale shade instead of becoming a separate bulb train.
    falloff_hue = falloff / max(float(falloff.max()), 1.0 / 255.0)
    core_hue = core / max(float(core.max()), 1.0 / 255.0)
    core_mix = smoothstep(0.46, 0.88, strength)
    lamp_hue = falloff_hue + (core_hue - falloff_hue) * core_mix[..., None]
    new_layer = strength[..., None] * lamp_hue
    lit = 1.0 - (1.0 - body) * (1.0 - new_layer)

    result = source.copy()
    field = strength > 0.0
    result[..., :3] = np.where(field[..., None], lit, rgb)
    return np.clip(result, 0.0, 1.0), field


def main() -> None:
    falloff_hex = sys.argv[1] if len(sys.argv) > 1 else "#6F7C12"
    core_hex = sys.argv[2] if len(sys.argv) > 2 else "#E2EB98"

    source_u8 = np.asarray(Image.open(SOURCE).convert("RGBA"), dtype=np.uint8)
    expected = (FRAME_HEIGHT, FRAME_WIDTH * SOURCE_FRAME_COUNT, 4)
    if source_u8.shape != expected:
        raise RuntimeError(f"{SOURCE.name} shape {source_u8.shape}; expected {expected}")

    source = source_u8.astype(np.float32) / 255.0
    recoloured, field = recolour(source, colour(falloff_hex), colour(core_hex))
    recoloured_u8 = np.rint(recoloured * 255.0).astype(np.uint8)

    frames = recoloured_u8.reshape(FRAME_HEIGHT, SOURCE_FRAME_COUNT, FRAME_WIDTH, 4)
    output = np.concatenate((frames, frames[:, 0:1]), axis=1).reshape(
        FRAME_HEIGHT, OUTPUT_FRAME_COUNT * FRAME_WIDTH, 4
    )
    if not np.array_equal(output[:, -FRAME_WIDTH:], output[:, :FRAME_WIDTH]):
        raise RuntimeError("frame 128 is not a byte-identical wrap to frame 0")

    pending = TARGET.with_suffix(".next.png")
    Image.fromarray(output, "RGBA").save(pending)
    pending.replace(TARGET)
    print(
        f"rebaked locked saucer wheel: {OUTPUT_FRAME_COUNT} x {FRAME_WIDTH}x{FRAME_HEIGHT}; "
        f"{int(field.sum())} measured-field pixels -> {falloff_hex} / {core_hex}; "
        "frame 128 == frame 0"
    )


if __name__ == "__main__":
    main()
