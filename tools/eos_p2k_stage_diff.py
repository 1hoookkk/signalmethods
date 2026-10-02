import os
import struct

ROM = os.environ.get("EOS410", r"C:\Users\hooki\Downloads\emu_re_artifacts\EOS410.rom")
DLI = os.environ.get("P2K226", r"C:\Users\hooki\Downloads\emu_re_artifacts\p2k226\p2k226.dli")
rom = open(ROM, "rb").read()
dli = open(DLI, "rb").read()

BASE, ROW, FAMILY = 0x272BC, 0x1C, 0x1C0
FCONST = 0x00F200F2
ABS = 0xFFCFFFCF
M = 0xFFFFFFFF


def u32(x):
    return x & M


def s32(x):
    x &= M
    return x - (1 << 32) if x & 0x80000000 else x


def asr(x, n):
    return u32(s32(x) >> n)


def lsr(x, n):
    return u32(u32(x) >> n)


def lsl(x, n):
    return u32(x << n)


def table(addr):
    return [struct.unpack_from(">I", rom, addr + 4 * i)[0] for i in range(16)]


LUT_END = table(0x0C0A14)
LUT_POLE_EOS = table(0x0C0A54)
LUT_POLE_P2K = table(0x0C158C)


def lp_zero(index, step):
    return u32(0xFFCF0000 | ((0xFF - step * index) << 8) | min(0xCF + 4 * index, 0xE8))


def lpf(c, n, f):
    if n == 3:
        c = u32(c - asr(c, 2))
    i = (c >> 3) & 0xF
    pole = LUT_POLE_P2K[i]
    if n < 2:
        return [pole, lp_zero(i, 1), ABS, ABS, ABS, u32(f - lsr(LUT_END[i], 1))]
    if n == 2:
        return [pole, lp_zero(i, 2), pole, ABS, ABS, u32(f - LUT_END[i])]
    return [pole, lp_zero(i, 2), pole, lp_zero(i, 1), pole,
            u32(f - LUT_END[i] - lsr(LUT_END[i], 1))]


def hpf(c, n, f):
    d = asr(c, 2)
    stage = u32(0x8004FFCF - lsl(d, 8) - lsl(d, 24))
    if n > 1:
        return [ABS, 0x80048004, stage, 0x80048004, stage, f]
    return [ABS, 0x80048004, stage, ABS, ABS, f]


def eq(c, n, f):
    a = asr(u32(c - 0x40), 1)
    b = 0x77 if n < 2 else (0x91 if n == 2 else 0xA4)
    lo = lsl(u32(a + 0xDF), 8) | 0x700D7
    hi = lsl(u32(0xDF - a), 8) | 0x700D7
    return [ABS, ABS, ABS,
            u32(lsl(u32(a + b), 24) | lo),
            u32(lsl(u32(b - a), 24) | hi), f]


def bpf(c, n, f):
    if n < 3:
        d = asr(c, 1)
        stage = u32(lsl(u32(0x88 - d), 24) | lsl(u32(0xF8 - d), 8) | 0x800C8)
        if n < 2:
            return [0xFF68FFCF, 0xFF68FFCF, ABS, 0xFF04FF90, stage, f]
        return [stage, 0xFF04FF90, ABS, 0xFF04FF90, stage, f]
    stage = u32(lsl(u32(0x88 - c), 24) | lsl(u32(0xD0 - c), 8) | 0x800C8)
    return [stage, ABS, ABS, 0xFFD0FF20, ABS, f]


def phaser(c, n, f):
    d = asr(c, 1)
    d = u32(lsl(d, 8) | asr(d, 2))
    d = u32(d | lsl(d & 0xFFFF, 16))
    if n < 2:
        return [ABS, 0x309030D0, u32(0xC090C0D0 - d), 0x30503090, u32(0xC050C090 - d), f]
    return [ABS, 0x309830F2, u32(0xC098C0F2 - d), 0x303030B0, u32(0xC030C0B0 - d), f]


def bat(c, n, f):
    d = asr(c, 1)
    hi = lsl(u32(0x70 - d), 24)
    lo = lsl(u32(0xB0 - d), 8)
    return [ABS, 0x001000C0, u32(hi | lo | 0x1400C4), 0x003000E0, u32(hi | lo | 0x2C00DC), f]


def flanger(c, n, f):
    a = asr(c, 4)
    a = u32(a | lsl(a & 0xFFFF, 16))
    b = lsl(asr(c, 2), 8)
    b = u32(b | lsl(b & 0xFFFF, 16))
    return [0xFFCFFAC0, 0x240F546F, u32(a + 0x8F13C074 - b),
            0x1403444F, u32(a + 0x7803A852 - b), f]


def vocal(w0, w1, w2):
    def build(c, n, f):
        d = asr(c, 2)
        d = u32(d | lsl(d & 0xFFFF, 16))
        return [u32(d + w0), 0xFFC0FFC0, u32(d + w1), ABS, u32(d + w2), ABS]
    return build


FAMILIES = [
    ("034 smooth 2 lpf", lpf, 1), ("033 classic 4 lpf", lpf, 2),
    ("035 steeper 6 lpf", lpf, 3), ("036 shallow 2 hpf", hpf, 1),
    ("037 deeper 4 hpf", hpf, 2), ("038 band pass1 2 bpf", bpf, 1),
    ("039 band pass2 4 bpf", bpf, 2), ("040 contraband 6 bpf", bpf, 3),
    ("041 swept 1oct", eq, 1), ("042 swept 2->1oct", eq, 2),
    ("043 swept 3->1oct", eq, 3), ("044 phazeshift1", phaser, 1),
    ("045 phazeshift2", phaser, 2), ("046 blissbatz", bat, 1),
    ("047 flangerlite", flanger, 1),
    ("048 aah ay eeh", vocal(0x709570B1, 0x706F709D, 0x70547035), 1),
    ("049 ooh to aah", vocal(0x90A39099, 0x8060908A, 0x50347060), 1),
]
KNOTS = [round(k * 127 / 15) for k in range(16)]


def stored(family, knot):
    off = BASE + family * FAMILY + knot * ROW
    return list(struct.unpack_from(">6I", dli, off)), struct.unpack_from(">2H", dli, off + 24)


def main():
    total = 0
    for index, (name, build, order) in enumerate(FAMILIES):
        hits = 0
        for knot in range(16):
            words, _ = stored(index, knot)
            hits += build(KNOTS[knot], order, FCONST) == words
        total += hits
        print("%-24s %2d/16" % (name, hits))
    print("total %d/272" % total)
    return 0 if total == 272 else 1


if __name__ == "__main__":
    raise SystemExit(main())
