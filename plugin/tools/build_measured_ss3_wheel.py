
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

FRAME_COUNT = 129
FRAME_SIZE = (417, 93)
RAW_FRAME_COUNT = 257
RAW_SIZE = (1200, 360)
FIXED_CROP = (25, 42, 1175, 318)
EMU_FRAME_COUNT = 129
EMU_FRAME_SIZE = (85, 16)

def rgb(hex_value: str) -> np.ndarray:
    value = hex_value.strip().lstrip("#")
    if len(value) != 6:
        raise ValueError(f"expected RRGGBB, got {hex_value!r}")
    return np.array(
        [int(value[i:i + 2], 16) / 255.0 for i in (0, 2, 4)],
        dtype=np.float32,
    )

def resize_premultiplied(image: Image.Image, size: tuple[int, int]) -> np.ndarray:
    rgba = np.asarray(image.convert("RGBA"), dtype=np.float32) / 255.0
    alpha = rgba[..., 3:4]
    packed = np.concatenate((rgba[..., :3] * alpha, alpha), axis=-1)
    packed_image = Image.fromarray(
        np.clip(np.rint(packed * 255.0), 0.0, 255.0).astype(np.uint8),
        "RGBA",
    ).resize(size, Image.Resampling.LANCZOS)
    result = np.asarray(packed_image, dtype=np.float32) / 255.0
    result_alpha = result[..., 3:4]
    result_rgb = np.divide(
        result[..., :3],
        np.maximum(result_alpha, 1.0 / 255.0),
        out=np.zeros_like(result[..., :3]),
        where=result_alpha > 1.0 / 255.0,
    )
    return np.concatenate((np.clip(result_rgb, 0.0, 1.0), result_alpha), axis=-1)

def load_rotating_bodies(folder: Path) -> list[np.ndarray]:
    source_indices = [
        int(round(value))
        for value in np.linspace(0, RAW_FRAME_COUNT - 1, FRAME_COUNT)
    ]
    bodies: list[np.ndarray] = []
    for source_index in source_indices:
        path = folder / f"frame_{source_index:03d}.png"
        if not path.exists():
            raise FileNotFoundError(path)
        with Image.open(path) as image:
            if image.size != RAW_SIZE:
                raise RuntimeError(f"unexpected raw body size {image.size} at {path}")
            bodies.append(resize_premultiplied(image.crop(FIXED_CROP), FRAME_SIZE))
    return bodies

def solve_field(source_rgb: np.ndarray) -> np.ndarray:
    red = source_rgb[..., 0]
    denominator = np.maximum(1.0 - red, 1.0 / 255.0)
    green = np.clip((source_rgb[..., 1] - red) / denominator, 0.0, 1.0)
    blue = np.clip((source_rgb[..., 2] - red) / denominator, 0.0, 1.0)
    field = np.maximum(green, blue)
    gate = np.clip((field - 0.018) / 0.055, 0.0, 1.0)
    return field * gate * gate * (3.0 - 2.0 * gate)

def alpha_bbox(alpha: np.ndarray) -> tuple[int, int, int, int]:
    ys, xs = np.where(alpha > 2.0 / 255.0)
    if xs.size == 0:
        raise RuntimeError("clean SS3 body has no visible pixels")
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("body_frames", type=Path)
    parser.add_argument("emu_strip", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--accent", default="#FFC229")
    parser.add_argument("--core", default="#FFF1B5")
    parser.add_argument("--gain", type=float, default=2.60)
    args = parser.parse_args()

    bodies = load_rotating_bodies(args.body_frames)
    x0, y0, x1, y1 = alpha_bbox(bodies[0][..., 3])

    with Image.open(args.emu_strip) as source_image:
        source = np.asarray(source_image.convert("RGB"), dtype=np.float32) / 255.0
    expected = (EMU_FRAME_SIZE[1], EMU_FRAME_SIZE[0] * EMU_FRAME_COUNT, 3)
    if source.shape != expected:
        raise RuntimeError(f"unexpected E-mu strip shape {source.shape}, expected {expected}")

    lamp = rgb(args.accent)
    core = rgb(args.core)
    rows = np.arange(y0, y1, dtype=np.float32)
    rows = (rows - y0) / max(1, y1 - y0)
    vertical_gate = np.exp(-0.5 * ((rows - 0.38) / 0.14) ** 2)

    atlas = Image.new("RGBA", (FRAME_SIZE[0] * FRAME_COUNT, FRAME_SIZE[1]), (0, 0, 0, 0))
    for frame, body_rgba in enumerate(bodies):
        body_rgb = body_rgba[..., :3]
        alpha = body_rgba[..., 3:4]
        source_luma = body_rgb @ np.array((0.299, 0.587, 0.114), dtype=np.float32)
        shaped_luma = source_luma ** 1.20 + np.maximum(source_luma - 0.25, 0.0) * 0.30
        body_scale = np.divide(
            shaped_luma,
            np.maximum(source_luma, 1.0 / 255.0),
            out=np.ones_like(source_luma),
            where=source_luma > 1.0 / 255.0,
        )
        body_rgb = np.clip(body_rgb * body_scale[..., None], 0.0, 1.0)
        body_luma = body_rgb @ np.array((0.299, 0.587, 0.114), dtype=np.float32)
        fin_occlusion = 1.0 - np.clip(body_luma, 0.0, 1.0)

        emu_frame = int(round(frame * (EMU_FRAME_COUNT - 1) / (FRAME_COUNT - 1)))
        sx = emu_frame * EMU_FRAME_SIZE[0]
        field = solve_field(source[:, sx:sx + EMU_FRAME_SIZE[0], :])
        field_image = Image.fromarray(np.rint(field * 255.0).astype(np.uint8), "L")
        mapped = np.asarray(
            field_image.resize((x1 - x0, y1 - y0), Image.Resampling.NEAREST),
            dtype=np.float32,
        ) / 255.0
        mapped = np.clip((mapped - 0.06) / 0.94, 0.0, 1.0) ** 1.05

        strength = np.zeros(FRAME_SIZE[::-1], dtype=np.float32)
        strength[y0:y1, x0:x1] = mapped * vertical_gate[:, None]
        strength *= fin_occlusion * alpha[..., 0]
        strength *= args.gain
        peak = float(strength.max())
        if peak > 1.0:
            strength /= peak
        strength = np.clip(strength, 0.0, 1.0)

        core_mix = np.clip((strength - 0.42) / 0.34, 0.0, 1.0) ** 2.0
        colour = lamp + (core - lamp) * core_mix[..., None]
        glow = strength[..., None] * colour
        composed = 1.0 - (1.0 - body_rgb) * (1.0 - glow)
        cell = Image.fromarray(
            np.rint(np.dstack((np.clip(composed, 0.0, 1.0), alpha[..., 0])) * 255.0)
            .astype(np.uint8),
            "RGBA",
        )
        atlas.alpha_composite(cell, (frame * FRAME_SIZE[0], 0))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(args.output)
    print(
        f"[ss3] rotating render + measured {args.accent} lamp -> "
        f"{args.output} {atlas.size}"
    )

if __name__ == "__main__":
    main()
