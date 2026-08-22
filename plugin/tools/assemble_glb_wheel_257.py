
from __future__ import annotations

import hashlib
import json
import math
import os
import shutil
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

FRAME_COUNT = 257
LAST_FRAME = FRAME_COUNT - 1
FINAL_SIZE = (417, 93)
SHIPPED_FRAME_SIZE = FINAL_SIZE
SHIPPED_FRAME_COUNT = 128
RAW_SIZE = (1200, 360)
FIXED_CROP = (25, 42, 1175, 318)
CHANNEL_BAND = (33, 61)
GATE_CENTER = 46.5
GATE_SIGMA = 7.17
AUTHORED_PROGRESS_FRAMES = 255.0
DIODE_RISE_FRAMES = 5.0

REPO = Path(__file__).resolve().parents[1]
RENDER_ROOT = REPO / "dev" / "tmp" / "thumbwheel_blender" / "glb_wheel_257"
WHEEL_DIR = RENDER_ROOT / "wheel_pass"
TRANSMISSION_DIR = RENDER_ROOT / "transmission_pass"
COMPOSITE_DIR = RENDER_ROOT / "composite_pass"
GLOW_DIR = RENDER_ROOT / "glow_pass"
ASSET = Path(os.environ.get(
    "TRENCH_WHEEL_ASSET",
    str(REPO / "plugin" / "assets" / "trench_roller_strip.png"),
))

def _rgba_float(image: Image.Image) -> np.ndarray:
    return np.asarray(image.convert("RGBA"), dtype=np.float32) / 255.0

def _resize_premultiplied(image: Image.Image, size: tuple[int, int] | None = None) -> np.ndarray:
    size = FINAL_SIZE if size is None else size
    rgba = _rgba_float(image)
    alpha = rgba[..., 3:4]
    premult = rgba[..., :3] * alpha
    packed = np.concatenate([premult, alpha], axis=-1)
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
        cropped = image.crop(FIXED_CROP)
        return _resize_premultiplied(cropped)

def _load_stack(folder: Path) -> np.ndarray:
    frames = []
    for frame in range(FRAME_COUNT):
        path = folder / f"frame_{frame:03d}.png"
        if not path.exists():
            raise FileNotFoundError(path)
        frames.append(_load_frame(path))
    return np.stack(frames, axis=0)

