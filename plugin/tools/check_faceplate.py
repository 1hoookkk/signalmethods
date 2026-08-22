#!/usr/bin/env python3
"""Certify a generated faceplate PNG against the layout well geometry.

  python tools/check_faceplate.py candidate.png [--install]

Any resolution accepted (rescaled to 828x1280). Each well must be a dark
recess centred on its layout rect. Writes an annotated overlay next to the
candidate. --install copies a passing plate to plugin/assets/ and rebuilds
the VST3 + Workstation.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
W, H = 828, 1280
SX, SY = W / 1010.0, H / 1557.0

WELLS = {
    "screen":       (110, 233, 795, 383),
    "morph wheel":  (115, 680, 428, 92),
    "q wheel":      (114, 848, 429, 93),
    "morph readout": (582, 697, 190, 77),
    "q readout":    (582, 858, 190, 77),
}

def art_box(r):
    x, y, w, h = r
    return (x * SX, y * SY, (x + w) * SX, (y + h) * SY)

def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    src = Path(sys.argv[1])
    im = Image.open(src).convert("RGB").resize((W, H), Image.LANCZOS)
    a = np.asarray(im).astype(np.float32).mean(axis=2)

    overlay = im.copy()
    d = ImageDraw.Draw(overlay)
    ok_all = True
    print(f"{src.name} -> {W}x{H}")
    for name, r in WELLS.items():
        x0, y0, x1, y1 = art_box(r)
        ix0, iy0, ix1, iy1 = map(int, (x0, y0, x1, y1))
        inside = a[iy0 + 4:iy1 - 4, ix0 + 4:ix1 - 4]
        ring = np.concatenate([
            a[max(iy0 - 18, 0):iy0 - 6, ix0:ix1].ravel(),
            a[iy1 + 6:iy1 + 18, ix0:ix1].ravel()])
        depth = float(ring.mean() - inside.mean())
        def edge_offset(profile, expected, into_dark):
            lo = max(expected - 22, 2)
            hi = min(expected + 22, len(profile) - 3)
            steps = profile[lo + 2:hi + 2] - profile[lo - 2:hi - 2]
            if into_dark:
                steps = -steps
            return float(lo + int(np.argmax(steps)) - expected)

        col = a[iy0 + 6:iy1 - 6, :].mean(axis=0)
        row = a[:, ix0 + 6:ix1 - 6].mean(axis=1)
        offs = [edge_offset(col, ix0, True), edge_offset(col, ix1, False),
                edge_offset(row, iy0, True), edge_offset(row, iy1, False)]
        dx = (offs[0] + offs[1]) / 2.0
        dy = (offs[2] + offs[3]) / 2.0
        worst = max(abs(o) for o in offs)
        ok = depth > 12.0 and worst < 12.0
        ok_all &= ok
        col = (80, 220, 120) if ok else (240, 70, 60)
        d.rectangle((ix0, iy0, ix1, iy1), outline=col, width=3)
        d.text((ix0 + 4, iy0 - 16),
               f"{name}: depth {depth:+.0f}  offset {dx:+.0f},{dy:+.0f} px",
               fill=col)
        print(f"  {name:14s} depth {depth:+6.1f}  centre offset "
              f"({dx:+5.1f},{dy:+5.1f}) px  {'OK' if ok else 'FAIL'}")
    out = src.with_name(src.stem + "_geometry_check.png")
    overlay.save(out)
    print(("PASS" if ok_all else "FAIL"), "->", out)

    if ok_all and "--install" in sys.argv:
        dst = ROOT / "plugin" / "assets" / "df2_panel_beige.png"
        im.save(dst)
        print("installed asset ->", dst)
        for script in ("tools/build_install_vst3.ps1",
                       "tools/build_workstation_exe.ps1"):
            subprocess.run(["powershell", "-ExecutionPolicy", "Bypass",
                            "-File", script], cwd=ROOT, check=True)
        print("VST3 + Workstation rebuilt. Restart FL to pick it up.")
    elif not ok_all:
        sys.exit(1)

if __name__ == "__main__":
    main()
