from PIL import Image
import numpy as np
from scipy.ndimage import gaussian_filter

ASSETS = r"C:\Users\hooki\trench-native\plugin\plugin\assets"
PLATE = ASSETS + r"\df2_panel_beige.png"
PX, PY = 828.0 / 326.0, 1280.0 / 503.0

im = Image.open(PLATE).convert("RGBA")
a = np.array(im, dtype=float)
H, W = a.shape[:2]
shadow = np.zeros((H, W), dtype=float)

yy, xx = np.mgrid[0:H, 0:W]
for xc, ybot in ((107.1, 246.4), (106.9, 300.2)):
    nx = (xx / PX - xc) / 45.0
    kenv = np.clip(1.0 - nx * nx, 0.0, None)
    kd = yy / PY - ybot
    with np.errstate(divide='ignore', invalid='ignore'):
        kfall = np.where(kenv > 0.0,
                         np.clip(1.0 - kd / (7.0 * kenv), 0.0, 1.0), 0.0)
    kfall[kd < 0.0] = 0.0
    kfield = gaussian_filter(0.85 * kenv * kfall, 1.2)
    kfield[:int(round(ybot * PY)), :] = 0.0
    np.maximum(shadow, kfield, out=shadow)

yy, xx = np.mgrid[0:H, 0:W]
ny = (yy / PY - 261.0) / 35.0
env = np.clip(1.0 - ny * ny, 0.0, None) ** 0.75
d = xx / PX - 282.0
with np.errstate(divide='ignore', invalid='ignore'):
    fall = np.where(env > 0.0, np.clip(1.0 - d / (7.5 * env), 0.0, 1.0), 0.0)
fall[d < 0.0] = 0.0
mix_field = gaussian_filter(1.14 * env * fall, 0.6)
mix_field[:, :int(round(282.0 * PX))] = 0.0
np.maximum(shadow, mix_field, out=shadow)

def rounded_rect(x0, y0, x1, y1, r):
    m = np.zeros((H, W), dtype=float)
    yy, xx = np.mgrid[0:H, 0:W]
    inner = (xx >= x0 + r) & (xx <= x1 - r) & (yy >= y0) & (yy <= y1)
    inner |= (xx >= x0) & (xx <= x1) & (yy >= y0 + r) & (yy <= y1 - r)
    for cx in (x0 + r, x1 - r):
        for cy in (y0 + r, y1 - r):
            inner |= (xx - cx) ** 2 + (yy - cy) ** 2 <= r * r
    m[inner] = 1.0
    return m

for y0, y1 in ((562, 627), (699, 764)):
    outer = rounded_rect(97 - 7, y0 - 7, 445 + 7, y1 + 7, 20)
    inner = rounded_rect(97, y0, 445, y1, 14)
    frame = gaussian_filter(np.clip(outer - inner, 0.0, 1.0), 2.0) * 0.22
    frame *= 1.0 - inner
    np.maximum(shadow, frame, out=shadow)

a[..., :3] *= (1.0 - np.clip(shadow, 0.0, 0.85))[..., None]
Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), "RGBA").save(PLATE)
print("wheel-silhouette + readout shadows baked")
