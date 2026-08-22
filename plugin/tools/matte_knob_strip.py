
from __future__ import annotations

import shutil
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
STRIP = ROOT / "plugin" / "assets" / "trench_knob_strip.png"
BACKUP = STRIP.with_suffix(".png.bak_pre_matte_20260806")

GAIN = 0.62
KNEE = 0.45
CEILING = 0.66

def matte(rgb: np.ndarray) -> np.ndarray:
    n = np.clip(rgb / 255.0 * GAIN, 0.0, 1.0)
    over = np.maximum(n - KNEE, 0.0)
    span = max(CEILING - KNEE, 1e-6)
    n = np.where(n > KNEE, KNEE + span * (1.0 - np.exp(-over / span)), n)
    return np.clip(n * 255.0, 0, 255)

def main() -> int:
    img = Image.open(STRIP).convert("RGBA")
    a = np.asarray(img, dtype=np.float32)
    opaque = a[..., 3] > 200

    out = a.copy()
    out[..., :3] = matte(a[..., :3])
    pointer = (a[..., :3].min(axis=-1) > 200.0) & opaque
    out[..., :3][pointer] = a[..., :3][pointer]
    result = Image.fromarray(np.clip(out + 0.5, 0, 255).astype(np.uint8), "RGBA")

    before_med = float(np.median(a[..., :3][opaque]))
    after_med = float(np.median(out[..., :3][opaque]))
    before_max = float(a[..., :3][opaque].max())
    after_max = float(out[..., :3][opaque].max())

    if "--preview" in sys.argv:
        f = img.width // (img.width // img.height) if img.height else img.height
        fw = img.height
        pair = Image.new("RGBA", (fw * 3, fw * 2), (18, 17, 16, 255))
        for i, frame in enumerate((0, img.width // fw // 2, img.width // fw - 1)):
            pair.paste(img.crop((frame * fw, 0, frame * fw + fw, fw)), (i * fw, 0))
            pair.paste(result.crop((frame * fw, 0, frame * fw + fw, fw)), (i * fw, fw))
        dst = ROOT / "knob_matte_preview.png"
        pair.resize((pair.width * 2, pair.height * 2), Image.NEAREST).save(dst)
        print(f"preview -> {dst} (top row = current, bottom = matte)")
    else:
        if not BACKUP.exists():
            shutil.copy2(STRIP, BACKUP)
            print(f"backed up -> {BACKUP.name}")
        result.save(STRIP)
        print(f"matted {STRIP.name}")

    print(f"  body median {before_med:.0f} -> {after_med:.0f}")
    print(f"  peak        {before_max:.0f} -> {after_max:.0f}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
