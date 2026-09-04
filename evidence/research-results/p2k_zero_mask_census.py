import glob
import math
import os
import struct
import sys
from collections import Counter

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

AUD = os.path.expanduser("~/Downloads/emu_re_artifacts/audity2000/os/audity_os_1.00.bin")
T12 = 0x2F8C8
HERE = os.path.dirname(os.path.abspath(__file__))
BODIES = os.path.join(HERE, "..", "factory-data", "p2k", "bodies")
OUT = sys.argv[1] if len(sys.argv) > 1 else HERE
RATE = 44100.0
ABSENT = 0xFFCFFFCF
FILL = {1: 0xF5, 2: 0xF9, 3: 0xFB}
ROSTER_EXCLUDED = {4, 7}
ENDS = ("M0", "M100")
TIGHT, LOOSE = 2, 8

a = open(AUD, "rb").read()
NAMES = {int(os.path.basename(p)[4:7]): os.path.basename(p)[8:-4]
         for p in glob.glob(os.path.join(BODIES, "P2k_0*.bin"))}


def row(f, k):
    o = T12 + (f * 16 + k) * 0x34
    return a[o:o + 0x34]


def byte_to_word(b):
    if b >= 0xE0:
        return 0xF000 | ((b - 0xE0) << 7) | 0x7D
    e = b >> 4
    if e >= 4:
        return ((b + 0x10) << 8) | 0xFC
    if e == 0:
        return ((2 * b + 1) << 8) | (0xF0 if b == 0 else 0xEE if b <= 5 else 0xED)
    return ((b + 0x10) << 8) | FILL[e]


def decode_word(w):
    u = w + 1
    if u >= 65536:
        return 1.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return math.ldexp(x, e - 15)


BYTE_VAL = np.array([decode_word(byte_to_word(b)) for b in range(256)])


def geometry(freq_b, rad_b):
    dm, dr = BYTE_VAL[freq_b], BYTE_VAL[rad_b]
    p, q = 4.0 * dm + dr - 2.0, 1.0 - dr
    if q <= 0.0 or p * p - 4.0 * q >= 0.0:
        return None
    r = math.sqrt(q)
    hz = math.acos(max(-1.0, min(1.0, -p / (2.0 * r)))) / (2.0 * math.pi) * RATE
    return hz, r, -math.log(r) * RATE / math.pi


def bytes_of(hz, r):
    theta = 2.0 * math.pi * hz / RATE
    p, q = -2.0 * r * math.cos(theta), r * r
    dr = 1.0 - q
    dm = (p - dr + 2.0) / 4.0
    return int(np.argmin(np.abs(BYTE_VAL - dm))), int(np.argmin(np.abs(BYTE_VAL - dr)))


DT = [("f", "i4"), ("k", "i4"), ("end", "i4"), ("s", "i4"),
      ("p_hz", "f8"), ("p_r", "f8"), ("p_bw", "f8"), ("p_fb", "i4"), ("p_rb", "i4"),
      ("z_hz", "f8"), ("z_r", "f8"), ("z_bw", "f8"), ("z_fb", "i4"), ("z_rb", "i4"),
      ("notch", "?"), ("near_s", "i4"), ("near_hz", "f8"), ("other_hz", "f8")]


