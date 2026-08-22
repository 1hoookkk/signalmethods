import sys
import numpy as np
from PIL import Image

TW, TH = 560, 1006
CAP = 38

SRC_TOP    = (0, 53)
SRC_WELLS  = [(53, 455), (492, 899), (933, 1329)]
SRC_GAP    = (455, 492)
SRC_BOTTOM = (1329, 1375)

TARGET = [
    (0,    60,   "field", SRC_TOP),
    (60,   400,  "well",  0),
    (400,  436,  "field", SRC_GAP),
    (436,  660,  "well",  1),
    (660,  696,  "field", SRC_GAP),
    (696,  792,  "well",  2),
    (792,  828,  "field", SRC_GAP),
    (828,  988,  "well",  1),
    (988,  1006, "field", SRC_BOTTOM),
]

def band(img, y0, y1):
    return img.crop((0, y0, img.width, y1))

def fit_well(img, src, height):
    y0, y1 = src
    cap_src = 46
    top = band(img, y0, y0 + cap_src).resize((TW, CAP), Image.LANCZOS)
    bot = band(img, y1 - cap_src, y1).resize((TW, CAP), Image.LANCZOS)
    mid = band(img, y0 + cap_src, y1 - cap_src).resize((TW, height - 2 * CAP),
                                                       Image.LANCZOS)
    out = Image.new("RGB", (TW, height))
    out.paste(top, (0, 0))
    out.paste(mid, (0, CAP))
    out.paste(bot, (0, height - CAP))
    return out

def main(src_path):
    art = Image.open(src_path).convert("RGB")
    art = art.resize((TW, art.height), Image.LANCZOS)
    out = Image.new("RGB", (TW, TH))
    for y0, y1, kind, src in TARGET:
        h = y1 - y0
        piece = (fit_well(art, SRC_WELLS[src], h) if kind == "well"
                 else band(art, *src).resize((TW, h), Image.LANCZOS))
        out.paste(piece, (0, y0))
    out.save("plugin/assets/trench_sidecar.png")
    print(f"wrote plugin/assets/trench_sidecar.png ({TW}x{TH}) from {src_path}")

if __name__ == "__main__":
    main(sys.argv[1])
