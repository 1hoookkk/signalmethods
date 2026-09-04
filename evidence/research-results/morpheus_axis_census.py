import math
import struct
import sys

import numpy as np

STREAM = r"C:\Users\hooki\trench-native\evidence\factory-data\morpheus\raw\records_stream.bin"
RATE = 39062.5
ANGLE_K = struct.unpack("<f", struct.pack("<I", 0x32C90FDB))[0]
RADIUS_K = struct.unpack("<f", struct.pack("<I", 0x32800800))[0]
GAIN_K = struct.unpack("<f", struct.pack("<I", 0x338007FF))[0]
AXES = {"Transform": 1, "Frequency": 2, "Morph": 4}
PITCH_TOL = 20.0
WIDTH_TOL = math.log2(1.25)
DEPTH_TOL = 0.01


def fields(block):
    bits = "".join(format(w, "032b") for w in struct.unpack("<%dI" % (len(block) // 4), block))
    return [int(bits[i:i + 11], 2) for i in range(0, len(bits) - 10, 11)]


def code15(f11):
    return (f11 << 4) | 0xF


def angle_hz(f11):
    c = code15(f11)
    e, m = c >> 11, c & 0x7FF
    return ((m | 0x800) << e) * ANGLE_K / (2.0 * math.pi) * RATE


def radius(f11):
    if f11 == 0:
        return 1.0
    c = code15(f11)
    e, m = c >> 11, c & 0x7FF
    return 1.0 - ((m | ((e != 0) << 11)) << (e - (e != 0))) * RADIUS_K


def gain(f11):
    c = code15(f11)
    e, m = c >> 11, c & 0x7FF
    return ((m | ((e != 0) << 11)) << (e - (e != 0))) * GAIN_K


def read_cubes():
    b = open(STREAM, "rb").read()
    cubes = []
    for k in range(289):
        off = 20 + k * 332
        payload = b[off:off + 320]
        name = b[off + 320:off + 332].decode("ascii", "replace").strip()
        secs = []
        for s in range(7):
            f = fields(payload[s * 44:(s + 1) * 44])
            secs.append([(angle_hz(f[c]), radius(f[8 + c]), angle_hz(f[16 + c]), radius(f[24 + c]),
                          f[c], f[8 + c], f[16 + c], f[24 + c]) for c in range(8)])
        g = fields(payload[308:320])[:8]
        cubes.append((k, name, secs, [gain(x) for x in g]))
    return cubes


def bw_hz(r):
    return -math.log(max(r, 1e-9)) * RATE / math.pi


def live(hz, r):
    return r > 0.5 and 20.0 < hz < 19000.0


def is_identity(sec):
    return sec[4:6] == sec[6:8]


def main():
    cubes = read_cubes()
    print("Morpheus axis census - a TRENCH tool reading Morpheus evidence; 289 cubes from records_stream.bin, decoded at %.1f Hz. Not a statement of the E-mu method." % RATE)
    k, name, secs, g = cubes[0]
    ident = all(is_identity(s[c]) for s in secs for c in range(8))
    print("record 0 = %r, all sections identity: %s, gain codes %s" % (name, ident, sorted(set(round(x, 4) for x in g))))
    k, name, secs, g = cubes[22]
    print("record 22 = %r corner 0 live poles: %s Hz" % (
        name, [round(s[0][0]) for s in secs if not is_identity(s[0]) and s[0][1] > 0.5]))
    for axis, bit in AXES.items():
        rows = []
        pairs_live = 0
        pairs_to_identity = 0
        cubes_live = 0
        switched = 0
        for k, name, secs, g in cubes:
            cube_live = False
            for a in range(8):
                if a & bit:
                    continue
                bb = a | bit
                moved = any(secs[s][a][4:] != secs[s][bb][4:] for s in range(7))
                if not moved:
                    continue
                if all(is_identity(secs[s][a]) for s in range(7)) or all(is_identity(secs[s][bb]) for s in range(7)):
                    pairs_to_identity += 1
                    continue
                pairs_live += 1
                cube_live = True
                gdb = 20.0 * math.log10(g[bb] / g[a]) if g[a] > 0 and g[bb] > 0 else 0.0
                for s in range(7):
                    ph0, pr0, zh0, zr0 = secs[s][a][:4]
                    ph1, pr1, zh1, zr1 = secs[s][bb][:4]
                    i0, i1 = is_identity(secs[s][a]), is_identity(secs[s][bb])
                    if i0 and i1:
                        continue
                    if i0 != i1:
                        switched += 1
                        continue
                    pl = live(ph0, pr0) and live(ph1, pr1)
                    zl = live(zh0, zr0) and live(zh1, zr1)
                    rows.append((k, a, s,
                                 1200.0 * math.log2(ph1 / ph0) if pl else np.nan,
                                 math.log2(bw_hz(pr1) / bw_hz(pr0)) if pl else np.nan,
                                 1200.0 * math.log2(zh1 / zh0) if zl else np.nan,
                                 zr1 - zr0 if zl else np.nan,
                                 gdb, pl, zl, ph0))
            cubes_live += cube_live
        r = np.array(rows, dtype=[("k", int), ("a", int), ("s", int), ("pc", float), ("pw", float),
                                  ("zc", float), ("zd", float), ("g", float), ("pl", bool), ("zl", bool), ("hz", float)])
        print("\n=== %s axis: %d cubes live, %d corner pairs (%d more go to a flat identity corner, skipped), %d section moves, %d sections switched on/off" % (
            axis, cubes_live, pairs_live, pairs_to_identity, len(r), switched))
        p = r[r["pl"]]
        pc, pw = np.abs(p["pc"]), np.abs(p["pw"])
        pitch = pc > PITCH_TOL
        width = pw > WIDTH_TOL
        print("poles (%d live): pitch-only %.0f%%  width-only %.0f%%  both %.0f%%  neither %.0f%%" % (
            len(p), 100 * np.mean(pitch & ~width), 100 * np.mean(~pitch & width),
            100 * np.mean(pitch & width), 100 * np.mean(~pitch & ~width)))
        mv = p[pitch]
        print("  pole pitch move where it moves: median %.0f c, IQR %.0f..%.0f c; width ratio on those: median x%.2f, IQR x%.2f..x%.2f (Hz)" % (
            np.median(np.abs(mv["pc"])), *np.percentile(np.abs(mv["pc"]), [25, 75]),
            2 ** np.median(mv["pw"]), 2 ** np.percentile(mv["pw"], 25), 2 ** np.percentile(mv["pw"], 75)))
        if len(mv) > 5:
            c = np.corrcoef(mv["pc"] / 1200.0, mv["pw"])[0, 1]
            slope = np.polyfit(mv["pc"] / 1200.0, mv["pw"], 1)[0]
            print("  width-vs-pitch on moving poles: r=%.2f slope %.2f oct/oct (0 = Hz held, 1 = Q held)" % (c, slope))
        z = r[r["zl"]]
        zc, zd = np.abs(z["zc"]), np.abs(z["zd"])
        zpitch = zc > PITCH_TOL
        zdepth = zd > DEPTH_TOL
        print("zeros (%d live): pitch-only %.0f%%  depth-only %.0f%%  both %.0f%%  neither %.0f%%" % (
            len(z), 100 * np.mean(zpitch & ~zdepth), 100 * np.mean(~zpitch & zdepth),
            100 * np.mean(zpitch & zdepth), 100 * np.mean(~zpitch & ~zdepth)))
        if zdepth.any():
            print("  zero radius change where it changes: median %+.3f (sign: + = deeper)" % np.median(z["zd"][zdepth]))
        per_pair = {}
        for row in p[pitch]:
            per_pair.setdefault((row["k"], row["a"]), []).append(row["pc"])
        spreads = [np.std(v) for v in per_pair.values() if len(v) >= 2]
        means = [np.mean(v) for v in per_pair.values() if len(v) >= 2]
        if spreads:
            print("  per corner pair (>=2 moving poles): shift median %+.0f c, IQR %+.0f..%+.0f c; spread across sections median %.0f c, %.0f%% of pairs within 50 c (transpose-like)" % (
                np.median(means), *np.percentile(means, [25, 75]), np.median(spreads), 100 * np.mean(np.array(spreads) < 50)))
        gp = {}
        for row in r:
            gp[(row["k"], row["a"])] = row["g"]
        gv = np.abs(np.array(list(gp.values())))
        print("gain: median |change| %.2f dB, %.0f%% of pairs change > 1 dB" % (np.median(gv), 100 * np.mean(gv > 1.0)))
        both = r[r["pl"] & r["zl"]]
        if len(both):
            follow = (np.abs(both["pc"]) > PITCH_TOL) & (np.abs(both["zc"]) > PITCH_TOL)
            print("sections with live pole and zero: %d; pole and zero both move pitch %.0f%%; pole moves, zero frozen %.0f%%; corr(pole move, zero move) r=%.2f" % (
                len(both), 100 * np.mean(follow), 100 * np.mean((np.abs(both["pc"]) > PITCH_TOL) & (np.abs(both["zc"]) <= PITCH_TOL)),
                np.corrcoef(both["pc"], both["zc"])[0, 1]))
        by_rank = {}
        for key in set(zip(p["k"], p["a"])):
            sel = p[(p["k"] == key[0]) & (p["a"] == key[1])]
            order = np.argsort(sel["hz"])
            n = len(order)
            for rank, idx in enumerate(order):
                by_rank.setdefault(("low" if rank < n / 2 else "high"), []).append(abs(sel["pc"][idx]))
        print("  |pitch move| by pole position in its corner: lower half median %.0f c (%.0f%% move), upper half median %.0f c (%.0f%% move)" % (
            np.median(by_rank["low"]), 100 * np.mean(np.array(by_rank["low"]) > PITCH_TOL),
            np.median(by_rank["high"]), 100 * np.mean(np.array(by_rank["high"]) > PITCH_TOL)))


if __name__ == "__main__":
    main()