def census():
    recs = []
    counts = Counter()
    roundtrip_miss = 0
    for f in range(33):
        for k in range(16):
            r = row(f, k)
            for end, off in ((0, 0), (1, 2)):
                poles = {}
                for s in range(6):
                    if struct.unpack_from(">I", r, s * 8)[0] == ABSENT:
                        counts["pole absent"] += 1
                        continue
                    fb, rb = r[s * 8 + off + 1], r[s * 8 + off]
                    g = geometry(fb, rb)
                    if g is None:
                        counts["pole real"] += 1
                        continue
                    if bytes_of(g[0], g[1]) != (fb, rb):
                        roundtrip_miss += 1
                    poles[s] = (g, fb, rb)
                for s in range(6):
                    if s not in poles:
                        continue
                    if struct.unpack_from(">I", r, s * 8 + 4)[0] == ABSENT:
                        counts["zero absent"] += 1
                        continue
                    fb, rb = r[s * 8 + 4 + off + 1], r[s * 8 + 4 + off]
                    g = geometry(fb, rb)
                    if g is None:
                        counts["zero real"] += 1
                        continue
                    if rb != 0 and bytes_of(g[0], g[1]) != (fb, rb):
                        roundtrip_miss += 1
                    (p_hz, p_r, p_bw), p_fb, p_rb = poles[s]
                    z_hz, z_r, z_bw = g
                    others = [(abs(math.log2(z_hz / poles[t][0][0])), t) for t in poles]
                    near = min(others)[1]
                    other = [t for t in poles if t != s]
                    other_hz = poles[other[(f + k + s) % len(other)]][0][0] if other else float("nan")
                    counts["notch (zero radius byte 0)" if rb == 0 else "paired"] += 1
                    recs.append((f, k, end, s, p_hz, p_r, p_bw, p_fb, p_rb,
                                 z_hz, z_r, z_bw, fb, rb, rb == 0, near, poles[near][0][0], other_hz))
    return np.array(recs, dtype=DT), counts, roundtrip_miss


def offset(rec):
    return np.log2(rec["z_hz"] / rec["p_hz"])


def bw_ratio_log2(rec):
    return np.log2(rec["z_bw"] / rec["p_bw"])


def iqr(v):
    q1, q3 = np.percentile(v, [25, 75])
    return q1, q3


def hist(values, lo, hi, step, unit):
    edges = np.arange(lo, hi + step / 2.0, step)
    counts, _ = np.histogram(np.clip(values, lo, hi - 1e-9), bins=edges)
    top = max(int(counts.max()), 1)
    for e, c in zip(edges[:-1], counts):
        print("  %+6.2f %-4s |%-50s %d" % (e, unit, "#" * int(round(50.0 * c / top)), c))


def describe(label, v):
    q1, q3 = iqr(v)
    print("  %-34s n=%4d  median %+6.2f  IQR %+6.2f..%+6.2f  (spread %.2f)" % (label, len(v), np.median(v), q1, q3, q3 - q1))


def verdict(off, br, notch):
    live = ~notch
    if live.sum() < 4:
        return "n/a (S6 notch only)"
    above = np.mean(off[live] > 0.0)
    broader = np.mean(br[live] > 0.0)
    q1, q3 = iqr(off[live])
    if above >= 0.8 and broader >= 0.8:
        return "HOLDS" if q3 - q1 <= 0.5 else "LOOSE"
    return "NO"


def fit_rule(rec, rule):
    live = ~rec["notch"]
    if rule == "oct+radius":
        return np.median(offset(rec[live])), np.median(rec["z_r"][live] - rec["p_r"][live])
    if rule == "oct+bwmul":
        return np.median(offset(rec[live])), np.median(np.log(rec["z_r"][live]) / np.log(rec["p_r"][live]))
    if rule == "bytes":
        return int(round(np.median(rec["z_fb"][live] - rec["p_fb"][live]))), int(round(np.median(rec["z_rb"][live] - rec["p_rb"][live])))
    raise ValueError(rule)


def predict(rec, rule, params):
    k, c = params
    if rule == "bytes":
        fb = np.clip(rec["p_fb"] + k, 0, 255)
        rb = np.clip(rec["p_rb"] + c, 0, 255)
        return fb, rb
    hz = rec["p_hz"] * 2.0 ** k
    r = np.clip(rec["p_r"] + c, 0.05, 0.999999) if rule == "oct+radius" else rec["p_r"] ** c
    out = [bytes_of(h, rr) for h, rr in zip(hz, r)]
    return np.array([o[0] for o in out]), np.array([o[1] for o in out])


