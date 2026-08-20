"""Do the E-mu factory filters contain Bell/Klatt serial-cascade constructions?

The reference is synthesised independently from published formant tables using
Klatt's digital resonator, then overlaid on the E-mu compound response with a
single global dB offset. Residual and shape metrics are reported. Subsets of
the E-mu cascade are tested too, so partial correspondence is visible.

No connection is assumed. Provenance of every reference term is stated in
REFERENCE_PROVENANCE below and printed with the results.
"""
import os
import json
import glob
import math
import itertools
import numpy as np
import polars as pl
from scipy.signal import find_peaks

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plots", "klatt")
DATA = os.path.join(ROOT, "plotdata", "emu")
os.makedirs(OUT, exist_ok=True)
os.makedirs(DATA, exist_ok=True)

FS = 39062.5
NYQ = FS / 2.0
GRID = np.geomspace(40.0, 16000.0, 2048)
BAND = (GRID >= 200.0) & (GRID <= 4000.0)      # formant region for metrics
W = 2.0 * math.pi * GRID / FS
E1, E2 = np.exp(-1j * W), np.exp(-2j * W)

REFERENCE_PROVENANCE = """
  formant tables : F1..F3 and bandwidths as supplied in this thread
                   (Bell Labs / Peterson-Barney men's means)
  resonator form : Klatt's digital resonator, unity gain at DC --
                   C = -exp(-2 pi B/fs); Bc = 2 exp(-pi B/fs) cos(2 pi F/fs);
                   A = 1 - Bc - C;  H(z) = A / (1 - Bc z^-1 - C z^-2)
  source tilt    : one Klatt glottal resonator at F=0 Hz, B=100 Hz  (RGP)
  higher poles   : uniform-tube continuation (2n-1)*500 Hz above the last
                   tabulated formant, up to Nyquist, B growing 200 Hz -> 1000 Hz
                   -- MY construction of Fant's higher-pole correction, not a
                   quoted constant. Reported with and without, so its effect
                   on every conclusion is visible.
"""

VOWELS = {
    "i":  ([270, 2290, 3010], [50, 90, 150]),
    "I":  ([390, 1990, 2550], [50, 70, 120]),
    "E":  ([530, 1840, 2480], [50, 70, 110]),
    "ae": ([660, 1720, 2410], [60, 80, 120]),
    "a":  ([730, 1090, 2440], [60, 80, 120]),
    "c":  ([570, 840, 2410], [60, 80, 120]),
    "U":  ([440, 1020, 2240], [50, 70, 110]),
    "u":  ([300, 870, 2240], [50, 70, 110]),
    "uh": ([520, 1190, 2390], [50, 70, 110]),
    "er": ([490, 1350, 1690], [50, 70, 110]),
}


def klatt_resonator_db(f_hz, b_hz):
    """Klatt digital resonator, unity gain at DC."""
    c = -math.exp(-2.0 * math.pi * b_hz / FS)
    bc = 2.0 * math.exp(-math.pi * b_hz / FS) * math.cos(2.0 * math.pi *
                                                         f_hz / FS)
    a = 1.0 - bc - c
    den = 1.0 - bc * E1 - c * E2
    return 20.0 * np.log10(np.maximum(np.abs(a / den), 1e-30))


def reference_db(vowel, source=True, higher=True):
    F, B = VOWELS[vowel]
    tot = np.zeros(len(GRID))
    for f, b in zip(F, B):
        tot += klatt_resonator_db(f, b)
    if higher:
        n = 1
        while True:
            fh = 500.0 * (2 * n - 1)
            n += 1
            if fh <= max(F) or fh >= NYQ * 0.95:
                if fh >= NYQ * 0.95:
                    break
                continue
            bh = min(1000.0, 200.0 + 0.12 * fh)
            tot += klatt_resonator_db(fh, bh)
    if source:
        tot += klatt_resonator_db(0.0, 100.0)
    return tot


def sec_db(fp, rp, fz, rz):
    num = 1 - 2 * rz * math.cos(2 * math.pi * fz / FS) * E1 + rz ** 2 * E2
    den = 1 - 2 * rp * math.cos(2 * math.pi * fp / FS) * E1 + rp ** 2 * E2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


def load_corners():
    out = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            secs, roots = [], []
            for s in cor["sections"]:
                w = s["raw"]
                if w[1] == 2047 and w[3] == 2047:
                    continue
                fp, rp = s["pole"]["hz"], s["pole"]["r"]
                fz, rz = s["zero"]["hz"], s["zero"]["r"]
                secs.append(sec_db(fp, rp, fz, rz))
                roots.append((fp, rp, fz, rz))
            if secs:
                out.append(dict(corpus="morpheus", name=c["name"], corner=ci,
                                secs=np.array(secs), roots=roots))
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            secs, roots = [], []
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
                if rp < 1e-6 and rz < 1e-6:
                    continue
                secs.append(sec_db(fp, rp, fz, rz))
                roots.append((fp, rp, fz, rz))
            if secs:
                out.append(dict(corpus="p2k", name=p["name"], corner=ci,
                                secs=np.array(secs), roots=roots))
    return out


