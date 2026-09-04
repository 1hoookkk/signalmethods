import math
import os
import struct
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from p2k_zero_mask_census import ABSENT, BYTE_VAL, NAMES, RATE, geometry, row

CORNERS = ((0, 0, "FROM Q0"), (0, 2, "TO Q0"), (15, 0, "FROM Q100"), (15, 2, "TO Q100"))
SAME_NOTE_SEMITONES = 0.5


def real_roots(freq_b, rad_b):
    dm, dr = BYTE_VAL[freq_b], BYTE_VAL[rad_b]
    p, q = 4.0 * dm + dr - 2.0, 1.0 - dr
    disc = p * p - 4.0 * q
    if disc < 0.0:
        return None
    s = math.sqrt(disc)
    return sorted(((-p - s) / 2.0, (-p + s) / 2.0))


def real_shape(freq_b, rad_b):
    roots = real_roots(freq_b, rad_b)
    if roots is None:
        return "complex"
    lo, hi = roots
    at_dc = abs(hi - 1.0) < 0.05
    at_nyq = abs(lo + 1.0) < 0.05
    if at_dc and at_nyq:
        return "real DC+NYQ"
    if at_dc:
        return "real at DC (highpass edge)"
    if at_nyq:
        return "real at NYQ (lowpass edge)"
    return "real pair r=%.2f/%.2f" % (lo, hi)


def shape(section_bytes, off):
    pole_long = struct.unpack_from(">I", section_bytes, 0)[0]
    zero_long = struct.unpack_from(">I", section_bytes, 4)[0]
    if pole_long == ABSENT:
        return "ABSENT", None
    p_fb, p_rb = section_bytes[off + 1], section_bytes[off]
    pole = geometry(p_fb, p_rb)
    if pole is None:
        pole_kind = "1-POLE " + real_shape(p_fb, p_rb)
    else:
        pole_kind = "2-POLE"
    if zero_long == ABSENT:
        return pole_kind + " · no zero (RESONATOR / TILT)", None
    z_fb, z_rb = section_bytes[4 + off + 1], section_bytes[4 + off]
    zero = geometry(z_fb, z_rb)
    if zero is None:
        return pole_kind + " · zero " + real_shape(z_fb, z_rb), None
    if pole is None:
        return pole_kind + " · complex zero", None
    (p_hz, p_r, p_bw), (z_hz, z_r, z_bw) = pole, zero
    semis = 12.0 * math.log2(z_hz / p_hz)
    same_note = abs(semis) <= SAME_NOTE_SEMITONES
    same_freq_byte = z_fb == p_fb
    detail = dict(semis=semis, same_freq_byte=same_freq_byte, z_rb=z_rb, p_rb=p_rb,
                  bw_ratio=z_bw / p_bw)
    if z_rb == 0:
        return ("2P2Z · NOTCH on the note" if same_note else "2P2Z · NOTCH off the note"), detail
    if same_note:
        if z_bw > p_bw * 1.05:
            return "2P2Z · PEAK (zero on the note, wider)", detail
        if z_bw < p_bw / 1.05:
            return "2P2Z · DIP (zero on the note, tighter)", detail
        return "2P2Z · CANCEL (zero on the pole)", detail
    return ("2P2Z · zero ABOVE the note" if semis > 0 else "2P2Z · zero BELOW the note"), detail


def main():
    counts = Counter()
    by_family = defaultdict(Counter)
    by_section = defaultdict(Counter)
    offsets = defaultdict(list)
    ratios = defaultdict(list)
    byte_same = Counter()
    for f in range(33):
        for knot, off, corner in CORNERS:
            r = row(f, knot)
            for s in range(6):
                kind, detail = shape(r[s * 8:s * 8 + 8], off)
                counts[kind] += 1
                by_family[f][kind] += 1
                by_section[s][kind] += 1
                if detail is not None:
                    offsets[kind].append(detail["semis"])
                    ratios[kind].append(detail["bw_ratio"])
                    byte_same[(kind, detail["same_freq_byte"])] += 1
    total = sum(counts.values())
    print("P2K shape census - authored corners only (33 families x FROM/TO x Q0/Q100 = 132 frames x 6 stages = %d stages)" % total)
    print("shape = the zero's relation to its own pole, in the hardware bytes; 'same note' = within %.1f semitone" % SAME_NOTE_SEMITONES)
    print()
    for kind, n in counts.most_common():
        print("%5d  %5.1f%%  %s" % (n, 100.0 * n / total, kind))
        if kind in offsets and offsets[kind]:
            o = sorted(offsets[kind])
            q = sorted(ratios[kind])
            same_b = byte_same[(kind, True)]
            print("            zero-pole offset semitones: min %+.1f  median %+.1f  max %+.1f;  zero/pole bandwidth ratio median %.2f;  same freq byte %d/%d"
                  % (o[0], o[len(o) // 2], o[-1], q[len(q) // 2], same_b, len(o)))
    print()
    print("=== by stage (S1..S6) ===")
    kinds = [k for k, _ in counts.most_common()]
    for s in range(6):
        print("S%d: " % (s + 1) + "; ".join("%s %d" % (k, by_section[s][k]) for k in kinds if by_section[s][k]))
    print()
    print("=== by family (shape vocabulary each body was voiced with) ===")
    for f in range(33):
        fam = by_family[f]
        print("%02d %-18s " % (f, NAMES.get(f, "?")) + "; ".join("%s x%d" % (k, fam[k]) for k in kinds if fam[k]))


if __name__ == "__main__":
    main()
