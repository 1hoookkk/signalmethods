import sys

import numpy as np
from PIL import Image

PLOT_W, PLOT_H = 246, 115
SS = 6
W, H = PLOT_W * SS, PLOT_H * SS

F_LO, F_HI = 20.0, 20000.0        # kept for the dB mapping note only
# The traced X3 dump frame the ruling geometry lives in.
DUMP_W = 156.0
DECADE_X0, DECADE_W = 34.0, 57.0
DB_TOP, DB_BOT, DB_STEP = 18.0, -30.0, 12.0

# THE LOG GRID IS THE ICONIC PART (Tyson 2026-08-12: "we need the grid tho, the
# iconic log grid"). What makes it read as a log graticule is the CLUSTERING -
# the 2..9 subdivisions bunching toward the end of each decade. Kill the minors
# and you keep three lonely verticals and lose the whole signature, which is
# what 0.09 did. These weights are for a PALE panel: dark rules cut into a light
# field carry at a strength that would shout on smoked glass.
# BACK ON SMOKED GLASS (Tyson 2026-08-12 final synthesis). The pale-panel
# weights above were tuned for DARK rules cut into a light field; on a dark
# field the rules LIFT instead, and light-on-dark reads far hotter at the same
# number. Majors stay readable structure, minors go to barely-present - the
# clustering is still legible as a log grid when you look, and invisible as
# texture when you are reading the trace.
A_DECADE, A_SUB = 1.00, 0.42
A_UNITY,  A_ROW = 1.00, 0.85   # major HORIZONTALS are back (2026-08-12 lock)

def stamp(alpha, pos_px, thickness_px, weight, vertical):
    start = int(round(pos_px * SS - thickness_px * SS / 2.0))
    start = int(round(start / SS)) * SS
    end = start + thickness_px * SS
    start, end = max(0, start), min(W if vertical else H, end)
    if end <= start:
        return
    if vertical:
        alpha[:, start:end] = np.maximum(alpha[:, start:end], weight)
    else:
        alpha[start:end, :] = np.maximum(alpha[start:end, :], weight)

def main(out_path):
    alpha = np.zeros((H, W), dtype=float)
    span = np.log10(F_HI / F_LO)

    # THE X3'S OWN RULING GEOMETRY (Tyson 2026-08-12: "the bitmap isn't right
    # either, the grid"). Earlier this file computed the verticals from OUR
    # 20 Hz..20 kHz axis so every decade line sat on a true frequency. That is
    # more correct and it is the wrong PICTURE: our span puts a decade boundary
    # hard on the left edge and gives three evenly-placed clusters, where the
    # X3's ruling is offset - its boundaries fall at dump x 34/91/148 on a 57px
    # decade, so subdivisions run off the left edge and the left of the screen
    # is dense. That offset is the iconic shape.
    #
    # Traced from BITMAP4613_1.bmp and already recorded in tools/make_glass.py;
    # this is the same ruling(), remapped from the 156px dump frame onto the
    # plot. Decade -1 is included precisely to fill the left edge.
    for major in (False, True):
        for decade in (-1, 0, 1, 2):
            x0 = DECADE_X0 + decade * DECADE_W
            for n in range(1, 10):
                if (n == 1) != major:
                    continue
                x_dump = x0 + np.log10(n) * DECADE_W
                if not (-0.5 <= x_dump <= DUMP_W - 0.5):
                    continue
                x = x_dump / DUMP_W * PLOT_W
                if -0.5 <= x <= PLOT_W + 0.5:
                    stamp(alpha, min(max(x, 0.0), PLOT_W - 1), 1,
                          A_DECADE if major else A_SUB, True)

    # MAJOR HORIZONTALS RESTORED (Tyson 2026-08-12 lock: "major verticals every
    # large division, major horizontals, much thinner minor verticals"). They
    # were pulled when the panel was bright and the crossings read as graph
    # paper; on the teal LCD they are part of the reference's own structure.
    rows = []
    db = DB_TOP
    while db >= DB_BOT - 1e-9:
        rows.append(db)
        db -= DB_STEP
    for db in rows:
        y = (DB_TOP - db) / (DB_TOP - DB_BOT) * PLOT_H
        stamp(alpha, min(y, PLOT_H - 1), 1, A_ROW, False)

    rgba = np.zeros((H, W, 4), dtype=np.uint8)
    rgba[..., :3] = 255
    rgba[..., 3] = np.clip(alpha * 255.0, 0, 255).astype(np.uint8)
    Image.fromarray(rgba, "RGBA").save(out_path)

    lit = int((alpha > 0).sum())
    print(f"wrote {out_path} ({W}x{H} = {SS}x of {PLOT_W}x{PLOT_H}), "
          f"{lit} lit subpixels, weights {sorted(set(np.unique(alpha)) - {0.0})}")

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    main(args[0] if args else "plugin/assets/trench_display_grid.png")
