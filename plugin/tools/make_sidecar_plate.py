import numpy as np
from PIL import Image, ImageDraw, ImageFilter

W, H = 560, 1006
S = 2.0

WELLS = {
    "drive":  (12, 30, 256, 170),
    "motion": (12, 218, 256, 112),
    "key":    (12, 348, 256, 48),
    "source": (12, 414, 256, 80),
}

rng = np.random.default_rng(7)

def brushed(base=(168, 171, 175)):
    noise = rng.standard_normal((H, W)).astype(np.float32)
    img = Image.fromarray(((noise * 10) + 128).clip(0, 255).astype(np.uint8))
    img = img.filter(ImageFilter.GaussianBlur((0.4, 3.2)))
    n = np.asarray(img).astype(np.float32) - 128.0
    yy, xx = np.mgrid[0:H, 0:W]
    sheen = 16.0 * (1.0 - yy / H) + 7.0 * (1.0 - xx / W)
    out = np.zeros((H, W, 3), np.float32)
    for c in range(3):
        out[..., c] = base[c] + n * 1.7 + sheen
    return out

def main():
    plate = brushed()

    plate[:, :3] *= 0.55
    plate[:, 3:5] *= 0.82
    plate[:, 5:7] = np.minimum(plate[:, 5:7] * 1.10, 255)

    mask = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(mask)
    for x, y, w, h in WELLS.values():
        d.rounded_rectangle((x * S, y * S, (x + w) * S, (y + h) * S),
                            radius=9 * S, fill=255)
    m = np.asarray(mask).astype(np.float32) / 255.0

    floor = brushed(base=(150, 153, 157)) * 0.96
    plate = plate * (1 - m[..., None]) + floor * m[..., None]

    inner = np.asarray(mask.filter(ImageFilter.GaussianBlur(3.0))).astype(np.float32) / 255.0
    lip = np.zeros((H, W), np.float32)
    shifted = np.roll(inner, 5, axis=0)
    lip = np.clip(inner - shifted, 0.0, 1.0)
    plate *= (1.0 - 0.38 * lip)[..., None]
    lift = np.clip(inner - np.roll(inner, -4, axis=0), 0.0, 1.0)
    plate = np.minimum(plate * (1.0 + 0.10 * lift[..., None]), 255)

    outer = np.asarray(mask.filter(ImageFilter.MaxFilter(5))).astype(np.float32) / 255.0 - m
    plate = np.minimum(plate * (1.0 + 0.06 * np.clip(outer, 0, 1)[..., None]), 255)

    Image.fromarray(plate.clip(0, 255).astype(np.uint8)).save("plugin/assets/trench_sidecar.png")
    print(f"wrote plugin/assets/trench_sidecar.png ({W}x{H})")

if __name__ == "__main__":
    main()
