
from __future__ import annotations

import hashlib
import json
import os
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

FRAME_COUNT = 129
LAST_FRAME = FRAME_COUNT - 1
CELL = (417, 93)
RAW_SIZE = (1200, 360)
FIXED_CROP = (25, 42, 1175, 318)
CHANNEL_BAND = (33, 61)
GATE_CENTER, GATE_SIGMA = 46.5, 7.17

REPO = Path(__file__).resolve().parents[1]
RENDER_ROOT = REPO / "dev" / "tmp" / "thumbwheel_blender" / "wheel_129"
WHEEL_DIR = RENDER_ROOT / "wheel_pass"
TRANSMISSION_DIR = RENDER_ROOT / "transmission_pass"
ASSET = Path(os.environ.get(
    "TRENCH_WHEEL_ASSET",
    str(REPO / "plugin" / "assets" / "trench_roller_strip.png"),
))
BACKUP = ASSET.with_name("trench_roller_strip.png.bak_pre_navy129_20260809")
X3_STRIP = Path(r"C:\Users\hooki\do-it\emu-x3-bitmap-dump\BITMAP4331_2.bmp")

def _rgba_float(image: Image.Image) -> np.ndarray:
    return np.asarray(image.convert("RGBA"), dtype=np.float32) / 255.0

def _resize_premultiplied(image: Image.Image, size) -> np.ndarray:
    rgba = _rgba_float(image)
    alpha = rgba[..., 3:4]
    packed = np.concatenate([rgba[..., :3] * alpha, alpha], axis=-1)
    packed_u8 = np.clip(np.rint(packed * 255.0), 0.0, 255.0).astype(np.uint8)
    resized = Image.fromarray(packed_u8, "RGBA").resize(size, Image.Resampling.LANCZOS)
    out = _rgba_float(resized)
    out_alpha = out[..., 3:4]
    out_rgb = np.divide(
        out[..., :3],
        np.maximum(out_alpha, 1.0 / 255.0),
        out=np.zeros_like(out[..., :3]),
        where=out_alpha > 1.0 / 255.0,
    )
    return np.concatenate([np.clip(out_rgb, 0.0, 1.0), out_alpha], axis=-1)

