
from __future__ import annotations

import colorsys
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ASSETS = Path(__file__).resolve().parents[1] / "plugin" / "assets"
SOURCE = ASSETS / "trench_roller_strip_cyan_backup.png"
TARGET = ASSETS / "trench_roller_strip.png"

NARROW = 2.05
GAIN = 1.95
ABSORB = 7.5
BODY_GAMMA = 1.00
BODY_TINT = np.array([1.000, 1.000, 1.000], dtype=np.float32)
FRAME_WIDTH = 417
SOURCE_FRAME_COUNT = 128
OUTPUT_FRAME_COUNT = 129

def hex_to_rgb(h: str) -> tuple[float, float, float]:
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))

def hot_core(body: tuple[float, float, float]) -> tuple[float, float, float]:
    h, l, s = colorsys.rgb_to_hls(*body)
    return colorsys.hls_to_rgb(h, min(0.92, l + 0.22), s * 0.45)


def retime_frames(source: np.ndarray) -> np.ndarray:
    """Retime the selected 128-frame wheel onto the reference's 0..128 grid."""
    height, width, channels = source.shape
    if channels != 4 or width != FRAME_WIDTH * SOURCE_FRAME_COUNT:
        raise SystemExit(
            f"expected {SOURCE_FRAME_COUNT} x {FRAME_WIDTH}px RGBA wheel source, "
            f"got {source.shape}"
        )

    frames = source.reshape(height, SOURCE_FRAME_COUNT, FRAME_WIDTH, 4).transpose(1, 0, 2, 3)
    output = np.empty((OUTPUT_FRAME_COUNT, height, FRAME_WIDTH, 4), dtype=np.float32)
    for output_index, position in enumerate(
        np.linspace(0.0, SOURCE_FRAME_COUNT - 1, OUTPUT_FRAME_COUNT)
    ):
        lower = int(np.floor(position))
        upper = min(lower + 1, SOURCE_FRAME_COUNT - 1)
        mix = float(position - lower)
        a = frames[lower]
        if lower == upper or mix == 0.0:
            output[output_index] = a
            continue

        b = frames[upper]
        alpha = a[..., 3:4] * (1.0 - mix) + b[..., 3:4] * mix
        premultiplied = (
            a[..., :3] * a[..., 3:4] * (1.0 - mix)
            + b[..., :3] * b[..., 3:4] * mix
        )
        rgb = np.divide(
            premultiplied,
            np.maximum(alpha, 1.0 / 255.0),
            out=np.zeros_like(premultiplied),
            where=alpha > 1.0 / 255.0,
        )
        output[output_index] = np.concatenate((rgb, alpha), axis=2)

    if not np.array_equal(output[0], frames[0]) or not np.array_equal(output[-1], frames[-1]):
        raise RuntimeError("129-state retime changed a selected-wheel endpoint")
    return output.transpose(1, 0, 2, 3).reshape(height, OUTPUT_FRAME_COUNT * FRAME_WIDTH, 4)

def main() -> int:
    body_hex = sys.argv[1] if len(sys.argv) > 1 else "#D9EF8A"
    tint_body = np.array(hex_to_rgb(body_hex), dtype=np.float32)
    tint_core = np.array(hex_to_rgb(sys.argv[2]) if len(sys.argv) > 2
                         else hot_core(tuple(tint_body)), dtype=np.float32)
    emission_gain = float(sys.argv[3]) if len(sys.argv) > 3 else GAIN
    target = Path(sys.argv[4]) if len(sys.argv) > 4 else TARGET
    narrow = float(sys.argv[5]) if len(sys.argv) > 5 else NARROW
    source = Path(sys.argv[6]) if len(sys.argv) > 6 else SOURCE
    body_source = Path(sys.argv[7]) if len(sys.argv) > 7 else source
    body_gamma = float(sys.argv[8]) if len(sys.argv) > 8 else BODY_GAMMA
    body_sharpen = int(sys.argv[9]) if len(sys.argv) > 9 else 0

    src = np.asarray(Image.open(source).convert("RGBA"), dtype=np.float32) / 255.0
    rgb = src[..., :3]
    body_src = np.asarray(Image.open(body_source).convert("RGBA"), dtype=np.float32) / 255.0
    if body_src.shape != src.shape:
        raise SystemExit(f"{body_source.name} shape {body_src.shape} does not match {source.name} {src.shape}.")
    src = retime_frames(src)
    body_src = src if body_source.resolve() == source.resolve() else retime_frames(body_src)
    rgb = src[..., :3]
    alpha = body_src[..., 3:4]

    red = rgb[..., 0]
    den = np.maximum(1.0 - red, 1.0 / 255.0)
    green = np.clip((rgb[..., 1] - red) / den, 0.0, 1.0)
    blue = np.clip((rgb[..., 2] - red) / den, 0.0, 1.0)
    field = np.maximum(green, blue)
    gate = np.clip((field - 0.018) / 0.055, 0.0, 1.0)
    field = field * (gate * gate * (3.0 - 2.0 * gate))

    body_red = body_src[..., 0]
    if body_sharpen > 0:
        body_red = np.asarray(
            Image.fromarray(np.clip(body_red * 255.0 + 0.5, 0, 255).astype(np.uint8), "L")
                 .filter(ImageFilter.UnsharpMask(radius=1.6, percent=body_sharpen, threshold=2)),
            dtype=np.float32,
        ) / 255.0
    body = np.dstack([body_red, body_red, body_red])
    body = np.clip((body ** body_gamma) * BODY_TINT, 0.0, 1.0)
    field = np.clip(field, 0.0, 1.0) ** narrow

    frames = field.shape[1] // 417
    for i in range(1, frames):
        a = field[:, i * 417:(i + 1) * 417]
        b = field[:, (i - 1) * 417:i * 417]
        np.maximum(a, b, out=a)

    lum = body.mean(axis=2, keepdims=True)
    openings = np.clip(1.0 - (lum - 0.05) * ABSORB, 0.0, 1.0)

    f = field[..., None]
    core = np.clip((f - 0.82) / 0.18, 0.0, 1.0) ** 2.0
    tint = tint_body + (tint_core - tint_body) * core
    glow = np.clip(f * emission_gain * tint * openings, 0.0, 1.0)
    out = 1.0 - (1.0 - body) * (1.0 - glow)

    res = np.dstack([np.clip(out, 0.0, 1.0), alpha])
    target.parent.mkdir(parents=True, exist_ok=True)
    pending = target.with_name(target.stem + ".next" + target.suffix)
    Image.fromarray((res * 255.0 + 0.5).astype(np.uint8), "RGBA").save(pending)
    pending.replace(target)
    print(f"baked wheel lamp {body_hex} (core "
          f"#{''.join('%02X' % round(c * 255) for c in tint_core)}) "
          f"-> {target.name}, {res.shape[1] // 417} frames, emission {emission_gain:.2f}, "
          f"focus {narrow:.2f}, emission {source.name}, body {body_source.name}, "
          f"gamma {body_gamma:.2f}, sharpen {body_sharpen}%")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
