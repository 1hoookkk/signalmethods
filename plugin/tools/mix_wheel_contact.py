import numpy as np
from PIL import Image, ImageDraw

D = "dev/reference/emu_mix_wheel/"
REF_STRIP = D + "BITMAP4388_2.bmp"
REF_SEAT = D + "BITMAP4386_2.bmp"
STRIP = "plugin/assets/mix_wheel_strip.png"
SHADOW = "plugin/assets/mix_wheel_seat_shadow.png"

LOG_W, LOG_H, SS, FRAMES = 12, 70, 3, 120
PLATE = (176, 168, 141)
EMU_PLATE = (104, 104, 104)

def minified(i):
    s = Image.open(STRIP)
    return s.crop((i * LOG_W * SS, 0, (i + 1) * LOG_W * SS, LOG_H * SS)) \
            .resize((LOG_W, LOG_H), Image.BOX)

def shadow_logical():
    s = Image.open(SHADOW)
    return s.resize((s.width // SS, s.height // SS), Image.BOX)

def emu_frame(i):
    return Image.open(REF_STRIP).convert("RGBA").crop((i * 8, 0, (i + 1) * 8, 47))

def emu_seat():
    return Image.open(REF_SEAT).convert("RGB").crop((46, 2, 78, 59))

def on_plate(frame_img, pad=(9, 6, 13, 6)):
    l, t, r, b = pad
    tile = Image.new("RGBA", (l + LOG_W + r, t + LOG_H + b), PLATE + (255,))
    sh = shadow_logical()
    tile.alpha_composite(sh, (l + LOG_W, t))
    tile.alpha_composite(frame_img, (l, t))
    return tile

def face_seat(which):
    return Image.open("trench_face_mix%s.png" % which).convert("RGBA").crop(
        (270, 222, 300, 300))

def row(images, gap, bg):
    w = sum(i.width for i in images) + gap * (len(images) + 1)
    h = max(i.height for i in images) + gap * 2
    c = Image.new("RGBA", (w, h), bg)
    x = gap
    for i in images:
        c.alpha_composite(i.convert("RGBA"), (x, gap + (h - 2 * gap - i.height) // 2))
        x += i.width + gap
    return c

def stack(rows, labels, k, bg=(26, 26, 26, 255)):
    scaled = [r.resize((r.width * k, r.height * k), Image.NEAREST) for r in rows]
    w = max(s.width for s in scaled) + 20
    h = sum(s.height + 18 for s in scaled) + 10
    c = Image.new("RGBA", (w, h), bg)
    d = ImageDraw.Draw(c)
    y = 6
    for s, lab in zip(scaled, labels):
        d.text((10, y), lab, fill=(225, 225, 225, 255))
        y += 14
        c.alpha_composite(s, (10, y))
        y += s.height + 4
    return c

def build(k, out):
    r1 = row([emu_seat().convert("RGBA")]
             + [emu_frame(i) for i in (0, 10, 20, 30, 40)], 6, EMU_PLATE + (255,))
    r2 = row([on_plate(minified(int(round(v * (FRAMES - 1)))))
              for v in (0.0, 0.25, 0.5, 0.75, 1.0)], 4, (26, 26, 26, 255))
    r3 = row([face_seat(s) for s in ("0", "50", "100")], 6, (26, 26, 26, 255))
    c = stack([r1, r2, r3],
              ["E-MU reference: 4386 seat, then 4388 frames 0/10/20/30/40",
               "recreated 12x70 on the face beige, MIX 0 / 25 / 50 / 75 / 100 %",
               "TRENCH FaceShot, real editor render, MIX 0 / 50 / 100 %"], k)
    c.convert("RGB").save(D + out)

if __name__ == "__main__":
    build(1, "contact_sheet.png")
    build(7, "contact_zoom.png")
    print("wrote", D + "contact_sheet.png", "and", D + "contact_zoom.png")
