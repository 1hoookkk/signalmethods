import glob
import math
import os
import struct

AUD = os.path.expanduser("~/Downloads/emu_re_artifacts/audity2000/os/audity_os_1.00.bin")
T12 = 0x2F8C8
BODIES = os.path.join(os.path.dirname(__file__), "..", "factory-data", "p2k", "bodies", "P2k_0*.bin")

a = open(AUD, "rb").read()


def hw_row(f, k):
    return a[T12 + (f * 16 + k) * 0x34: T12 + (f * 16 + k) * 0x34 + 0x34]


x3 = {}
for p in sorted(glob.glob(BODIES)):
    x3[int(os.path.basename(p)[4:7])] = struct.unpack("<120H", open(p, "rb").read())


def sec(w, c, s):
    b = (c * 6 + s) * 5
    return w[b:b + 5]


FILL = {1: 0xF5, 2: 0xF9, 3: 0xFB}


def byte_to_x3_word(b):
    if b >= 0xE0:
        return 0xF000 | ((b - 0xE0) << 7) | 0x7D
    e = b >> 4
    if e >= 4:
        return ((b + 0x10) << 8) | 0xFC
    if e == 0:
        return ((2 * b + 1) << 8) | (0xF0 if b == 0 else 0xEE if b <= 5 else 0xED)
    return ((b + 0x10) << 8) | FILL[e]


def byte_value(b):
    u = b + 1
    if b >= 0xE0:
        return 0.5 + (u - 0xE0) / 64.0
    e, m = u >> 4, u & 0xF
    if e == 0:
        return m / 8.0 * 2.0 ** -15
    return (1.0 + m / 16.0) * 2.0 ** (e - 15)


def x3_dec(w):
    u = w + 1
    if u >= 65536:
        return 1.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return math.ldexp(x, e - 15)


def check_interior():
    exact = total = 0
    per_family = []
    for f in range(33):
        b0, b15 = hw_row(f, 0), hw_row(f, 15)
        longs0 = struct.unpack(">12I", b0[:0x30])
        fe = ft = 0
        for k in range(1, 15):
            bk = hw_row(f, k)
            for j in range(0x30):
                if longs0[j // 4] == 0xFFCFFFCF:
                    continue
                p = b0[j] if b0[j] == b15[j] else math.floor(b0[j] + (b15[j] - b0[j]) * k / 15.0 + 0.5)
                fe += p == bk[j]
                ft += 1
        exact += fe
        total += ft
        per_family.append((f, fe, ft))
    print("hardware knots 1..14 = per-byte linear interpolation of knots 0 and 15, round half up: %d/%d bytes exact" % (exact, total))
    print("  families not 100%%: %s" % [(f, e, t) for f, e, t in per_family if e != t])


def check_corners():
    corner_src = {0: (0, 0), 1: (0, 2), 2: (15, 0), 3: (15, 2)}
    ok = total = 0
    empirical = {}
    conflicts = 0
    misses = []
    for f in range(33):
        for c, (k, off) in corner_src.items():
            r = hw_row(f, k)
            for s in range(6):
                w = sec(x3[f], c, s)
                absent = struct.unpack_from(">I", r, s * 8 + 4)[0] == 0xFFCFFFCF
                pairs = [(r[s * 8 + off + 1], w[2]), (r[s * 8 + off], w[3])]
                if absent:
                    total += 2
                    ok += (w[0], w[1]) == (0xDFFC, 0xFFFD)
                    ok += (w[0], w[1]) == (0xDFFC, 0xFFFD)
                else:
                    pairs += [(r[s * 8 + 4 + off + 1], w[0]), (r[s * 8 + 4 + off], w[1])]
                for b, word in pairs:
                    total += 1
                    if b in empirical and empirical[b] != word:
                        conflicts += 1
                    empirical[b] = word
                    pred = byte_to_x3_word(b)
                    if pred == word:
                        ok += 1
                    elif pred is not None:
                        misses.append((f, c, s, "%02X" % b, "%04X" % word, "%04X" % pred))
    print("X3 corner words reproduced from hardware endpoint bytes by the closed form: %d/%d" % (ok, total))
    print("  closed form misses (denormal range not formalised): %d, byte->word conflicts in the empirical map: %d over %d byte values" % (len(misses), conflicts, len(empirical)))
    worst = max(abs(math.log2(x3_dec(w) / byte_value(b))) for b, w in empirical.items())
    print("  byte_value(b) vs x3_dec(word) largest |log2 ratio| over all %d byte values: %.4f" % (len(empirical), worst))


if __name__ == "__main__":
    check_interior()
    check_corners()
