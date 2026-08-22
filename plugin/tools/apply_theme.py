#!/usr/bin/env python3
from __future__ import annotations

import colorsys
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
THEMES = ROOT / "design" / "themes.json"
LAYOUT = ROOT / "plugin" / "source" / "UiLayout.h"
GLASS_TOOL = ROOT / "tools" / "make_glass.py"
LAMP_TOOL = ROOT / "tools" / "retint_roller_glow.py"

def hex_to_rgb(h: str) -> tuple[float, float, float]:
    h = h.lstrip("#")
    return tuple(int(h[i : i + 2], 16) / 255 for i in (0, 2, 4))

def rgb_to_hex(r: float, g: float, b: float) -> str:
    return "#%02X%02X%02X" % tuple(max(0, min(255, round(v * 255))) for v in (r, g, b))

def relative_luminance(hexv: str) -> float:
    def lin(v):
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4

    r, g, b = (lin(c) for c in hex_to_rgb(hexv))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b

def shade(hue: float, sat: float, light: float) -> str:
    return rgb_to_hex(*colorsys.hls_to_rgb(hue / 360.0, light, sat))

def derive(theme: dict) -> dict:
    ah, asat = float(theme["aperture_hue"]), float(theme["aperture_sat"])
    accent = theme["accent"].upper()
    lamp = theme.get("lamp", theme["accent"]).upper()
    glass = theme.get("glass", shade(ah, asat, 0.22)).upper()
    r, g, b = hex_to_rgb(accent)
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    return {
        "accent": accent,
        "curveColour": accent,
        "curveHighlight": theme.get("curve_highlight", shade(h * 360, s, min(0.92, l + 0.14))).upper(),
        "rollerIllumination": lamp,
        "modulationLamp": lamp,
        "phosphor": glass,
        "spectrumGhost": glass,
        "screenEdge": theme["edge"].upper(),
        "telemetry": theme["support"].upper(),
        "dashed": theme["support"].upper(),
    }

GLASS_KNOT_LIGHT = 0.38

def glass_ratio(theme: dict) -> tuple[float, float, float]:
    if "glass" in theme:
        return tuple(round(c * 255 / 135.3, 3) for c in hex_to_rgb(theme["glass"]))
    rgb = colorsys.hls_to_rgb(
        float(theme["aperture_hue"]) / 360.0, GLASS_KNOT_LIGHT, float(theme["aperture_sat"])
    )
    return tuple(round(c * 255 / 135.3, 3) for c in rgb)

def patch_layout(colours: dict) -> int:
    src = LAYOUT.read_text()
    n = 0
    for name, hexv in colours.items():
        pattern = re.compile(
            r'(layout\.colours\s*\[\s*"%s"\s*\]\s*=\s*juce::Colour\s*\(\s*0xff)[0-9A-Fa-f]{6}' % name
        )
        src, hits = pattern.subn(lambda m: m.group(1) + hexv.lstrip("#"), src)
        n += hits
    LAYOUT.write_text(src)
    return n

def patch_tool(path: Path, pattern: str, replacement: str) -> bool:
    text = path.read_text()
    new, hits = re.subn(pattern, replacement, text, count=1)
    if hits:
        path.write_text(new)
    return bool(hits)

def main() -> int:
    doc = json.loads(THEMES.read_text())
    if "--list" in sys.argv:
        for name, t in doc["themes"].items():
            mark = "*" if name == doc["active"] else " "
            print(f"{mark} {name:14s} {t['accent']}  lum {relative_luminance(t['accent']):.2f}"
                  f"  {t['description'][:58]}")
        return 0

    name = next((a for a in sys.argv[1:] if not a.startswith("-")), doc["active"])
    if name not in doc["themes"]:
        print(f"unknown theme {name!r}; try --list")
        return 2
    theme = doc["themes"][name]

    colours = derive(theme)
    wrote = patch_layout(colours)
    ratio = glass_ratio(theme)
    lamp = theme.get("lamp", theme["accent"])
    glass_hex = theme.get("glass", shade(float(theme["aperture_hue"]),
                                         float(theme["aperture_sat"]), 0.22))
    # The graticule is STATED, not derived from the field: 'support' is the
    # display-markings colour, and it has to read on a dark field and a light
    # one alike.
    subprocess.run([sys.executable, str(GLASS_TOOL),
                    str(ROOT / "plugin" / "assets" / "trench_glass.png"),
                    f"--field={glass_hex}",
                    f"--markings={theme['support']}"],
                   check=True, cwd=ROOT)
    lamp_args = [lamp] + ([theme["lamp_core"]] if "lamp_core" in theme else [])
    subprocess.run([sys.executable, str(LAMP_TOOL), *lamp_args], check=True, cwd=ROOT)
    subprocess.run([sys.executable, str(ROOT / "tools" / "bake_knob_pointer_lamp.py"), lamp],
                   check=True, cwd=ROOT)
    if doc["active"] != name:
        doc["active"] = name
        THEMES.write_text(json.dumps(doc, indent=2) + "\n")

    print(f"\ntheme '{name}' applied (now active)")
    print(f"  accent      {theme['accent']}  (curve + wheel lamp; judge visibility on the render)")
    print(f"  aperture    hue {theme['aperture_hue']} sat {theme['aperture_sat']} -> ratio {ratio}")
    print(f"  {wrote} colour tokens written to UiLayout.h")
    print("  glass + locked cycle-10 wheel strip + knob pointer regenerated")
    print("\nrebuild: cmake --build plugin/build --config Release --target TRENCH_VST3")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
