from PIL import Image
import numpy as np
from scipy.ndimage import distance_transform_edt

PLATE = r"C:\Users\hooki\trench-x3-clean\plugin\assets\df2_panel_beige.png"

RING_A, RING_W = 0.66, 7.5
TOP_A, TOP_W = 0.0, 10.0
MAX_A = 0.84
BOTTOM_FADE = 0.85
DARK = 60
BANDS = [(556, 634), (693, 771)]

im = Image.open(PLATE).convert("RGBA")
a = np.array(im)
grey = a[..., :3].mean(axis=2)

def floor_run(row):
    dark = row < DARK
    if not dark[270]:
        return None
    L = R = 270
    gap = 0
    for x in range(270, 84, -1):
        if dark[x]: L, gap = x, 0
        else:
            gap += 1
            if gap > 2: break
    gap = 0
    for x in range(270, 470):
        if dark[x]: R, gap = x, 0
        else:
            gap += 1
            if gap > 2: break
    return (L, R) if R - L >= 40 else None

for y0, y1 in BANDS:
    hole = np.zeros_like(grey, dtype=bool)
    for y in range(y0, y1):
        r = floor_run(grey[y])
        if r is not None:
            hole[y, r[0]:r[1] + 1] = True
    for y in range(y0, y1):
        xs_r = np.where(hole[y])[0]
        if len(xs_r) > 1:
            gaps = np.where(np.diff(xs_r) > 1)[0]
            for g in gaps:
                if xs_r[g + 1] - xs_r[g] < 30:
                    hole[y, xs_r[g]:xs_r[g + 1] + 1] = True
    for x in range(84, 470):
        ys_c = np.where(hole[y0:y1, x])[0]
        if len(ys_c) > 1:
            gaps = np.where(np.diff(ys_c) > 1)[0]
            for g in gaps:
                if ys_c[g + 1] - ys_c[g] < 8:
                    hole[y0 + ys_c[g]:y0 + ys_c[g + 1] + 1, x] = True
    ys, xs = np.where(hole)
    if len(ys) == 0:
        raise SystemExit(f"no hole found in band {y0}..{y1}")
    d_wall = distance_transform_edt(hole)
    top = {x: ys[xs == x].min() for x in np.unique(xs)}
    bot = {x: ys[xs == x].max() for x in np.unique(xs)}
    for y, x in zip(ys, xs):
        frac = (y - top[x]) / max(1, bot[x] - top[x])
        ring = RING_A * max(0.0, 1.0 - d_wall[y, x] / RING_W) ** 1.2
        ring *= 1.0 - BOTTOM_FADE * frac
        dt = y - top[x]
        cast = TOP_A * max(0.0, 1.0 - dt / TOP_W) ** 1.5
        shadow = min(MAX_A, ring + cast)
        a[y, x] = (0, 0, 0, int(round(shadow * 255)))

Image.fromarray(a, "RGBA").save(PLATE)
print("punched:", PLATE)

al = a[..., 3]
for y0, y1 in BANDS:
    region = al[y0:y1, 84:470]
    n0 = (region == 0).sum()
    npart = ((region > 0) & (region < 255)).sum()
    print(f"band {y0}..{y1}: fully-clear px {n0}, feathered px {npart}")
