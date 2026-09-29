import argparse

import numpy as np
from PIL import Image, ImageFilter


def main():
    p = argparse.ArgumentParser()
    p.add_argument("strip")
    p.add_argument("out")
    p.add_argument("--frame", type=int, default=96)
    p.add_argument("--dx", type=int, default=0)
    p.add_argument("--dy", type=int, default=1)
    p.add_argument("--blur", type=float, default=1.2)
    p.add_argument("--opacity", type=float, default=0.35)
    a = p.parse_args()
    alpha = Image.open(a.strip).convert("RGBA").crop((0, 0, a.frame, a.frame)).split()[3]
    shifted = Image.new("L", alpha.size, 0)
    shifted.paste(alpha, (a.dx, a.dy))
    soft = np.asarray(shifted.filter(ImageFilter.GaussianBlur(a.blur)), dtype=np.float32) * a.opacity
    out = np.zeros((a.frame, a.frame, 4), dtype=np.uint8)
    out[..., 3] = np.clip(soft, 0, 255).astype(np.uint8)
    Image.fromarray(out, "RGBA").save(a.out)


main()
