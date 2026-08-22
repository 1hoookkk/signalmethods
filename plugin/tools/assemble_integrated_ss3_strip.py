
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

FRAME_COUNT = 129
RAW_SIZE = (1200, 360)
FIXED_CROP = (25, 42, 1175, 318)
CELL = (417, 93)

def resize_premultiplied(image: Image.Image) -> Image.Image:
    rgba = np.asarray(image.convert("RGBA"), dtype=np.float32) / 255.0
    alpha = rgba[..., 3:4]
    packed = np.concatenate([rgba[..., :3] * alpha, alpha], axis=-1)
    packed_image = Image.fromarray(
        np.clip(np.rint(packed * 255.0), 0.0, 255.0).astype(np.uint8), "RGBA"
    )
    resized = np.asarray(
        packed_image.resize(CELL, Image.Resampling.LANCZOS), dtype=np.float32
    ) / 255.0
    out_alpha = resized[..., 3:4]
    out_rgb = np.divide(
        resized[..., :3],
        np.maximum(out_alpha, 1.0 / 255.0),
        out=np.zeros_like(resized[..., :3]),
        where=out_alpha > 1.0 / 255.0,
    )
    return Image.fromarray(
        np.clip(np.rint(np.concatenate([out_rgb, out_alpha], axis=-1) * 255.0), 0, 255)
        .astype(np.uint8),
        "RGBA",
    )

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("sequence_dir", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--contact", type=Path)
    args = parser.parse_args()

    atlas = Image.new("RGBA", (CELL[0] * FRAME_COUNT, CELL[1]), (0, 0, 0, 0))
    cells: list[Image.Image] = []
    for index in range(FRAME_COUNT):
        path = args.sequence_dir / f"frame_{index:03d}.png"
        with Image.open(path) as source:
            if source.size != RAW_SIZE:
                raise RuntimeError(f"{path} is {source.size}, expected {RAW_SIZE}")
            cell = resize_premultiplied(source.crop(FIXED_CROP))
        atlas.alpha_composite(cell, (index * CELL[0], 0))
        cells.append(cell)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    pending = args.output.with_name(f"{args.output.stem}.next{args.output.suffix}")
    atlas.save(pending)
    pending.replace(args.output)

    if args.contact is not None:
        picks = (0, 26, 51, 77, 102, 128)
        scale = 2
        contact = Image.new("RGBA", (CELL[0] * scale, CELL[1] * scale * len(picks)), (0, 0, 0, 0))
        for row, index in enumerate(picks):
            contact.alpha_composite(
                cells[index].resize((CELL[0] * scale, CELL[1] * scale), Image.Resampling.NEAREST),
                (0, row * CELL[1] * scale),
            )
        args.contact.parent.mkdir(parents=True, exist_ok=True)
        contact.save(args.contact)

    print(f"[ss3] packed {FRAME_COUNT} integrated frames -> {args.output} ({atlas.size})")

if __name__ == "__main__":
    main()