def score(rec, fb, rb):
    live = ~rec["notch"]
    dfb = np.abs(fb - rec["z_fb"])
    drb = np.abs(rb - rec["z_rb"])
    pred_hz = np.array([geometry(x, y)[0] if geometry(x, y) else np.nan for x, y in zip(fb, rb)])
    d_oct = np.abs(np.log2(pred_hz / rec["z_hz"]))
    return dict(n=len(rec),
                exact=np.mean((dfb == 0) & (drb == 0)),
                tight=np.mean((dfb <= TIGHT) & (drb <= TIGHT)),
                loose=np.mean((dfb <= LOOSE) & (drb <= LOOSE)),
                pitch_loose=np.mean(dfb <= LOOSE),
                med_oct=np.nanmedian(d_oct[live]) if live.any() else np.nan,
                med_dfb=np.median(dfb), med_drb=np.median(drb))


def grouped_predict(rec, rule, keys):
    fb = np.zeros(len(rec), dtype=int)
    rb = np.zeros(len(rec), dtype=int)
    groups = {}
    for i, r in enumerate(rec):
        groups.setdefault(tuple(int(r[key]) for key in keys), []).append(i)
    for idx in groups.values():
        sub = rec[idx]
        fb[idx], rb[idx] = predict(sub, rule, fit_rule(sub, rule))
    return fb, rb, len(groups)


def notch_pitch_predict(rec, keys):
    fb = np.zeros(len(rec), dtype=int)
    groups = {}
    for i, r in enumerate(rec):
        groups.setdefault(tuple(int(r[key]) for key in keys), []).append(i)
    for idx in groups.values():
        sub = rec[idx]
        k = np.median(offset(sub))
        fb[idx] = [bytes_of(h, 1.0)[0] for h in sub["p_hz"] * 2.0 ** k]
    return fb, len(groups)


def pole_free_predict(rec, keys):
    fb = np.zeros(len(rec), dtype=int)
    rb = np.zeros(len(rec), dtype=int)
    groups = {}
    for i, r in enumerate(rec):
        groups.setdefault(tuple(int(r[key]) for key in keys), []).append(i)
    for idx in groups.values():
        sub = rec[idx]
        fb[idx] = int(round(np.median(sub["z_fb"])))
        rb[idx] = int(round(np.median(sub["z_rb"])))
    return fb, rb, len(groups)


def print_score(label, sc, rules=None):
    extra = "" if rules is None else "  [%d rule%s]" % (rules, "" if rules == 1 else "s")
    print("  %-46s exact %5.1f%%  tight(+-%d) %5.1f%%  loose(+-%d) %5.1f%%  pitch-only loose %5.1f%%  median |doct| %.2f  median |dfb| %2d |drb| %2d%s" % (
        label, 100 * sc["exact"], TIGHT, 100 * sc["tight"], LOOSE, 100 * sc["loose"], 100 * sc["pitch_loose"],
        sc["med_oct"], sc["med_dfb"], sc["med_drb"], extra))


def tracking(rec, axis):
    moves = []
    frozen = 0
    moved = 0
    if axis == "morph":
        keys = ("f", "k", "s")
        lo, hi = rec["end"] == 0, rec["end"] == 1
        sel = np.isin(rec["k"], (0, 15))
    else:
        keys = ("f", "end", "s")
        lo, hi = rec["k"] == 0, rec["k"] == 15
        sel = np.isin(rec["k"], (0, 15))
    index = {}
    for i in np.where(sel)[0]:
        index[(tuple(int(rec[i][key]) for key in keys), bool(hi[i]))] = i
    for (key, is_hi), i in index.items():
        if is_hi:
            continue
        j = index.get((key, True))
        if j is None:
            continue
        pm = math.log2(rec[j]["p_hz"] / rec[i]["p_hz"])
        zm = math.log2(rec[j]["z_hz"] / rec[i]["z_hz"])
        moves.append((pm, zm))
        if abs(pm) > 0.25:
            moved += 1
            frozen += rec[i]["z_fb"] == rec[j]["z_fb"] and rec[i]["z_rb"] == rec[j]["z_rb"]
    m = np.array(moves)
    d = m[:, 1] - m[:, 0]
    big = np.abs(m[:, 0]) > 0.25
    print("  %s: %d pole/zero pairs seen at both ends" % (axis, len(m)))
    print("    poles that move > 0.25 oct: %d; of those, zero moves the same (within 0.1 oct): %d (%.0f%%), zero byte-frozen: %d (%.0f%%)" % (
        big.sum(), np.sum(np.abs(d[big]) <= 0.1), 100 * np.mean(np.abs(d[big]) <= 0.1) if big.any() else 0, frozen, 100 * frozen / max(moved, 1)))
    print("    zero move vs pole move: correlation %.2f, median |difference| %.2f oct" % (np.corrcoef(m[:, 0], m[:, 1])[0, 1], np.median(np.abs(d))))


