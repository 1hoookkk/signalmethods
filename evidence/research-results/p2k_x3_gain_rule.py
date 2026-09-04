import glob
import math
import os
import struct

import numpy as np

from p2k_hardware_bytes_to_x3 import byte_to_x3_word, hw_row, sec, x3, x3_dec

CORNER_SRC = {0: (0, 0), 1: (0, 2), 2: (15, 0), 3: (15, 2)}


def coefs(fb, rb):
    F, R = x3_dec(byte_to_x3_word(fb)), x3_dec(byte_to_x3_word(rb))
    return 4 * F + R - 2, 1 - R


def state(f, k, off):
    r = hw_row(f, k)
    out = []
    for s in range(6):
        pr, pf = r[s * 8 + off], r[s * 8 + off + 1]
        absent = struct.unpack_from(">I", r, s * 8 + 4)[0] == 0xFFCFFFCF
        out.append((pf, pr, None if absent else r[s * 8 + 4 + off + 1], None if absent else r[s * 8 + 4 + off]))
    return out


def dc_gain(secs):
    g = 1.0
    for pf, pr, zf, zr in secs:
        a1, a2 = coefs(pf, pr)
        num = 1.0
        if zf is not None:
            b1, b2 = coefs(zf, zr)
            num = 1 + b1 + b2
        g *= num / (1 + a1 + a2)
    return abs(g)


equal = []
cuts = {}
for f in range(33):
    for c, (k, off) in CORNER_SRC.items():
        gw = [sec(x3[f], c, s)[4] for s in range(6)]
        total = np.prod([x3_dec(w) * 4.0 for w in gw])
        after = 20 * math.log10(dc_gain(state(f, k, off)) * total)
        if len(set(gw)) == 1:
            equal.append(after)
        else:
            emax = max(gw) >> 12
            cuts.setdefault(f, set()).add(tuple(emax - (w >> 12) for w in gw))
equal = np.array(equal)
names = {int(os.path.basename(p)[4:7]): os.path.basename(p)[8:-4] for p in glob.glob(os.path.join(os.path.dirname(__file__), "..", "factory-data", "p2k", "bodies", "P2k_0*.bin"))}
print("X3 gain words: %d/132 corners carry one word in all six sections; cascade DC gain after those words: max |err| %.3f dB, mean %+.3f dB" % (len(equal), np.abs(equal).max(), equal.mean()))
print("  -> X3 gain = DC normalisation to 0 dB, split equally over the six sections")
print("remaining %d corners in %d bodies: the same words with the exponent nibble lowered on chosen sections (6.02 dB steps):" % (132 - len(equal), len(cuts)))
for f, pats in sorted(cuts.items()):
    print("  %-13s exponent steps down per section S1..S6: %s" % (names.get(f, f), sorted(pats)))
