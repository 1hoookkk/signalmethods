"""Cumulative cascade prefixes: what does each stage DO to the running response?

For every corner:      C_0 = flat,  C_k = H_1 . H_2 ... H_k
For every transition:  C_{k-1} -> C_k, record the landmark change.
Then mine the transitions for recurrence, and compare every prefix against
the published acoustic reference (not just the finished cascade).
"""
import os
import json
import glob
import math
import numpy as np
import polars as pl
from collections import Counter, defaultdict
from scipy.signal import find_peaks

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plotdata", "emu")
os.makedirs(OUT, exist_ok=True)
FS = 39062.5
NYQ = FS / 2.0
GRID = np.geomspace(40.0, 16000.0, 2048)
LOG2F = np.log2(GRID)
W = 2.0 * math.pi * GRID / FS
E1, E2 = np.exp(-1j * W), np.exp(-2j * W)
PROM = 3.0
MATCH_ST = 3.0
BAND = (GRID >= 200.0) & (GRID <= 4000.0)
LOG = []


def say(s=""):
    print(s)
    LOG.append(s)


def sec_db(fp, rp, fz, rz):
    num = 1 - 2 * rz * math.cos(2 * math.pi * fz / FS) * E1 + rz ** 2 * E2
    den = 1 - 2 * rp * math.cos(2 * math.pi * fp / FS) * E1 + rp ** 2 * E2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


def klatt_res(f_hz, b_hz):
    c = -math.exp(-2.0 * math.pi * b_hz / FS)
    bc = 2.0 * math.exp(-math.pi * b_hz / FS) * math.cos(2.0 * math.pi * f_hz / FS)
    a = 1.0 - bc - c
    return 20.0 * np.log10(np.maximum(np.abs(a / (1.0 - bc * E1 - c * E2)), 1e-30))


VOWELS = {
    "i": ([270, 2290, 3010], [50, 90, 150]),
    "I": ([390, 1990, 2550], [50, 70, 120]),
    "E": ([530, 1840, 2480], [50, 70, 110]),
    "ae": ([660, 1720, 2410], [60, 80, 120]),
    "a": ([730, 1090, 2440], [60, 80, 120]),
    "c": ([570, 840, 2410], [60, 80, 120]),
    "U": ([440, 1020, 2240], [50, 70, 110]),
    "u": ([300, 870, 2240], [50, 70, 110]),
    "uh": ([520, 1190, 2390], [50, 70, 110]),
    "er": ([490, 1350, 1690], [50, 70, 110]),
}


def reference(v):
    F, B = VOWELS[v]
    tot = np.zeros(len(GRID))
    for f, b in zip(F, B):
        tot += klatt_res(f, b)
    n = 1
    while True:
        fh = 500.0 * (2 * n - 1)
        n += 1
        if fh >= NYQ * 0.95:
            break
        if fh <= max(F):
            continue
        tot += klatt_res(fh, min(1000.0, 200.0 + 0.12 * fh))
    tot += klatt_res(0.0, 100.0)
    return tot


REF = {v: reference(v) for v in VOWELS}


def landmarks(curve):
    pk, _ = find_peaks(curve, prominence=PROM)
    vl, _ = find_peaks(-curve, prominence=PROM)
    slope = float(np.polyfit(LOG2F, curve, 1)[0])
    return (GRID[pk], curve[pk], GRID[vl], curve[vl],
            slope, float(curve.mean()))


def match(a, b):
    """Match landmark frequency arrays; return (moved pairs, added, removed)."""
    used = set()
    moved = []
    for i, fa in enumerate(a):
        best, bd = None, MATCH_ST
        for j, fb in enumerate(b):
            if j in used:
                continue
            d = abs(12 * math.log2(max(fb, 1) / max(fa, 1)))
            if d < bd:
                bd, best = d, j
        if best is not None:
            used.add(best)
            moved.append((fa, b[best], 12 * math.log2(max(b[best], 1) /
                                                      max(fa, 1))))
    added = [b[j] for j in range(len(b)) if j not in used]
    removed = [a[i] for i in range(len(a))
               if not any(abs(m[0] - a[i]) < 1e-9 for m in moved)]
    return moved, added, removed


def bucket_slope(x):
    if x <= -3:
        return "tilt--"
    if x <= -1:
        return "tilt-"
    if x < 1:
        return "flat"
    if x < 3:
        return "tilt+"
    return "tilt++"