def figures(rec_e, rec_a):
    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    for col, (rec, title) in enumerate(((rec_e, "authored endpoints (knots 0 and 15)"), (rec_a, "all 16 knots (interpolated)"))):
        live = ~rec["notch"]
        axes[0, col].hist(np.clip(offset(rec[live]), -3, 4), bins=np.arange(-3, 4.01, 0.125), color="#404040")
        axes[0, col].set_xlim(-3, 4)
        axes[0, col].axvline(0, color="k", lw=1)
        axes[0, col].set_title("zero minus own pole, octaves  -  " + title, fontsize=9)
        axes[1, col].hist(np.clip(bw_ratio_log2(rec[live]), -4, 8), bins=np.arange(-4, 8.01, 0.25), color="#404040")
        axes[1, col].set_xlim(-4, 8)
        axes[1, col].axvline(0, color="k", lw=1)
        axes[1, col].set_title("log2(zero bw / pole bw)  -  " + title, fontsize=9)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "zero_mask_distributions.png"), dpi=110)
    plt.close(fig)

    fig, axes = plt.subplots(1, 2, figsize=(14, 11), sharey=True)
    colors = ["#c00000", "#e08000", "#208020", "#0060c0", "#8000c0", "#000000"]
    live = ~rec_e["notch"]
    for s in range(6):
        sel = rec_e["s"] == s
        axes[0].scatter(np.clip(offset(rec_e[sel]), -3, 4), rec_e["f"][sel] + (s - 2.5) * 0.08, s=14, color=colors[s], label="S%d" % (s + 1), marker="o" if s < 5 else "x")
        sel2 = sel & live
        axes[1].scatter(np.clip(bw_ratio_log2(rec_e[sel2]), -4, 8), rec_e["f"][sel2] + (s - 2.5) * 0.08, s=14, color=colors[s], marker="o" if s < 5 else "x")
    for ax, lo, hi, title in ((axes[0], -3, 4, "zero minus own pole (octaves), 4 authored corners per family"),
                              (axes[1], -4, 8, "log2(zero bw / pole bw), S6 unit-circle notches excluded")):
        ax.set_xlim(lo, hi)
        ax.axvline(0, color="k", lw=1)
        ax.set_title(title, fontsize=9)
        ax.grid(axis="x", lw=0.3)
    axes[0].set_yticks(range(33))
    axes[0].set_yticklabels(["%02d %s%s" % (f, NAMES.get(f, "?"), " *" if f in ROSTER_EXCLUDED else "") for f in range(33)], fontsize=8)
    axes[0].invert_yaxis()
    axes[0].legend(loc="lower right", fontsize=8)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "zero_mask_by_family.png"), dpi=110)
    plt.close(fig)


