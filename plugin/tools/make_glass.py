import sys
import numpy as np
from PIL import Image

W, H = 1590, 766
DUMP_W, DUMP_H = 156.0, 69.0
SCREEN_PX = W / 256.6

CELL_TOP_FRAC = 0.0

DECADE_X0, DECADE_W = 34.0, 57.0
H_LINES_Y = (15.0, 33.0, 51.0)

FIELD_HEX = "#1E5149"
def _field_palette(hexv):
    c = np.array([int(hexv.lstrip("#")[i:i + 2], 16) for i in (0, 2, 4)], dtype=float)
    # Positive LCDs need a nearly flat reflective field. Reusing the smoked-
    # glass corner colours with a light centre creates a giant luminous ring.
    luminance = float(c @ np.array([0.2126, 0.7152, 0.0722]))
    if luminance >= 150.0:
        lift = c + (255.0 - c) * 0.025
        fall = c * 0.94
        return c, lift, fall
    # Dark instrument glass stays visibly coloured across the whole aperture.
    # Keep the selected hue intact and use a modest value rolloff; fixed cyan
    # corner colours made every dark theme drift back toward the X3 reference.
    lift = np.clip(c * 1.07, 0.0, 255.0)
    fall = c * 0.78
    return c, lift, fall
CENTRE, LIFT_TL, FALL_BR = _field_palette(FIELD_HEX)
COMPRESS_SPAN = 0.11

GRID_MAJOR = np.array([0x20, 0x2A, 0x40], dtype=float)
GRID_MINOR = np.array([0x19, 0x22, 0x38], dtype=float)
MAJOR_STRENGTH = 0.72
MINOR_STRENGTH = 0.44
LINE_W_DUMP = 1.00
EDGE_W_DUMP = 0.35

NOISE_SD = 0.09

REFLECT_PX = 1.0
REFLECT_COLOUR = np.array([0x8A, 0x91, 0x7E], dtype=float)
REFLECT_ALPHA = 0.0
SHADOW_PX = 7.0
SHADOW_MIN = 0.56
SHEEN_COLOUR = np.array([0x8F, 0xAA, 0xBE], dtype=float)
SHEEN_ALPHA = 0.0
SHEEN_SPAN = 0.55

def ruling():
    out = []
    for decade in (-1, 0, 1, 2):
        x0 = DECADE_X0 + decade * DECADE_W
        for n in range(1, 10):
            x = x0 + np.log10(n) * DECADE_W
            if -0.5 <= x <= DUMP_W - 0.5:
                out.append((x, n == 1))
    return sorted(out)

def line_coverage(centers_dump, npix, scale):
    cov = np.zeros(npix)
    px = np.arange(npix)
    core = 0.5 * LINE_W_DUMP * scale
    edge = EDGE_W_DUMP * scale
    for c_dump in centers_dump:
        c = (c_dump + 0.5) * scale
        d = np.abs(px - c)
        cov = np.maximum(cov, np.clip((core + edge - d) / edge, 0.0, 1.0))
    return cov

LAMP_X, LAMP_Y = 0.50, 0.38
LAMP_FALL = 1.75

def field(compress):
    x = np.arange(W)[None, :] / (W - 1.0)
    y = np.arange(H)[:, None] / (H - 1.0)
    r = np.sqrt(((x - LAMP_X) * 1.0) ** 2 + ((y - LAMP_Y) * 0.72) ** 2)
    t = np.clip((r / r.max()) ** LAMP_FALL, 0.0, 1.0)
    if compress:
        return CENTRE[None, None, :] * (1.0 + COMPRESS_SPAN * (0.5 - t))[..., None]
    lo = LIFT_TL[None, None, :] + (CENTRE - LIFT_TL)[None, None, :] * (t / 0.5)[..., None]
    hi = CENTRE[None, None, :] + (FALL_BR - CENTRE)[None, None, :] * ((t - 0.5) / 0.5)[..., None]
    return np.where((t <= 0.5)[..., None], lo, hi)

# THE GRATICULE IS DRAWN HERE, and only here. It used to be baked into the
# committed trench_glass.png (d9bdfe97, 214 rules in the field itself) with this
# switch off, so GraphDisplay draws nothing and regenerating the field for a new
# theme silently produced a RULELESS display — change the theme, lose the grid.
# One ruling, authored where the field is authored.
DRAW_GRID = True

# Display markings. Stated by the theme (apply_theme passes --markings from
# 'support'), never derived from the field: a field can be dark or light and the
# rules have to read either way. The old derivation lightened the field colour,
# which is invisible the moment the field itself is light.
MARKINGS_HEX = "#3C7569"
MARKINGS_MINOR_MIX = 0.45      # minor rules, blended this far back toward field

