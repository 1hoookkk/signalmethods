import numpy as np
from PIL import Image
from pathlib import Path

S = 4
FW, FH, NF = 6 * S, 47 * S, 64

NAVY = np.ones(3, dtype=np.float32)

CREST = 140.0
GROOVE = 8.0
EDGE = 136.0
FAR_LINE = 4.0

frames = []
for i in range(NF):
    f = np.zeros((FH, FW, 3), dtype=np.float32)

    R = 40.0
    dphi = 4.0 / R
    phase = -0.5 * i
    for y in range(FH):
        s = (FH / 2.0 - (y + 0.5)) / R
        if abs(s) >= 1.0:
            continue
        rib_gap = int(np.floor(np.arcsin(s) / dphi + phase)) % 2
        t = (y + 0.5) / FH

        e = float(np.interp(t, [0.0, 0.10, 0.32, 0.60, 0.90, 1.0],
                               [0.84, 0.80, 1.00, 0.75, 0.31, 0.28]))
        level = GROOVE if rib_gap else CREST * e

        for x in range(1 * S, 4 * S):
            k = 1.0 - 0.18 * ((x - 1 * S) / (3 * S))
            if x == 1 * S:
                k *= 0.85
            f[y, x] = NAVY * (level * k)

    edge_tint = (NAVY + 2.0 * np.array([1.0, 1.0, 1.0])) / 3.0
    edge_tint /= float(edge_tint @ [0.2126, 0.7152, 0.0722])
    for y in range(FH):
        t = y / FH
        rim = EDGE * float(np.interp(t, [0.0, 0.06, 0.5, 0.94, 1.0],
                                        [0.35, 0.9, 1.0, 0.85, 0.3]))
        f[y, 0:S] = edge_tint * rim

    f[:, 4 * S:5 * S] = NAVY * FAR_LINE
    f[:, 5 * S:FW] = NAVY * (FAR_LINE + 9.0)

    f[0:S, :] *= 0.25
    f[FH - S:, :] *= 0.25

    frames.append(np.clip(f, 0, 255).astype(np.uint8))

strip = np.concatenate(frames, axis=1)
rgba = np.dstack([strip, np.full(strip.shape[:2], 255, np.uint8)])

OUT = Path(r"C:\Users\hooki\trench-x3-clean\plugin\assets\thin_wheel_strip.png")
Image.fromarray(rgba).save(OUT)
print("Saved thin wheel strip", OUT)

check = strip[:, :FW, :].astype(np.float32)
l = check @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
print("frame0 column luma means:", np.round(l.mean(axis=0), 1).tolist())
print("target (4388, at 8px)   : [121.1, 44.4, 45.7, 45.2, 45.6, 45.2, 45.6, 4.2]")
