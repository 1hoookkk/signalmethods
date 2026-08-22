import hashlib
import numpy as np
from PIL import Image

REF = "dev/reference/emu_mix_wheel/BITMAP4388_2.bmp"
OUT_STRIP = "plugin/assets/mix_wheel_strip.png"
OUT_SHADOW = "plugin/assets/mix_wheel_seat_shadow.png"

NAT_W, NAT_H = 8, 47
LOG_W, LOG_H = 12, 70
SS = 3
AW, AH = LOG_W * SS, LOG_H * SS

RIM_FRAC, EDGE_FRAC = 1.0 / NAT_W, 1.0 / NAT_W
PITCH_LOG = 3.0
DUTY = 0.50
FRAMES = 120
ADVANCE_LOG = PITCH_LOG / 6.0

CREST_K, FLOOR_K = 1.00, 0.075
RIM_RIPPLE = 0.04
RIM_BEVEL = 0.18
DITHER_SD = 3.8

NAT_PER_LOG = NAT_H / float(LOG_H)
INDENT_Y0 = 60.75
IND_U = [-2.6, -1.6, -1.2, 0.0, 1.2, 1.9, 3.1, 3.9, 5.1, 5.7, 6.6]
IND_BASE = [.077, .077, .030, .030, .030, .250, .250, .104, .104, .077, .077]
IND_AMP = [1.00, 1.00, .020, .020, .020, .053, .053, .082, .082, 1.00, 1.00]
IND_RIM = [1.00, 1.00, .920, .920, .920, .950, .950, .950, .960, 1.00, 1.00]

def envelopes():
    a = np.array(Image.open(REF).convert("L")).astype(float)
    s = np.stack([a[:, f * NAT_W:(f + 1) * NAT_W] for f in range(41)])
    face = s[:, :, 1:NAT_W - 1]
    return (face.max(axis=(0, 2)), face.min(axis=(0, 2)),
            s[:, :, 0].mean(axis=0), s[:, :, NAT_W - 1].mean(axis=0))

def resample(curve, n):
    m = len(curve)
    y = (np.arange(n) + 0.5) * m / n
    out = np.interp(y, np.arange(m) + 0.5, curve)
    out[y < 1.0] = curve[0]
    out[y >= m - 1.0] = curve[-1]
    return out

def rib_coverage(y_log, phase):
    lo, hi = y_log - 0.5 / SS, y_log + 0.5 / SS
    t = np.linspace(lo, hi, 8, endpoint=False) + (hi - lo) / 16.0
    return np.mean(((t / PITCH_LOG + phase) % 1.0) < DUTY)

def build_strip():
    E, G, R, D = envelopes()

    eE, eG = resample(E, AH), resample(G, AH)
    eR, eD = resample(R, AH), resample(D, AH)

    K = 16
    sub = (np.arange(AW * K) + 0.5) / (AW * K) * NAT_W
    in_rim = sub < NAT_W * RIM_FRAC
    in_edge = sub >= NAT_W * (1.0 - EDGE_FRAC)
    in_face = ~in_rim & ~in_edge
    bev = 1.0 + RIM_BEVEL * np.cos(2.0 * np.pi * (sub - 0.45))
    bev = np.where(in_rim, bev / bev[in_rim].mean(), 0.0)
    w_rim = bev.reshape(AW, K).mean(axis=1)
    w_face = in_face.astype(float).reshape(AW, K).mean(axis=1)
    w_edge = in_edge.astype(float).reshape(AW, K).mean(axis=1)

    yy, xx = np.mgrid[0:AH, 0:AW]
    h = (yy * 73856093) ^ (xx * 19349663)
    dither = ((h % 1024) / 1023.0 - 0.5) * 2.0 * DITHER_SD * w_face[None, :]

    strip = np.zeros((AH, AW * FRAMES, 4), dtype=np.uint8)
    y_log = (np.arange(AH) + 0.5) / SS

    for f in range(FRAMES):
        phase = -(f * ADVANCE_LOG) / PITCH_LOG
        cov = np.array([rib_coverage(y, phase) for y in y_log])

        u = (y_log - (INDENT_Y0 - f * ADVANCE_LOG)) * NAT_PER_LOG
        base = np.interp(u, IND_U, IND_BASE, left=FLOOR_K, right=FLOOR_K)
        amp = np.interp(u, IND_U, IND_AMP, left=1.0, right=1.0)
        rim_cut = np.interp(u, IND_U, IND_RIM, left=1.0, right=1.0)

        lit = base + amp * (CREST_K - FLOOR_K) * cov

        face_col = eG + (eE - eG) * lit
        rim_col = eR * rim_cut * (1.0 + RIM_RIPPLE * amp * (2.0 * cov - 1.0))

        frame = (rim_col[:, None] * w_rim[None, :]
                 + face_col[:, None] * w_face[None, :]
                 + eD[:, None] * w_edge[None, :]
                 + dither)

        px = np.clip(np.rint(frame), 0, 255).astype(np.uint8)
        x0 = f * AW
        strip[:, x0:x0 + AW, 0] = px
        strip[:, x0:x0 + AW, 1] = px
        strip[:, x0:x0 + AW, 2] = px
        strip[:, x0:x0 + AW, 3] = 255

    Image.fromarray(strip, "RGBA").save(OUT_STRIP)
    return strip.shape

SH_CENTRE_NAT = 24.0
SH_RY_NAT = 17.0
SH_RX_NAT = 5.0
SH_N = 2.0
SH_RHO = [0.00, 0.20, 0.33, 0.47, 0.60, 0.73, 0.87, 1.00, 1.13, 1.27, 1.40,
          1.53, 1.65]
SH_F = [1.00, 1.00, 0.96, 0.86, 0.75, 0.69, 0.55, 0.41, 0.28, 0.14, 0.06,
        0.03, 0.00]

def build_shadow():
    w = LOG_W * SS
    h = (LOG_H + 1) * SS
    yc = SH_CENTRE_NAT * (LOG_H / float(NAT_H)) * SS
    ry = SH_RY_NAT * (LOG_H / float(NAT_H)) * SS
    rx = SH_RX_NAT * (LOG_W / float(NAT_W)) * SS
    xs = np.arange(w) + 0.5
    ys = np.arange(h) + 0.5
    rho = ((xs[None, :] / rx) ** SH_N
           + (np.abs(ys[:, None] - yc) / ry) ** SH_N) ** (1.0 / SH_N)
    alpha = np.interp(rho, SH_RHO, SH_F, left=SH_F[0], right=0.0)
    img = np.zeros((h, w, 4), dtype=np.uint8)
    img[..., 3] = np.rint(np.clip(alpha, 0.0, 1.0) * 255).astype(np.uint8)
    Image.fromarray(img, "RGBA").save(OUT_SHADOW)
    return img.shape

def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()

if __name__ == "__main__":
    s = build_strip()
    d = build_shadow()
    print("strip  %s  %d frames of %dx%d (3x of %dx%d)" %
          (OUT_STRIP, FRAMES, AW, AH, LOG_W, LOG_H))
    print("       %d x %d  sha256 %s" % (s[1], s[0], sha(OUT_STRIP)))
    print("shadow %s  %d x %d (3x of %dx%d)" %
          (OUT_SHADOW, d[1], d[0], d[1] // SS, d[0] // SS))
    print("       sha256 %s" % sha(OUT_SHADOW))