def _save_rgba(path: Path, rgba: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    u8 = np.clip(np.rint(rgba * 255.0), 0.0, 255.0).astype(np.uint8)
    Image.fromarray(u8, "RGBA").save(path)

def _save_rgb(path: Path, rgb: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    u8 = np.clip(np.rint(rgb * 255.0), 0.0, 255.0).astype(np.uint8)
    Image.fromarray(u8, "RGB").save(path)

def _smoothstep(edge0: float, edge1: float, value: float) -> float:
    if edge0 == edge1:
        return 1.0 if value >= edge1 else 0.0
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)

def _detect_diode_centers(signal: np.ndarray, x0: int, x1: int) -> np.ndarray:
    span = max(1, x1 - x0)
    region = signal[x0:x1]
    if len(region) < 16:
        raise RuntimeError(f"Visible tooth region is too short: {len(region)} pixels")
    smoothed = np.convolve(region, np.ones(5, dtype=np.float32) / 5.0, mode="same")
    trend = np.convolve(smoothed, np.ones(15, dtype=np.float32) / 15.0, mode="same")
    detrended = smoothed - trend
    lo, hi = np.percentile(detrended, [10, 90])
    norm = np.clip((detrended - lo) / max(1e-5, hi - lo), 0.0, 1.0)
    peaks: list[int] = []
    min_dist = max(5, int(round(span / 22.0)))
    for index in range(2, len(norm) - 2):
        if norm[index] < 0.38:
            continue
        if norm[index] < norm[index - 1] or norm[index] < norm[index + 1]:
            continue
        if norm[index] < norm[index - 2] or norm[index] < norm[index + 2]:
            continue
        if not peaks or index - peaks[-1] >= min_dist:
            peaks.append(index)
        elif norm[index] > norm[peaks[-1]]:
            peaks[-1] = index

    centers = np.asarray(
        [float(index + x0) for index in peaks if index > 5 and index < span - 5],
        dtype=np.float32,
    )
    if len(centers) < 8 or len(centers) > 20:
        raise RuntimeError(
            f"Measured visible tooth centers are not credible: count={len(centers)}, centers={centers.tolist()}"
        )
    spacing = np.diff(centers)
    if len(spacing) < 2 or float(np.std(spacing)) < 0.8:
        raise RuntimeError(
            "Measured centers are too evenly spaced; refusing to replace real geometry with dots: "
            f"centers={centers.tolist()}"
        )
    return centers

def _bead_progress_profile(value: float, span: int, diode_centers: np.ndarray) -> np.ndarray:
    if value <= 0.002:
        return np.zeros(span, dtype=np.float32)
    if len(diode_centers) < 2:
        raise RuntimeError("Progression requires at least two measured visible centers")

    authored_frame = max(0.0, min(1.0, value)) * AUTHORED_PROGRESS_FRAMES
    diode_count = len(diode_centers)
    last_start = max(0.0, AUTHORED_PROGRESS_FRAMES - DIODE_RISE_FRAMES)
    start_frames = np.linspace(0.0, last_start, diode_count, dtype=np.float32)
    xs = np.arange(span, dtype=np.float32)
    spacing = np.diff(diode_centers)
    median_spacing = float(np.median(spacing)) if len(spacing) else span / 13.0
    bead_sigma = max(1.45, median_spacing * 0.22)
    profile = np.zeros(span, dtype=np.float32)

    for start, center in zip(start_frames, diode_centers):
        ramp = _smoothstep(float(start), float(start + DIODE_RISE_FRAMES), authored_frame)
        if ramp <= 0.001:
            continue
        age = max(0.0, authored_frame - float(start + DIODE_RISE_FRAMES))
        trail = float(np.exp(-age / 40.0))
        hold = 0.08 + 0.92 * trail
        amplitude = ramp * hold
        width_scale = 1.0 + 1.6 * (age / AUTHORED_PROGRESS_FRAMES)
        current_sigma = bead_sigma * width_scale
        profile += amplitude * np.exp(-((xs - center) / current_sigma) ** 2)

    lead_x = float(
        np.interp(
            authored_frame,
            start_frames + DIODE_RISE_FRAMES * 0.5,
            diode_centers,
            left=diode_centers[0],
            right=diode_centers[-1],
        )
    )
    lead_sigma = bead_sigma * 2.5
    lead = np.exp(-((xs - lead_x) / lead_sigma) ** 2).astype(np.float32)
    profile = profile * 0.74 + lead * 0.26
    profile = np.convolve(
        profile,
        np.array([0.05, 0.16, 0.58, 0.16, 0.05], dtype=np.float32),
        mode="same",
    )
    return np.clip(profile, 0.0, 1.0)

def _transmission_energy(delta: np.ndarray) -> np.ndarray:
    return np.clip(np.max(np.clip(delta, 0.0, 1.0), axis=-1), 0.0, 1.0)

def _slot_signal(wheel: np.ndarray, transmission_delta: np.ndarray) -> tuple[np.ndarray, str]:
    top, bottom = CHANNEL_BAND
    wheel_rgb = wheel[..., :3]
    luma = wheel_rgb[..., 0] * 0.2126 + wheel_rgb[..., 1] * 0.7152 + wheel_rgb[..., 2] * 0.0722
    wheel_signal = luma[FRAME_COUNT // 2, top:bottom, :].mean(axis=0).astype(np.float32)
    delta = transmission_delta[FRAME_COUNT // 2, top:bottom, :]
    transmission_signal = _transmission_energy(delta).mean(axis=0).astype(np.float32)
    for signal, name in ((wheel_signal, "wheel_luma"), (transmission_signal, "transmission_delta")):
        try:
            return _detect_diode_centers(signal, 4, FINAL_SIZE[0] - 4), name
        except RuntimeError as error:
            print(f"[glb-wheel] center candidate {name} rejected: {error}")
    raise RuntimeError("No credible irregular visible tooth/section centers were detected")

def _profile_debug(path: Path, signal: np.ndarray, centers: np.ndarray) -> None:
    width, height = 400, 160
    canvas = np.zeros((height, width, 3), dtype=np.uint8)
    scaled = signal - float(np.min(signal))
    scaled /= max(1e-6, float(np.max(scaled)))
    for x in range(min(width, len(signal))):
        y = height - 1 - int(round(float(scaled[x]) * (height - 20)))
        canvas[max(0, y - 1) : min(height, y + 2), x] = (120, 220, 220)
    for center in centers:
        x = int(round(float(center)))
        if 0 <= x < width:
            canvas[:, max(0, x - 1) : min(width, x + 2)] = (255, 160, 40)
    _save_rgb(path, canvas.astype(np.float32) / 255.0)

def _compose_frame(
    wheel: np.ndarray,
    transmission_delta: np.ndarray,
    value: float,
    centers: np.ndarray,
    accent: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    height, width = wheel.shape[:2]
    yy = np.arange(height, dtype=np.float32)[:, None]
    vertical = np.exp(-((yy - GATE_CENTER) / GATE_SIGMA) ** 8)

    energy = _transmission_energy(transmission_delta)
    dilation_x = np.arange(-9, 10, dtype=np.float32)
    dilation_kernel = np.exp(-((dilation_x / 3.2) ** 2))
    dilation_kernel /= np.sum(dilation_kernel)
    channel_energy = energy[CHANNEL_BAND[0] : CHANNEL_BAND[1], :]
    measured_gate = np.stack(
        [np.convolve(row, dilation_kernel, mode="same") for row in channel_energy],
        axis=0,
    )
    measured_gate = np.clip(measured_gate * 4.6, 0.0, 1.0)
    vertical_radius = 3
    padded_gate = np.pad(
        measured_gate,
        ((vertical_radius, vertical_radius), (0, 0)),
        mode="constant",
    )
    measured_gate = np.maximum.reduce(
        [padded_gate[offset : offset + measured_gate.shape[0], :]
         for offset in range(vertical_radius * 2 + 1)]
    )
    occluder_gate = np.zeros_like(energy)
    occluder_gate[CHANNEL_BAND[0] : CHANNEL_BAND[1], :] = measured_gate
    strength = np.clip(vertical * occluder_gate, 0.0, 1.0)

    peak = accent / float(np.max(accent))
    dim = peak * 0.40
    mid_t = np.clip((strength - 0.12) / 0.50, 0.0, 1.0)
    mid_t = mid_t * mid_t * (3.0 - 2.0 * mid_t)
    lamp_color = dim + (peak - dim) * mid_t[..., None]
    lamp_add = lamp_color * strength[..., None]

    base_rgb = np.clip(wheel[..., :3], 0.0, 1.0)
    base_alpha = np.clip(wheel[..., 3:4], 0.0, 1.0)
    composite_rgb = base_rgb + (1.0 - base_rgb) * lamp_add
    composite = np.concatenate([np.clip(composite_rgb, 0.0, 1.0), base_alpha], axis=-1)

    glow_alpha = np.clip(strength[..., None] * base_alpha * 0.92, 0.0, 1.0)
    glow_rgb = np.clip(lamp_color * strength[..., None], 0.0, 1.0)
    glow = np.concatenate([glow_rgb, glow_alpha], axis=-1)
    return composite, glow

def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()

def _write_contact_sheet(frames: dict[int, np.ndarray], degrees: dict[int, float]) -> None:
    selected = list(frames)
    cell_w, cell_h = FINAL_SIZE
    columns = 3
    rows = int(math.ceil(len(selected) / columns))
    sheet = Image.new("RGBA", (cell_w * columns, cell_h * rows), (12, 14, 16, 255))
    draw = ImageDraw.Draw(sheet)
    for index, frame in enumerate(selected):
        x = (index % columns) * cell_w
        y = (index // columns) * cell_h
        image = Image.fromarray(
            np.clip(np.rint(frames[frame] * 255.0), 0.0, 255.0).astype(np.uint8), "RGBA"
        )
        sheet.alpha_composite(image, (x, y))
        degree = degrees.get(frame)
        label = f"{frame}" if degree is None else f"{frame} / {degree:.1f}°"
        draw.text((x + 6, y + 4), label, fill=(235, 245, 245, 255))
    sheet.save(RENDER_ROOT / "glow_contact_sheet.png")

def _write_arc_proof(frames: dict[int, np.ndarray], degrees: dict[int, float]) -> None:
    cell_w, cell_h = FINAL_SIZE
    proof = Image.new("RGBA", (cell_w * 3, cell_h), (12, 14, 16, 255))
    draw = ImageDraw.Draw(proof)
    for index, frame in enumerate((0, 128, 256)):
        rgba = Image.fromarray(
            np.clip(np.rint(frames[frame] * 255.0), 0.0, 255.0).astype(np.uint8), "RGBA"
        )
        proof.alpha_composite(rgba, (index * cell_w, 0))
        draw.text(
            (index * cell_w + 6, 4),
            f"frame {frame} / {degrees[frame]:.1f}°",
            fill=(235, 245, 245, 255),
        )
    proof.save(RENDER_ROOT / "arc_proof_start_mid_end.png")

def main() -> None:
    manifest_path = RENDER_ROOT / "render_manifest.json"
    if not manifest_path.exists():
        raise FileNotFoundError(manifest_path)
    render_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if render_manifest.get("frame_count") != FRAME_COUNT:
        raise RuntimeError("Render manifest does not contain exactly 257 frames")
    if render_manifest.get("fixed_crop") != list(FIXED_CROP):
        raise RuntimeError("Render manifest crop differs from the fixed aperture contract")

    wheel = _load_stack(WHEEL_DIR)
    transmission = _load_stack(TRANSMISSION_DIR)
    if wheel.shape != (FRAME_COUNT, FINAL_SIZE[1], FINAL_SIZE[0], 4):
        raise RuntimeError(f"Unexpected wheel stack shape {wheel.shape}")
    transmission_delta = np.maximum(transmission - wheel, 0.0)
    if float(np.max(transmission_delta)) < 2.0 / 255.0:
        raise RuntimeError("Transmission pass has no measurable lamp delta")

    transmission_manifest = render_manifest.get("transmission_light", {})
    accent_hex = os.environ.get(
        "TRENCH_WHEEL_ACCENT",
        transmission_manifest.get("accent_srgb_hex", ""),
    ).lstrip("#")
    if not accent_hex:
        raise RuntimeError(
            "Render manifest carries no accent colour; re-run tools/render_glb_wheel_257.py"
        )
    accent = np.array(
        [int(accent_hex[i : i + 2], 16) / 255.0 for i in (0, 2, 4)], dtype=np.float32
    )
    centers = np.asarray(transmission_manifest.get("screen_centers", []), dtype=np.float32)
    centers *= FINAL_SIZE[0] / 400.0
    if len(centers) < 8 or len(centers) > 20:
        raise RuntimeError(f"Render manifest has no credible calibrated transmission centers: {centers.tolist()}")
    spacing = np.diff(centers)
    if len(spacing) < 2 or float(np.std(spacing)) < 0.8:
        raise RuntimeError(f"Calibrated transmission centers are implausibly uniform: {centers.tolist()}")
    center_source = transmission_manifest.get(
        "center_source", "calibrated physical transmission bank"
    )
    _profile_debug(RENDER_ROOT / "measured_centers.png", wheel[FRAME_COUNT // 2, CHANNEL_BAND[0] : CHANNEL_BAND[1], ..., 0].mean(axis=0), centers)
    print(f"[glb-wheel] measured {len(centers)} irregular centers from {center_source}: {centers.tolist()}")

    if ASSET.exists():
        backup = RENDER_ROOT / "rejected_procedural_strip_backup.png"
        if not backup.exists():
            shutil.copy2(ASSET, backup)

    COMPOSITE_DIR.mkdir(parents=True, exist_ok=True)
    GLOW_DIR.mkdir(parents=True, exist_ok=True)
    composite_frames: dict[int, np.ndarray] = {}
    glow_frames: dict[int, np.ndarray] = {}
    atlas_parts = []
    for frame in range(FRAME_COUNT):
        composite, glow = _compose_frame(
            wheel[frame], transmission_delta[frame], frame / float(LAST_FRAME), centers, accent
        )
        composite_frames[frame] = composite
        glow_frames[frame] = glow
        _save_rgba(COMPOSITE_DIR / f"frame_{frame:03d}.png", composite)
        _save_rgba(GLOW_DIR / f"frame_{frame:03d}.png", glow)
        atlas_parts.append(np.clip(np.rint(composite * 255.0), 0.0, 255.0).astype(np.uint8))
        if frame % 32 == 0 or frame == LAST_FRAME:
            print(f"[glb-wheel] composed frame {frame}/{LAST_FRAME}")

    if np.array_equal(atlas_parts[0], atlas_parts[-1]):
        raise RuntimeError("Frame 256 is identical to frame 0; endpoint is not authored")

    selected = [
        int(round(index))
        for index in np.linspace(0, LAST_FRAME, SHIPPED_FRAME_COUNT)
    ]
    atlas = np.zeros(
        (SHIPPED_FRAME_SIZE[1], SHIPPED_FRAME_SIZE[0] * SHIPPED_FRAME_COUNT, 4),
        dtype=np.uint8,
    )
    for column, source_frame in enumerate(selected):
        cell = atlas_parts[source_frame].astype(np.float32) / 255.0
        x = column * SHIPPED_FRAME_SIZE[0]
        atlas[:, x : x + SHIPPED_FRAME_SIZE[0], :] = np.clip(
            np.rint(cell * 255.0), 0.0, 255.0
        ).astype(np.uint8)
    ASSET.parent.mkdir(parents=True, exist_ok=True)
    pending_asset = ASSET.with_name(f"{ASSET.stem}.next{ASSET.suffix}")
    Image.fromarray(atlas, "RGBA").save(pending_asset)
    pending_asset.replace(ASSET)

    degrees_raw = render_manifest.get("opposite_degrees", {})
    degrees = {int(key): float(value) for key, value in degrees_raw.items()}
    for frame in (0, 128, 256):
        degrees.setdefault(frame, float(frame))
    for frame in (0, 128, 256):
        _save_rgba(RENDER_ROOT / f"proof_composite_{frame:03d}.png", composite_frames[frame])
        _save_rgba(RENDER_ROOT / f"proof_glow_{frame:03d}.png", glow_frames[frame])
    _write_contact_sheet({frame: composite_frames[frame] for frame in range(0, FRAME_COUNT, 32)}, degrees)
    _write_arc_proof(composite_frames, degrees)

    asset_manifest = {
        "source_render_manifest": str(manifest_path),
        "source_blend": render_manifest["source_blend"],
        "source_glb": render_manifest["source_glb"],
        "frame_count": FRAME_COUNT,
        "frame_values": list(range(FRAME_COUNT)),
        "raw_size": list(RAW_SIZE),
        "fixed_crop": list(FIXED_CROP),
        "final_frame_size": list(FINAL_SIZE),
        "shipped_frame_size": list(SHIPPED_FRAME_SIZE),
        "shipped_frame_count": SHIPPED_FRAME_COUNT,
        "shipped_source_frames": selected,
        "atlas_size": [int(atlas.shape[1]), int(atlas.shape[0])],
        "opposite_degrees": {str(key): value for key, value in sorted(degrees.items())},
        "center_source": center_source,
        "visible_irregular_centers": [float(value) for value in centers],
        "visible_center_spacing": [float(value) for value in np.diff(centers)],
        "lamp_accent_srgb_hex": accent_hex,
        "channel_band": list(CHANNEL_BAND),
        "authored_progress_frames": AUTHORED_PROGRESS_FRAMES,
        "diode_rise_frames": DIODE_RISE_FRAMES,
        "offscreen_centers_counted": False,
        "per_frame_normalization": False,
        "geometry_source": "WheelT from supplied GLB scene; no mesh edits",
        "excluded_objects": render_manifest["excluded_objects"],
        "asset": str(ASSET),
        "asset_sha256": _sha256(ASSET),
    }
    (RENDER_ROOT / "atlas_manifest.json").write_text(
        json.dumps(asset_manifest, indent=2), encoding="utf-8"
    )
    print(json.dumps({"asset": str(ASSET), "size": [int(atlas.shape[1]), int(atlas.shape[0])], "centers": centers.tolist()}))

if __name__ == "__main__":
    main()
