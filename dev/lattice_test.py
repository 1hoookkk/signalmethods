"""Classify each cascade's extrema spacing by measurement, never by name.

For every corner, evaluate the COMPLETE serial sub-cascade (the product of all
live sections), locate the magnitude extrema, and test two hypotheses:

    linear comb   : delta_f_n   = f_{n+1} - f_n            approximately constant
    octave lattice: delta_o_n   = log2(f_{n+1} / f_n)      approximately constant

Constancy is measured by the coefficient of variation of the spacing series.
If neither is constant the corner is reported as 'measured, unassigned' with
its actual spacing statistics.
"""
import os
import json
import glob
import math
import numpy as np
import polars as pl
from scipy.signal import find_peaks

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plotdata", "emu")
os.makedirs(OUT, exist_ok=True)

FS = 39062.5
NYQ = FS / 2.0
IDLE_POLE = (1909, 2015)

N = 16384
GRID = np.linspace(20.0, NYQ * 0.995, N)
W = 2.0 * np.pi * GRID / FS
E1 = np.exp(-1j * W)
E2 = np.exp(-2j * W)

CV_TIGHT = 0.12
MIN_EXTREMA = 4
PROM_DB = 3.0


def section_db(fp, rp, fz, rz):
    num = 1 - 2 * rz * math.cos(2 * math.pi * fz / FS) * E1 + rz ** 2 * E2
    den = 1 - 2 * rp * math.cos(2 * math.pi * fp / FS) * E1 + rp ** 2 * E2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


def cv(x):
    x = np.asarray(x, float)
    if len(x) < 2:
        return None
    m = float(np.mean(x))
    if abs(m) < 1e-12:
        return None
    return float(np.std(x) / abs(m))


def classify(freqs):
    """freqs: ascending extremum frequencies of one kind."""
    if len(freqs) < MIN_EXTREMA:
        return None
    f = np.asarray(freqs, float)
    df = np.diff(f)
    do = np.diff(np.log2(f))
    cf, co = cv(df), cv(do)
    if cf is None or co is None:
        return None
    if cf < CV_TIGHT and cf <= co:
        kind = "linear comb"
    elif co < CV_TIGHT and co < cf:
        kind = "octave lattice"
    else:
        kind = "measured, unassigned"
    return dict(n=len(f), lo=float(f[0]), hi=float(f[-1]),
                mean_df=float(np.mean(df)), cv_df=cf,
                mean_do=float(np.mean(do)), cv_do=co, kind=kind)


def load():
    out = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            secs = []
            for s in cor["sections"]:
                w = s["raw"]
                sentinel = w[1] == 2047 and w[3] == 2047
                selfc = w[0] == w[2] and w[1] == w[3]
                idle = tuple(w[:2]) == IDLE_POLE and w[3] == 2047
                if sentinel or selfc or idle:
                    continue
                secs.append((s["pole"]["hz"], s["pole"]["r"],
                             s["zero"]["hz"], s["zero"]["r"]))
            if secs:
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
                if rp < 1e-6 and rz < 1e-6:
                    continue
                secs.append((fp, rp, fz, rz))
            if secs:
                out.append(("p2k", p["name"], ci, secs))
    return out


def main():
    rows = []
    for corpus, name, ci, secs in load():
        tot = np.zeros(N)
        for fp, rp, fz, rz in secs:
            tot += section_db(fp, rp, fz, rz)
        pk, _ = find_peaks(tot, prominence=PROM_DB)
        tr, _ = find_peaks(-tot, prominence=PROM_DB)
        for kind_name, idx in (("maxima", pk), ("minima", tr)):
            r = classify(GRID[idx])
            if r is None:
                continue
            rows.append(dict(corpus=corpus, filter=name, corner=ci,
                             extremum=kind_name, **r))
    df = pl.DataFrame(rows)
    df.write_parquet(os.path.join(OUT, "lattice.parquet"))

    print(f"corners analysed: {len(load())}   spacing series with "
          f">={MIN_EXTREMA} extrema: {df.height}")
    print(f"constancy threshold: CV < {CV_TIGHT}")
    print()
    print(df.group_by("extremum", "kind").agg(pl.len().alias("n"))
          .sort("extremum", "n", descending=[False, True]))
    print()
    print("by corpus:")
    print(df.group_by("corpus", "kind").agg(pl.len().alias("n"))
          .sort("corpus", "n", descending=[False, True]))
    print()
    for k in ("linear comb", "octave lattice"):
        sub = df.filter(pl.col("kind") == k).sort("cv_df" if
                                                  k == "linear comb"
                                                  else "cv_do")
        print(f"--- {k}: {sub.height} series, tightest 8")
        for r in sub.head(8).iter_rows(named=True):
            print(f"    {r['filter'][:16]:16s} c{r['corner']} {r['extremum']:6s} "
                  f"n={r['n']:2d}  {r['lo']:7.0f}-{r['hi']:7.0f} Hz  "
                  f"df={r['mean_df']:7.1f} Hz (cv {r['cv_df']:.3f})  "
                  f"do={r['mean_do']:.3f} oct (cv {r['cv_do']:.3f})")
        print()
    print("--- names that contain 'flange'/'comb'/'phas': what they measure as")
    nm = df.filter(pl.col("filter").str.to_lowercase()
                   .str.contains("flng|flange|comb|phas"))
    print(nm.group_by("kind").agg(pl.len().alias("n")).sort("n", descending=True))
    print()
    print("--- what the named ones actually are, first 10")
    for r in nm.sort("cv_do").head(10).iter_rows(named=True):
        print(f"    {r['filter'][:16]:16s} c{r['corner']} {r['extremum']:6s} "
              f"n={r['n']:2d}  df cv {r['cv_df']:.3f}  do cv {r['cv_do']:.3f}  "
              f"-> {r['kind']}")
    print()
    print("--- do names predict the measurement?")
    named = set(nm.select("filter").to_series().to_list())
    df2 = df.with_columns(
        pl.col("filter").is_in(list(named)).alias("named_comb"))
    print(df2.group_by("named_comb", "kind").agg(pl.len().alias("n"))
          .sort("named_comb", "n", descending=[True, True]))


if __name__ == "__main__":
    main()