def bucket_lvl(x):
    if x <= -6:
        return "cut"
    if x < -1.5:
        return "duck"
    if x <= 1.5:
        return "hold"
    if x < 6:
        return "lift"
    return "boost"


def band_of(f):
    if f < 300:
        return "lo"
    if f < 1200:
        return "lomid"
    if f < 4000:
        return "mid"
    return "hi"


def load():
    out = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            secs = []
            for s in cor["sections"]:
                w = s["raw"]
                dead = (w[1] == 2047 and w[3] == 2047)
                secs.append(None if dead else
                            (s["pole"]["hz"], s["pole"]["r"],
                             s["zero"]["hz"], s["zero"]["r"]))
            if any(s is not None for s in secs):
                out.append(("morpheus", c["name"], ci, secs))
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            secs = []
            for sec in p["sections"]:
                g = sec["corners"][cn]

                def rd(x):
                    if "pair" in x:
                        a, b = x["pair"]
                        return (0.3 if (a + b) >= 0 else NYQ,
                                min(math.sqrt(abs(a * b)), 0.99999))
                    return (x["hz"], min(x["r"], 0.99999))
                fp, rp = rd(g["pole"])
                fz, rz = rd(g["zero"])
                secs.append(None if (rp < 1e-6 and rz < 1e-6) else
                            (fp, rp, fz, rz))
            if any(s is not None for s in secs):
                out.append(("p2k", p["name"], ci, secs))
    return out


