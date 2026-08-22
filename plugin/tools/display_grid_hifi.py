import numpy as np
from PIL import Image
from pathlib import Path

ASSET = Path(r"C:\Users\hooki\df2-workstation\plugin\assets\display_bitmap4613.png")
S = 6

src = np.array(Image.open(ASSET).convert("RGB"), dtype=np.float32)
H, W = src.shape[:2]

def rule_positions(profile, win=9, delta=2.0):
    pad = np.pad(profile, win // 2, mode="edge")
    base = np.convolve(pad, np.ones(win) / win, mode="valid")
    hit = np.nonzero(profile < base - delta)[0]
    groups = []
    for x in hit:
        if groups and x - groups[-1][-1] <= 1:
            groups[-1].append(x)
        else:
            groups.append([x])
    return [int(np.mean(g)) for g in groups]

lum = src.mean(axis=2)
vx = rule_positions(lum.mean(axis=0))
hy = rule_positions(lum.mean(axis=1))
print(f"vertical rules ({len(vx)}):", vx)
print(f"horizontal rules ({len(hy)}):", hy)

bed = src.copy()
for x in vx:
    l, r = max(0, x - 1), min(W - 1, x + 1)
    bed[:, x] = 0.5 * (bed[:, l] + bed[:, r])
for y in hy:
    t, b = max(0, y - 1), min(H - 1, y + 1)
    bed[y, :] = 0.5 * (bed[t, :] + bed[b, :])
bed_hi = np.array(
    Image.fromarray(bed.astype(np.uint8)).resize((W * S, H * S), Image.BILINEAR),
    dtype=np.float32)

out = bed_hi
LINE = 4

def draw_v(x):
    shade = src[:, x, :]
    shade_hi = np.array(Image.fromarray(shade[None].astype(np.uint8))
                        .resize((H * S, 1), Image.BILINEAR), dtype=np.float32)[0]
    cx = int(round((x + 0.5) * S))
    out[:, cx - LINE // 2: cx + LINE - LINE // 2, :] = shade_hi[:, None, :]

def draw_h(y):
    shade = src[y, :, :]
    shade_hi = np.array(Image.fromarray(shade[None].astype(np.uint8))
                        .resize((W * S, 1), Image.BILINEAR), dtype=np.float32)[0]
    cy = int(round((y + 0.5) * S))
    out[cy - LINE // 2: cy + LINE - LINE // 2, :, :] = shade_hi[None, :, :]

for x in vx:
    draw_v(x)
for y in hy:
    draw_h(y)

Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(ASSET)
print("wrote", ASSET, f"{W*S}x{H*S}")