def main():
    rec, counts, roundtrip_miss = census()
    rec_e = rec[np.isin(rec["k"], (0, 15))]
    print("Audity OS 1.00 hardware bank at 0x%X: 33 families x 16 Q knots x 2 Morph ends = %d frames x 6 sections" % (T12, 33 * 16 * 2))
    print("knots 1..14 are per-byte interpolations of knots 0/15 (proven 21543/21616), so the authored data is 4 corners per family;")
    print("'endpoints' below = knots 0 and 15 (what was authored), 'all knots' = what the hardware plays.")
    print("byte -> X3 word -> validated X3 law (3168/3168). geometry -> bytes round trip misses: %d" % roundtrip_miss)
    print("section census over all 16 knots: %s" % dict(counts))
    print("paired zeros at endpoints: %d (of which S6 notches %d); over all knots: %d" % (len(rec_e), rec_e["notch"].sum(), len(rec)))
    print("families 004 meaty_gizmo and 007 fuzzi_face are in this census (marked *), excluded only from the shipping roster.")

    for rec_x, label in ((rec_e, "ENDPOINTS"), (rec, "ALL 16 KNOTS")):
        live = ~rec_x["notch"]
        print()
        print("=== %s: zero minus own-section pole, octaves (S6 notches included; they have a pitch) ===" % label)
        hist(offset(rec_x), -3.0, 4.0, 0.25, "oct")
        describe("all paired zeros", offset(rec_x))
        describe("S1-S5 + live S6", offset(rec_x[live]))
        describe("S6 notches only", offset(rec_x[~live]))
        print("=== %s: log2(zero bw / pole bw)  (>0 = zero broader than pole; notches excluded) ===" % label)
        hist(bw_ratio_log2(rec_x[live]), -4.0, 8.0, 0.5, "")
        describe("S1-S5 + live S6", bw_ratio_log2(rec_x[live]))
        describe("radius diff zero-pole", rec_x["z_r"][live] - rec_x["p_r"][live])
        print("  zero above its pole: %.0f%%   zero broader than its pole: %.0f%%   both: %.0f%%" % (
            100 * np.mean(offset(rec_x[live]) > 0), 100 * np.mean(bw_ratio_log2(rec_x[live]) > 0),
            100 * np.mean((offset(rec_x[live]) > 0) & (bw_ratio_log2(rec_x[live]) > 0))))

    print()
    print("=== per section slot (endpoints) ===")
    for s in range(6):
        sub = rec_e[rec_e["s"] == s]
        live = ~sub["notch"]
        if live.sum():
            q1, q3 = iqr(offset(sub[live]))
            b1, b3 = iqr(bw_ratio_log2(sub[live]))
            print("  S%d n=%3d (notch %3d)  offset median %+5.2f IQR %+5.2f..%+5.2f   log2 bw ratio median %+5.2f IQR %+5.2f..%+5.2f   above %3.0f%%  broader %3.0f%%" % (
                s + 1, len(sub), (~live).sum(), np.median(offset(sub[live])), q1, q3, np.median(bw_ratio_log2(sub[live])), b1, b3,
                100 * np.mean(offset(sub[live]) > 0), 100 * np.mean(bw_ratio_log2(sub[live]) > 0)))
        else:
            q1, q3 = iqr(offset(sub))
            print("  S%d n=%3d (notch %3d)  offset median %+5.2f IQR %+5.2f..%+5.2f   all unit-circle notches" % (s + 1, len(sub), (~live).sum(), np.median(offset(sub)), q1, q3))

    print()
    print("=== own pole vs nearest pole (endpoints, S1-S5 + live S6) ===")
    live = rec_e[~rec_e["notch"]]
    own_nearest = np.mean(live["near_s"] == live["s"])
    print("  own-section pole is the nearest pole (log-frequency) to the zero: %.0f%% (%d/%d)" % (100 * own_nearest, np.sum(live["near_s"] == live["s"]), len(live)))
    describe("offset vs own pole", offset(live))
    describe("offset vs nearest pole", np.log2(live["z_hz"] / live["near_hz"]))
    describe("offset vs a different section's pole", np.log2(live["z_hz"] / live["other_hz"]))
    skeletons = {}
    for r in rec_e:
        skeletons.setdefault((int(r["f"]), int(r["k"]), int(r["end"])), set()).add(float(r["p_hz"]))
    frames = sorted(skeletons)
    null = []
    for i, r in enumerate(live):
        other = frames[(i * 7919 + 13) % len(frames)]
        while other[0] == r["f"]:
            other = frames[(frames.index(other) + 1) % len(frames)]
        null.append(min(abs(math.log2(r["z_hz"] / hz)) for hz in skeletons[other]))
    describe("|offset| vs nearest pole, own frame", np.abs(np.log2(live["z_hz"] / live["near_hz"])))
    describe("|offset| vs nearest pole, unrelated family (null)", np.array(null))
    parked = np.sum(rec_e["z_hz"][rec_e["notch"]] > 20000.0)
    print("  S6 notches parked above 20 kHz (pitch offset meaningless): %d of %d" % (parked, rec_e["notch"].sum()))

    print()
    print("=== does the zero follow its pole? (byte-exact hardware ends) ===")
    tracking(rec, "morph")
    tracking(rec, "q")

    print()
    print("=== per family (endpoints). criterion: HOLDS = >=80% of live zeros above AND broader than own pole and offset IQR <= 0.5 oct; LOOSE = sign holds, IQR > 0.5; NO = sign fails ===")
    fam_rows = []
    for f in range(33):
        sub = rec_e[rec_e["f"] == f]
        off = offset(sub)
        br = np.where(sub["notch"], np.nan, np.log2(sub["z_bw"] / sub["p_bw"]))
        live_m = ~sub["notch"]
        v = verdict(off, br, sub["notch"])
        if live_m.sum():
            q1, q3 = iqr(off[live_m])
            b1, b3 = iqr(br[live_m])
            fam_rows.append((f, v))
            print("  %02d %-14s%s zeros %2d (notch %d)  offset median %+5.2f IQR %+5.2f..%+5.2f  log2 bw median %+5.2f IQR %+5.2f..%+5.2f  above %3.0f%% broader %3.0f%%  %s" % (
                f, NAMES.get(f, "?"), "*" if f in ROSTER_EXCLUDED else " ", len(sub), (~live_m).sum(), np.median(off[live_m]), q1, q3,
                np.median(br[live_m]), b1, b3, 100 * np.mean(off[live_m] > 0), 100 * np.mean(br[live_m] > 0), v))
        else:
            fam_rows.append((f, v))
            print("  %02d %-14s%s zeros %2d (notch %d)  %s" % (f, NAMES.get(f, "?"), "*" if f in ROSTER_EXCLUDED else " ", len(sub), (~live_m).sum(), v))
    tally = Counter(v for _, v in fam_rows)
    print("  tally: %s" % dict(tally))

    print()
    live_e = rec_e[~rec_e["notch"]]
    notch_e = rec_e[rec_e["notch"]]
    print("=== MASK rule test: predict the S1-S5 zero from its own pole alone (%d live zeros, authored endpoints), scored in hardware bytes ===" % len(live_e))
    print("  tolerance: tight = both bytes within +-%d (about 3/4 semitone, 9%% bandwidth); loose = within +-%d (about a quarter octave, x1.4 bandwidth)" % (TIGHT, LOOSE))
    print("  rules: oct+radius = pitch x 2^k, radius + c;  oct+bwmul = pitch x 2^k, bandwidth x m;  bytes = freq byte + k, radius byte + c")
    for rule in ("oct+radius", "oct+bwmul", "bytes"):
        params = fit_rule(live_e, rule)
        fb, rb = predict(live_e, rule, params)
        print_score("one rule for the bank  %-10s k=%s c=%s" % (rule, "%+.2f" % params[0] if rule != "bytes" else "%+d" % params[0], "%+.3f" % params[1] if rule != "bytes" else "%+d" % params[1]), score(live_e, fb, rb), 1)
    for rule in ("oct+radius", "oct+bwmul", "bytes"):
        fb, rb, n = grouped_predict(live_e, rule, ("s",))
        print_score("one rule per slot S1..S5   %-10s" % rule, score(live_e, fb, rb), n)
    for rule in ("oct+radius", "oct+bwmul", "bytes"):
        fb, rb, n = grouped_predict(live_e, rule, ("f",))
        print_score("one rule per family        %-10s" % rule, score(live_e, fb, rb), n)
    for rule in ("oct+radius", "oct+bwmul", "bytes"):
        fb, rb, n = grouped_predict(live_e, rule, ("f", "s"))
        print_score("one rule per family+slot   %-10s (4 zeros each)" % rule, score(live_e, fb, rb), n)
    print("  -- baselines that ignore the pole entirely --")
    fb, rb, n = pole_free_predict(live_e, ("s",))
    print_score("median zero per slot, pole ignored", score(live_e, fb, rb), n)
    fb, rb, n = pole_free_predict(live_e, ("f",))
    print_score("median zero per family, pole ignored", score(live_e, fb, rb), n)
    fb, rb, n = pole_free_predict(live_e, ("f", "s"))
    print_score("median zero per family+slot, pole ignored", score(live_e, fb, rb), n)
    rng = np.random.default_rng(7)
    print_score("chance: random bytes", score(live_e, rng.integers(0, 256, len(live_e)), rng.integers(0, 256, len(live_e))))
    print_score("zero := pole (k=0, same radius)", score(live_e, live_e["p_fb"], live_e["p_rb"]))

    print()
    print("=== S6 notch (%d, radius byte 0 is a known law, given for free): can the notch pitch be predicted from the S6 pole? ===" % len(notch_e))
    for keys, label in ((("s",), "one octave offset for the bank"), (("f",), "one octave offset per family")):
        fb, n = notch_pitch_predict(notch_e, keys)
        dfb = np.abs(fb - notch_e["z_fb"])
        pred_hz = np.array([geometry(x, 0)[0] for x in fb])
        d_oct = np.abs(np.log2(pred_hz / notch_e["z_hz"]))
        print("  %-34s freq byte exact %5.1f%%  within +-%d %5.1f%%  within +-%d %5.1f%%  median |doct| %.2f  [%d rule%s]" % (
            label, 100 * np.mean(dfb == 0), TIGHT, 100 * np.mean(dfb <= TIGHT), LOOSE, 100 * np.mean(dfb <= LOOSE), np.median(d_oct), n, "" if n == 1 else "s"))
    fb_m = np.zeros(len(notch_e), dtype=int)
    for f in range(33):
        sel = notch_e["f"] == f
        if sel.any():
            fb_m[sel] = int(round(np.median(notch_e["z_fb"][sel])))
    dfb = np.abs(fb_m - notch_e["z_fb"])
    print("  %-34s freq byte exact %5.1f%%  within +-%d %5.1f%%  within +-%d %5.1f%%" % ("median notch per family, pole ignored", 100 * np.mean(dfb == 0), TIGHT, 100 * np.mean(dfb <= TIGHT), LOOSE, 100 * np.mean(dfb <= LOOSE)))

    print()
    print("=== per family: best pole-only rule (per-family oct+bwmul) vs pole-ignored median, S1-S5 loose hits ===")
    fb_r, rb_r, _ = grouped_predict(live_e, "oct+bwmul", ("f",))
    fb_m, rb_m, _ = pole_free_predict(live_e, ("f",))
    for f in range(33):
        sel = live_e["f"] == f
        if not sel.any():
            continue
        sr = score(live_e[sel], fb_r[sel], rb_r[sel])
        sm = score(live_e[sel], fb_m[sel], rb_m[sel])
        print("  %02d %-14s%s  rule loose %3.0f%% tight %3.0f%%   pole-ignored loose %3.0f%%   %s" % (
            f, NAMES.get(f, "?"), "*" if f in ROSTER_EXCLUDED else " ", 100 * sr["loose"], 100 * sr["tight"], 100 * sm["loose"], dict(fam_rows)[f]))

    figures(rec_e, rec)
    print()
    print("figures: %s, %s" % (os.path.join(OUT, "zero_mask_distributions.png"), os.path.join(OUT, "zero_mask_by_family.png")))


if __name__ == "__main__":
    main()