def _load_frame(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        if image.size != RAW_SIZE:
            raise RuntimeError(f"Unexpected raw render size {image.size} at {path}")
        return _resize_premultiplied(image.crop(FIXED_CROP), CELL)

def _load_stack(folder: Path) -> np.ndarray:
    frames = []
    for frame in range(FRAME_COUNT):
        path = folder / f"frame_{frame:03d}.png"
        if not path.exists():
            raise FileNotFoundError(path)
        frames.append(_load_frame(path))
    return np.stack(frames, axis=0)

def _luma(rgb: np.ndarray) -> np.ndarray:
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722

def _lateral_correction(body_frame0: np.ndarray) -> np.ndarray:
    x3 = np.asarray(Image.open(X3_STRIP).convert("RGB"), dtype=np.float32)
    x3_frame = x3[:, 0:85, :]
    x3_env = _luma(x3_frame / 255.0).mean(axis=0)
    x3_env = np.convolve(x3_env, np.ones(5) / 5.0, mode="same")

    alpha = body_frame0[..., 3]
    weight = np.maximum(alpha.sum(axis=0), 1e-3)
    ours_env = (_luma(body_frame0[..., :3]) * alpha).sum(axis=0) / weight
    ours_env = np.convolve(ours_env, np.ones(15) / 15.0, mode="same")

    x3_at_ours = np.interp(np.linspace(0, 84, CELL[0]), np.arange(85), x3_env)
    c0, c1 = int(CELL[0] * 0.35), int(CELL[0] * 0.65)
    x3_norm = x3_at_ours / max(1e-6, x3_at_ours[c0:c1].mean())
    ours_norm = ours_env / max(1e-6, ours_env[c0:c1].mean())
    ratio = np.clip(x3_norm / np.maximum(ours_norm, 1e-6), 0.0, 1.0)
    return ratio.astype(np.float32)

def _transmission_energy(delta: np.ndarray) -> np.ndarray:
    return np.clip(np.max(np.clip(delta, 0.0, 1.0), axis=-1), 0.0, 1.0)

def _compose_frame(wheel, transmission_delta, core, accent, lateral):
    height, width = wheel.shape[:2]
    yy = np.arange(height, dtype=np.float32)[:, None]
    vertical = np.exp(-(((yy - GATE_CENTER) / GATE_SIGMA) ** 8))

    energy = _transmission_energy(transmission_delta)
    dilation_x = np.arange(-9, 10, dtype=np.float32)
    dilation_kernel = np.exp(-((dilation_x / 3.2) ** 2))
    dilation_kernel /= np.sum(dilation_kernel)
    channel_energy = energy[CHANNEL_BAND[0]: CHANNEL_BAND[1], :]
    measured_gate = np.stack(
        [np.convolve(row, dilation_kernel, mode="same") for row in channel_energy],
        axis=0,
    )
    measured_gate = np.clip(measured_gate * 1.4, 0.0, 1.0)
    vertical_radius = 3
    padded = np.pad(measured_gate, ((vertical_radius, vertical_radius), (0, 0)), mode="constant")
    measured_gate = np.maximum.reduce(
        [padded[o: o + measured_gate.shape[0], :] for o in range(vertical_radius * 2 + 1)]
    )
    occluder_gate = np.zeros_like(energy)
    occluder_gate[CHANNEL_BAND[0]: CHANNEL_BAND[1], :] = measured_gate
    strength = np.clip(vertical * occluder_gate, 0.0, 1.0)

    peak = accent / float(np.max(accent))
    dim = peak * 0.40
    mid_t = np.clip((strength - 0.12) / 0.50, 0.0, 1.0)
    mid_t = mid_t * mid_t * (3.0 - 2.0 * mid_t)
    lamp_color = dim + (peak - dim) * mid_t[..., None]
    hot_t = np.clip((strength - 0.55) / 0.35, 0.0, 1.0)
    hot_t = hot_t * hot_t * (3.0 - 2.0 * hot_t)
    lamp_color = lamp_color + (core - peak) * hot_t[..., None]
    lamp_add = lamp_color * strength[..., None]

    base_rgb = np.clip(wheel[..., :3] * lateral[None, :, None], 0.0, 1.0)
    base_alpha = np.clip(wheel[..., 3:4], 0.0, 1.0)
    composite_rgb = base_rgb + (1.0 - base_rgb) * lamp_add
    return np.concatenate([np.clip(composite_rgb, 0.0, 1.0), base_alpha], axis=-1)

def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

def main() -> None:
    manifest_path = RENDER_ROOT / "render_manifest.json"
    render_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if render_manifest.get("frame_count") != FRAME_COUNT:
        raise RuntimeError("Render manifest does not contain exactly 129 frames")
    if render_manifest.get("fixed_crop") != list(FIXED_CROP):
        raise RuntimeError("Render manifest crop differs from the fixed aperture contract")

    accent_hex = os.environ.get(
        "TRENCH_WHEEL_ACCENT",
        render_manifest["transmission_light"]["accent_srgb_hex"],
    ).lstrip("#")
    core_hex = os.environ.get(
        "TRENCH_WHEEL_CORE",
        render_manifest["transmission_light"]["core_srgb_hex"],
    ).lstrip("#")
    accent = np.array(
        [int(accent_hex[i: i + 2], 16) / 255.0 for i in (0, 2, 4)], dtype=np.float32
    )
    core = np.array(
        [int(core_hex[i: i + 2], 16) / 255.0 for i in (0, 2, 4)], dtype=np.float32
    )

    wheel = _load_stack(WHEEL_DIR)
    transmission = _load_stack(TRANSMISSION_DIR)
    transmission_delta = np.maximum(transmission - wheel, 0.0)
    if float(np.max(transmission_delta)) < 2.0 / 255.0:
        raise RuntimeError("Transmission pass has no measurable lamp delta")

    lateral = _lateral_correction(wheel[0])
    print(f"[wheel-129] lateral correction: centre {lateral[100:317].mean():.3f}, "
          f"caps {lateral[:30].mean():.3f}/{lateral[-30:].mean():.3f}")

    atlas = np.zeros((CELL[1], CELL[0] * FRAME_COUNT, 4), dtype=np.uint8)
    first = last = None
    for frame in range(FRAME_COUNT):
        composite = _compose_frame(wheel[frame], transmission_delta[frame], core, accent, lateral)
        cell = np.clip(np.rint(composite * 255.0), 0.0, 255.0).astype(np.uint8)
        atlas[:, frame * CELL[0]: (frame + 1) * CELL[0], :] = cell
        if frame == 0:
            first = cell
        if frame == LAST_FRAME:
            last = cell
        if frame % 16 == 0 or frame == LAST_FRAME:
            print(f"[wheel-129] composed {frame}/{LAST_FRAME}")

    if np.array_equal(first, last):
        raise RuntimeError("Frame 128 is identical to frame 0; the glow endpoint is missing")

    if ASSET.exists() and not BACKUP.exists():
        shutil.copy2(ASSET, BACKUP)
        print(f"[wheel-129] backed up shipping strip -> {BACKUP.name}")

    ASSET.parent.mkdir(parents=True, exist_ok=True)
    pending = ASSET.with_name(f"{ASSET.stem}.next{ASSET.suffix}")
    Image.fromarray(atlas, "RGBA").save(pending)
    pending.replace(ASSET)

    asset_manifest = {
        "source_render_manifest": str(manifest_path),
        "source_blend": render_manifest["source_blend"],
        "frame_count": FRAME_COUNT,
        "cell_size": list(CELL),
        "atlas_size": [int(atlas.shape[1]), int(atlas.shape[0])],
        "channel_band": list(CHANNEL_BAND),
        "gate": [GATE_CENTER, GATE_SIGMA],
        "lamp_accent_srgb_hex": accent_hex,
        "lamp_core_srgb_hex": core_hex,
        "lateral_correction": "X3-over-ours frame-0 column envelope ratio, clamped <=1, body only",
        "loop": "one tooth pitch, frames 0/128 congruent body poses",
        "asset": str(ASSET),
        "asset_sha256": _sha256(ASSET),
    }
    (RENDER_ROOT / "atlas_manifest.json").write_text(
        json.dumps(asset_manifest, indent=2), encoding="utf-8"
    )
    print(json.dumps({"asset": str(ASSET), "size": [int(atlas.shape[1]), int(atlas.shape[0])]}))

if __name__ == "__main__":
    main()
