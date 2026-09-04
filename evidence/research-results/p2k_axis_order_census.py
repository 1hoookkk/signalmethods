import glob
import os
import struct
from collections import Counter, defaultdict

import numpy as np

AUD = os.path.expanduser("~/Downloads/emu_re_artifacts/audity2000/os/audity_os_1.00.bin")
T12 = 0x2F8C8
ABSENT = 0xFFCFFFCF
BODIES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "factory-data", "p2k", "bodies")
NAMES = {int(os.path.basename(p)[4:7]): os.path.basename(p)[8:-4] for p in glob.glob(os.path.join(BODIES, "P2k_0*.bin"))}
ROLES = ("pole freq", "pole radius", "zero freq", "zero radius")
PAIRS = {
    "Q pair (Morph fixed)": ((0, 0, 15, 0), (0, 2, 15, 2)),
    "Morph pair (Q fixed)": ((0, 0, 0, 2), (15, 0, 15, 2)),
    "diagonal (both change)": ((0, 0, 15, 2), (0, 2, 15, 0)),
}

a = open(AUD, "rb").read()


def row(f, k):
    return a[T12 + (f * 16 + k) * 0x34: T12 + (f * 16 + k) * 0x34 + 0x34]


def corner(f, k, off):
    r = row(f, k)
    out = {}
    for s in range(6):
        if struct.unpack_from(">I", r, s * 8)[0] != ABSENT:
            out[(s, "pole freq")] = r[s * 8 + off + 1]
            out[(s, "pole radius")] = r[s * 8 + off]
        if struct.unpack_from(">I", r, s * 8 + 4)[0] != ABSENT:
            out[(s, "zero freq")] = r[s * 8 + 4 + off + 1]
            out[(s, "zero radius")] = r[s * 8 + 4 + off]
    return out


def main():
    print("P2K axis-order census - TRENCH tool over the Audity 1.00 hardware bank (33 families x knots 0/15 x Morph ends). Byte-identity between the four authored corners.")
    same = {p: Counter() for p in PAIRS}
    total = {p: Counter() for p in PAIRS}
    uniform = {p: Counter() for p in PAIRS}
    uniform_total = {p: Counter() for p in PAIRS}
    per_family = defaultdict(dict)
    for f in range(33):
        for pname, specs in PAIRS.items():
            fam_same = fam_total = 0
            for k0, o0, k1, o1 in specs:
                c0, c1 = corner(f, k0, o0), corner(f, k1, o1)
                for key in c0:
                    if key not in c1:
                        continue
                    total[pname][key[1]] += 1
                    fam_total += 1
                    if c0[key] == c1[key]:
                        same[pname][key[1]] += 1
                        fam_same += 1
                for role in ROLES:
                    deltas = [c1[key] - c0[key] for key in c0 if key in c1 and key[1] == role]
                    if len(deltas) >= 3:
                        uniform_total[pname][role] += 1
                        if len(set(deltas)) == 1 and deltas[0] != 0:
                            uniform[pname][role] += 1
            per_family[f][pname] = 100.0 * fam_same / max(fam_total, 1)
    print("\nidentical bytes between the two corners of a pair (all 33 families, both pairs of each kind):")
    print("%-26s %s" % ("", "  ".join("%12s" % r for r in ROLES)))
    for pname in PAIRS:
        print("%-26s %s" % (pname, "  ".join("%5.0f%% (%3d)" % (100.0 * same[pname][r] / max(total[pname][r], 1), total[pname][r]) for r in ROLES)))
    print("\npairs where every live section moved by the SAME nonzero byte delta (one-dial operation fingerprint):")
    for pname in PAIRS:
        print("%-26s %s" % (pname, "  ".join("%3d of %3d" % (uniform[pname][r], uniform_total[pname][r]) for r in ROLES)))
    print("\nper family: %% identical bytes  Q-pair / Morph-pair / diagonal   (>> on one axis = that axis was the copy-and-edit)")
    q_wins = m_wins = 0
    for f in range(33):
        q, m, d = (per_family[f][p] for p in PAIRS)
        q_wins += q > m + 5
        m_wins += m > q + 5
        print("  %02d %-14s %5.0f%% / %5.0f%% / %5.0f%%  %s" % (f, NAMES.get(f, "?"), q, m, d, "Q shares more" if q > m + 5 else "Morph shares more" if m > q + 5 else "even"))
    print("\nfamilies where Q pairs share more: %d; Morph pairs share more: %d; even (within 5 points): %d" % (q_wins, m_wins, 33 - q_wins - m_wins))


if __name__ == "__main__":
    main()