def main():
    corners = load()
    say("=" * 78)
    say("CUMULATIVE CASCADE PREFIXES")
    say(f"  corners: {len(corners)}   grid: {len(GRID)} log points 40 Hz-16 kHz")
    say(f"  landmark prominence {PROM} dB, match window {MATCH_ST} st")
    say()

    trans, best_prefix, prefix_rows = [], [], []
    for corpus, name, ci, secs in corners:
        cum = np.zeros(len(GRID))
        prev = landmarks(cum)
        prefixes = [cum.copy()]
        for k, s in enumerate(secs):
            if s is None:
                prefixes.append(cum.copy())
                continue
            cum = cum + sec_db(*s)
            cur = landmarks(cum)
            pm, pa, pr = match(prev[0], cur[0])
            vm, va, vr = match(prev[2], cur[2])
            big = [m for m in pm if abs(m[2]) > 1.0]
            op = "|".join([
                f"P+{len(pa)}" if pa else "P+0",
                f"P-{len(pr)}" if pr else "P-0",
                f"V+{len(va)}" if va else "V+0",
                f"V-{len(vr)}" if vr else "V-0",
                bucket_slope(cur[4] - prev[4]),
                bucket_lvl(cur[5] - prev[5]),
                "move" if big else "still",
            ])
            trans.append(dict(
                corpus=corpus, name=name, corner=ci, stage=k + 1, op=op,
                peaks_added=len(pa), peaks_removed=len(pr),
                valleys_added=len(va), valleys_removed=len(vr),
                d_slope=cur[4] - prev[4], d_level=cur[5] - prev[5],
                moved=len(big),
                add_band=(band_of(pa[0]) if pa else ""),
                n_peaks=len(cur[0]), n_valleys=len(cur[2])))
            prev = cur
            prefixes.append(cum.copy())

        for k, pc in enumerate(prefixes):
            if k == 0:
                continue
            e = pc[BAND]
            for v, r in REF.items():
                rr = r[BAND]
                off = float(np.mean(rr - e))
                resid = (e + off) - rr
                rms = float(np.sqrt(np.mean(resid ** 2)))
                ec, rc = e - e.mean(), rr - rr.mean()
                den = float(np.sqrt((ec ** 2).sum() * (rc ** 2).sum()))
                corr = float((ec * rc).sum() / den) if den > 1e-12 else 0.0
                prefix_rows.append(dict(corpus=corpus, name=name, corner=ci,
                                        prefix=k, vowel=v, rms_db=rms,
                                        corr=corr))
    T = pl.DataFrame(trans)
    P = pl.DataFrame(prefix_rows)
    T.write_parquet(os.path.join(OUT, "prefix_transitions.parquet"))
    P.write_parquet(os.path.join(OUT, "prefix_klatt.parquet"))

    say(f"stage transitions recorded: {T.height:,}")
    say()
    say("RECURRING STAGE OPERATIONS  (top 14 of "
        f"{T['op'].n_unique()} distinct signatures)")
    say(f"  {'count':>6} {'%':>5}  {'stages':<22} operation")
    tot = T.height
    for op, n in Counter(T["op"].to_list()).most_common(14):
        sub = T.filter(pl.col("op") == op)
        st = Counter(sub["stage"].to_list())
        stag = " ".join(f"S{k}:{v}" for k, v in sorted(st.items())[:5])
        say(f"  {n:6d} {100*n/tot:5.1f}  {stag:<22} {op}")
    say()
    say("WHAT EACH STAGE POSITION TYPICALLY DOES")
    say(f"  {'stage':<6}{'n':>6}{'peaks+':>8}{'peaks-':>8}{'valleys+':>10}"
        f"{'d slope':>9}{'d level':>9}  most common op")
    for k in sorted(T["stage"].unique().to_list()):
        s = T.filter(pl.col("stage") == k)
        top = Counter(s["op"].to_list()).most_common(1)[0]
        say(f"  S{k:<5}{s.height:6d}{s['peaks_added'].mean():8.2f}"
            f"{s['peaks_removed'].mean():8.2f}{s['valleys_added'].mean():10.2f}"
            f"{s['d_slope'].mean():9.2f}{s['d_level'].mean():9.2f}  "
            f"{top[0]} ({100*top[1]/s.height:.0f}%)")
    say()
    say("ORDERED OPERATION CHAINS  (the sequence of ops within one corner)")
    chains = Counter()
    for (co, nm, cid), g in T.group_by(["corpus", "name", "corner"],
                                       maintain_order=True).__iter__():
        chains[tuple(g.sort("stage")["op"].to_list())] += 1
    say(f"  distinct chains: {len(chains)} over "
        f"{sum(chains.values())} corners")
    for ch, n in chains.most_common(5):
        say(f"    x{n:3d}  " + "  ->  ".join(ch))
    say()
    say("PREFIX vs PUBLISHED ACOUSTIC REFERENCE")
    say("  does any cumulative prefix match a vowel better than the finished "
        "cascade?")
    best = (P.sort("rms_db").group_by("corpus", "name", "corner")
            .agg(pl.col("prefix").first().alias("best_prefix"),
                 pl.col("rms_db").first().alias("best_rms"),
                 pl.col("vowel").first().alias("vowel")))
    say(f"  best-fitting prefix, over {best.height} corners:")
    for r in (best.group_by("best_prefix").agg(pl.len().alias("corners"))
              .sort("best_prefix").iter_rows(named=True)):
        say(f"     C_{r['best_prefix']}  {r['corners']:5d} corners")
    say(f"  best RMS overall: {best['best_rms'].min():.2f} dB")
    say(f"  corners whose best prefix beats their own full cascade by >2 dB: "
        f"{best.filter(pl.col('best_rms') < 0).height}")
    full_by = (P.group_by("corpus", "name", "corner")
               .agg(pl.col("prefix").max().alias("last")))
    fullrms = (P.join(full_by, on=["corpus", "name", "corner"])
               .filter(pl.col("prefix") == pl.col("last"))
               .group_by("corpus", "name", "corner")
               .agg(pl.col("rms_db").min().alias("full_rms")))
    cmp = best.join(fullrms, on=["corpus", "name", "corner"])
    gain = (cmp["full_rms"] - cmp["best_rms"])
    say(f"  median improvement from truncating the cascade: "
        f"{float(gain.median()):.2f} dB")
    say(f"  corners improved by >3 dB by truncation: "
        f"{int((gain > 3).sum())} of {cmp.height}")
    say(f"  corners where any prefix reaches <3 dB of a vowel: "
        f"{P.filter(pl.col('rms_db') < 3).height}")
    say(f"  corners where any prefix reaches <5 dB of a vowel: "
        f"{P.filter(pl.col('rms_db') < 5).select(['name','corner']).n_unique()}")
    say()
    say("  best prefix match per vowel:")
    for v in VOWELS:
        s = P.filter(pl.col("vowel") == v).sort("rms_db").head(1)
        r = s.row(0, named=True)
        say(f"    /{v:<3s} {r['rms_db']:6.2f} dB  corr {r['corr']:.3f}  "
            f"C_{r['prefix']} of {r['name']} c{r['corner']}")

    open(os.path.join(ROOT, "plots", "corpus", "prefix_ops.txt"),
         "w").write("\n".join(LOG))


if __name__ == "__main__":
    main()