# HOW HARD THE RULES LAND. These were authored for a DARK field, where the same
# alpha of a light ink is faint. Inverted, the identical numbers put a dark ink
# on a light field and the graticule became a fence: the trace is one pixel wide
# and the same colour as the rules, so it disappeared into them. The curve is the
# reading; the rules are the paper it is read on.
ROW_STRENGTH       = 0.30
ROW_ZERO_STRENGTH  = 0.40      # 0 dB is the one row worth finding at a glance
MINOR_STRENGTH_A   = 0.20
MAJOR_STRENGTH_A   = 0.36

def main(out_path, compress=False):
    img = field(compress)

    verts = ruling()
    major_x = [x for x, m in verts if m]
    minor_x = [x for x, m in verts if not m]
    sx, sy = W / DUMP_W, H / DUMP_H
    cell_top = int(H * CELL_TOP_FRAC)
    cell_mask = np.zeros((H, 1, 1))
    cell_mask[cell_top:, 0, 0] = 1.0
    DB_TOP, DB_BOT, DB_STEP = 18.0, -30.0, 12.0
    RULE = np.array([int(MARKINGS_HEX.lstrip("#")[i:i + 2], 16) for i in (0, 2, 4)],
                    dtype=float)
    # Hierarchy is carried by STRENGTH below, not by a second hue: the decade
    # rules and the 0 dB row are the markings colour outright, the minor rules
    # sit back toward the field.
    ROW_BOOST = RULE
    ROW_ZERO  = RULE
    ROW_CUT   = RULE
    COL_MAJOR = RULE
    COL_MINOR = RULE + (CENTRE - RULE) * MARKINGS_MINOR_MIX
    cell_h_frac = 1.0 - CELL_TOP_FRAC
    db = DB_TOP
    while DRAW_GRID and db >= DB_BOT - 1e-6:
        y_frac = CELL_TOP_FRAC + (DB_TOP - db) / (DB_TOP - DB_BOT) * cell_h_frac
        colour = ROW_ZERO if abs(db) < 1e-6 else (ROW_BOOST if db > 0 else ROW_CUT)
        strength = ROW_ZERO_STRENGTH if abs(db) < 1e-6 else ROW_STRENGTH
        cov = line_coverage([y_frac * DUMP_H], H, sy)[:, None, None] * cell_mask
        a = cov * strength
        img = img * (1.0 - a) + colour[None, None, :] * a
        db -= DB_STEP
    for cov, colour, strength in ((
        (line_coverage(minor_x, W, sx)[None, :, None] * cell_mask, COL_MINOR, MINOR_STRENGTH_A),
        (line_coverage(major_x, W, sx)[None, :, None] * cell_mask, COL_MAJOR, MAJOR_STRENGTH_A),
    ) if DRAW_GRID else ()):
        a = cov * strength
        img = img * (1.0 - a) + colour[None, None, :] * a

    yy = np.arange(H)[:, None, None] / float(H)
    sheen = np.clip(1.0 - yy / SHEEN_SPAN, 0.0, 1.0) ** 2
    img = img + (SHEEN_COLOUR[None, None, :] - img) * (sheen * SHEEN_ALPHA)

    r = max(1, int(round(REFLECT_PX * SCREEN_PX)))
    for sl in (np.s_[:r, :, :], np.s_[:, :r, :]):
        img[sl] = img[sl] * (1.0 - REFLECT_ALPHA) + REFLECT_COLOUR[None, None, :] * REFLECT_ALPHA

    s_px = max(1, int(round(SHADOW_PX * SCREEN_PX)))
    ramp = SHADOW_MIN + (1.0 - SHADOW_MIN) * (np.arange(s_px) / float(s_px))
    img[:, W - s_px:, :] *= ramp[::-1][None, :, None]
    img[H - s_px:, :, :] *= ramp[::-1][:, None, None]
    img[:, :s_px, :] *= ramp[None, :, None]
    img[:s_px, :, :] *= ramp[:, None, None]

    rng = np.random.default_rng(240)
    img = img + rng.normal(0.0, NOISE_SD, size=img.shape)

    Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(out_path)
    print(f"wrote {out_path} ({W}x{H}, {len(major_x)} major + {len(minor_x)} minor "
          f"verticals, {'compressed' if compress else 'stated-corner'} field)")

if __name__ == "__main__":
    for a in sys.argv[1:]:
        if a.startswith("--field="):
            CENTRE, LIFT_TL, FALL_BR = _field_palette(a.split("=", 1)[1])
            REFLECT_COLOUR = np.clip(CENTRE * 1.85, 0, 255)
        elif a.startswith("--markings="):
            MARKINGS_HEX = a.split("=", 1)[1]
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    main(args[0] if args else "plugin/assets/trench_glass.png",
         "--compress" in sys.argv)
