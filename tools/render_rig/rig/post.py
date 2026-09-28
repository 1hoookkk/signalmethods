import bpy
import numpy as np


def read(path):
    img = bpy.data.images.load(path, check_existing=False)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    bpy.data.images.remove(img)
    return px.reshape(h, w, 4)[::-1].copy()


def write(path, rgba):
    h, w = rgba.shape[:2]
    img = bpy.data.images.new("rig_out", w, h, alpha=True)
    img.pixels.foreach_set(np.ascontiguousarray(np.clip(rgba[::-1], 0.0, 1.0)).ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def _blur(a, radius):
    sigma = max(radius / 2.0, 0.5)
    x = np.arange(-int(3 * sigma), int(3 * sigma) + 1)
    k = np.exp(-0.5 * (x / sigma) ** 2)
    k /= k.sum()
    a = np.apply_along_axis(lambda r: np.convolve(r, k, mode="same"), 1, a)
    return np.apply_along_axis(lambda c: np.convolve(c, k, mode="same"), 0, a)


def glow(rgba, colour_srgb, radius_px, strength, threshold=0.55):
    rgb = rgba[..., :3]
    lum = rgb.max(axis=2) * rgba[..., 3]
    chroma = rgb.max(axis=2) - rgb.min(axis=2)
    source = np.clip((lum - threshold) / (1.0 - threshold), 0.0, 1.0) * np.clip((chroma - 0.25) / 0.35, 0.0, 1.0)
    halo = np.clip(_blur(source, radius_px) * strength, 0.0, 1.0)
    col = np.asarray(colour_srgb, dtype=np.float32)
    a = rgba[..., 3:4]
    premul = rgb * a
    light = col * halo[..., None]
    out_premul = premul + light * (1.0 - premul)
    alpha = np.clip(rgba[..., 3] + halo * (1.0 - rgba[..., 3]), 0.0, 1.0)
    out = rgba.copy()
    out[..., :3] = np.where(alpha[..., None] > 1e-6, out_premul / np.maximum(alpha[..., None], 1e-6), 0.0)
    out[..., 3] = alpha
    return out


def downsample(rgba, factor):
    if factor <= 1:
        return rgba
    h, w = rgba.shape[0] // factor, rgba.shape[1] // factor
    blocks = rgba[: h * factor, : w * factor].reshape(h, factor, w, factor, 4)
    a = blocks[..., 3].mean(axis=(1, 3))
    rgb = (blocks[..., :3] * blocks[..., 3:4]).mean(axis=(1, 3))
    out = np.zeros((h, w, 4), dtype=np.float32)
    out[..., :3] = np.where(a[..., None] > 1e-6, rgb / np.maximum(a[..., None], 1e-6), 0.0)
    out[..., 3] = a
    return out


def strip(frames):
    return np.concatenate(frames, axis=1)