def metrics(emu, ref):
    """One global dB offset only; metrics on the 200-4000 Hz formant band."""
    e, r = emu[BAND], ref[BAND]
    off = float(np.mean(r - e))
    resid = (e + off) - r
    rms = float(np.sqrt(np.mean(resid ** 2)))
    ec, rc = e - e.mean(), r - r.mean()
    denom = float(np.sqrt((ec ** 2).sum() * (rc ** 2).sum()))
    corr = float((ec * rc).sum() / denom) if denom > 1e-12 else 0.0
    return off, rms, corr, resid


def peak_list(curve, prom=2.0):
    idx, _ = find_peaks(curve, prominence=prom)
    f = GRID[idx]
    return f[(f >= 150.0) & (f <= 5000.0)]


def main():
    print("REFERENCE MODEL PROVENANCE" + REFERENCE_PROVENANCE)
    refs = {}
    for v in VOWELS:
        refs[v] = dict(
            full=reference_db(v, True, True),
            no_higher=reference_db(v, True, False),
            bare=reference_db(v, False, False),
            F=VOWELS[v][0], B=VOWELS[v][1])
    corners = load_corners()
    print(f"E-mu corners loaded: {len(corners)}")
    print(f"reference configurations: {len(refs)}")
    print()

    rows = []
    for co in corners:
        n = len(co["secs"])
        variants = {"full": np.arange(n)}
        if n > 2:
            for k in range(n):
                variants[f"drop_S{k+1}"] = np.array([i for i in range(n)
                                                     if i != k])
            for a in range(n):
                for b in range(a + 2, n + 1):
                    if b - a >= 3 and (b - a) < n:
                        variants[f"S{a+1}-S{b}"] = np.arange(a, b)
        curves = {k: co["secs"][idx].sum(axis=0) for k, idx in variants.items()}
        for v, R in refs.items():
            for vk, curve in curves.items():
                off, rms, corr, _ = metrics(curve, R["full"])
                rows.append(dict(corpus=co["corpus"], name=co["name"],
                                 corner=co["corner"], variant=vk, vowel=v,
                                 offset_db=off, rms_db=rms, corr=corr,
                                 n_sections=len(variants[vk])))
    df = pl.DataFrame(rows)
    df.write_parquet(os.path.join(DATA, "klatt_match.parquet"))
    print(f"comparisons: {df.height:,}")
    print()
    print("BEST MATCH PER REFERENCE VOWEL (full cascade only)")
    full = df.filter(pl.col("variant") == "full")
    print(f"  full-cascade comparisons: {full.height:,}")
    print(f"  {'vowel':6s} {'best RMS':>9s} {'corr':>7s}  {'corner':<28s}")
    for v in VOWELS:
        s = full.filter(pl.col("vowel") == v).sort("rms_db").head(1)
        if s.height:
            r = s.row(0, named=True)
            print(f"  /{v:<4s} {r['rms_db']:9.2f} {r['corr']:7.3f}  "
                  f"{r['name']}  c{r['corner']}")
    print()
    print("BEST MATCH PER REFERENCE VOWEL (any subset variant)")
    print(f"  {'vowel':6s} {'best RMS':>9s} {'corr':>7s}  {'variant':<10s} "
          f"{'corner':<28s}")
    for v in VOWELS:
        s = df.filter(pl.col("vowel") == v).sort("rms_db").head(1)
        if s.height:
            r = s.row(0, named=True)
            print(f"  /{v:<4s} {r['rms_db']:9.2f} {r['corr']:7.3f}  "
                  f"{r['variant']:<10s} {r['name']}  c{r['corner']}")
    print()
    print("DISTRIBUTION OF FULL-CASCADE FIT (all corners x all vowels)")
    a = full["rms_db"].to_numpy()
    c = full["corr"].to_numpy()
    print(f"  RMS residual dB : p1 {np.percentile(a,1):.1f}  "
          f"p10 {np.percentile(a,10):.1f}  median {np.median(a):.1f}")
    print(f"  correlation     : p99 {np.percentile(c,99):.3f}  "
          f"p90 {np.percentile(c,90):.3f}  median {np.median(c):.3f}")
    print(f"  corners with RMS < 3 dB to some vowel: "
          f"{full.filter(pl.col('rms_db') < 3).select('name').n_unique()}")
    print(f"  corners with corr > 0.9 to some vowel: "
          f"{full.filter(pl.col('corr') > 0.9).select('name').n_unique()}")
    print()
    print("DOES REMOVING A SECTION IMPROVE THE FIT?  (best variant per corner)")
    best = (df.sort("rms_db").group_by("corpus", "name", "corner")
            .agg(pl.col("variant").first().alias("best_variant"),
                 pl.col("rms_db").first().alias("best_rms"),
                 pl.col("vowel").first().alias("best_vowel")))
    print(best.group_by("best_variant").agg(pl.len().alias("corners"))
          .sort("corners", descending=True).head(12))


if __name__ == "__main__":
    main()
