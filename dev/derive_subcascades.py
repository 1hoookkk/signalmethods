"""Templates = recurring stage COMBINATIONS whose combined transfer function
is lower order than the stages they are built from.

A run of n stages stores order 2n in numerator and denominator. Multiply the
polynomials and cancel any root the numerator and denominator share, and the
run collapses:

    stored order 2n   ->   effective order after cancellation

A chained rung (zero of stage k on the pole of stage k+1) collapses a 4th
order pair to 2nd order exactly. Those are the templates worth naming: a
lower-order building block the factory reached for repeatedly.

Frequency is factored out -- a template is the surviving root pattern
expressed in semitones from its own lowest pole, so it can be seated anywhere.
"""
import os
import json
import glob
import math
import struct
import numpy as np
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "ref", "stage_templates.json")
FS = 39062.5
NYQ = FS / 2.0
IDENTITY = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
CANCEL_TOL = 1e-3          # root distance in the z plane
MAXRUN = 4


def conj_poly(hz, r):
    w = 2.0 * math.pi * hz / FS
    return np.array([1.0, -2.0 * r * math.cos(w), r * r])


def real_poly(a, b):
    return np.array([1.0, -(a + b), a * b])


def decode_u16(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))


def kernel_biquad(words):
    d = [decode_u16(x) for x in words]
    k = (4.0 * d[0] + d[1], d[1], 4.0 * d[2] + d[3], d[3], 4.0 * d[4])
    return (np.array([k[4], (k[0] - 2.0) * k[4], (1.0 - k[1]) * k[4]]),
            np.array([1.0, k[2] - 2.0, 1.0 - k[3]]))


def roots_of(poly):
    p = np.trim_zeros(np.asarray(poly, float), "f")
    if len(p) <= 1:
        return np.array([], dtype=complex)
    return np.roots(p)


def cancel(nr, dr, tol=CANCEL_TOL):
    """Remove numerator/denominator roots that coincide."""
    nr, dr = list(nr), list(dr)
    keep_n, used = [], [False] * len(dr)
    for z in nr:
        hit = -1
        best = tol
        for j, p in enumerate(dr):
            if used[j]:
                continue
            d = abs(z - p)
            if d < best:
                best, hit = d, j
        if hit >= 0:
            used[hit] = True
        else:
            keep_n.append(z)
    keep_d = [p for j, p in enumerate(dr) if not used[j]]
    return keep_n, keep_d


def z_to_hz_r(z):
    r = abs(z)
    hz = abs(math.atan2(z.imag, z.real)) / (2 * math.pi) * FS
    return hz, r


def q_band(r):
    rp = 60.0 if r >= 1 else (0.0 if r <= 0 else
                              min(60.0, 20 * math.log10(1 / (1 - r))))
    if rp < 2:
        return "flat"
    if rp < 12:
        return "broad"
    if rp < 26:
        return "tight"
    if rp < 45:
        return "sharp"
    return "extreme"


def st_bucket(d):
    a = abs(d)
    if a < 1:
        return 0
    q = round(d / 3.0) * 3
    return int(q)


def signature(pol, zer):
    """Frequency-free description of the surviving roots."""
    if not pol:
        base = min((z_to_hz_r(z)[0] for z in zer), default=1.0) or 1.0
    else:
        base = min(z_to_hz_r(p)[0] for p in pol) or 1.0
    parts = []
    for tag, lst in (("P", pol), ("Z", zer)):
        for z in sorted(lst, key=lambda w: z_to_hz_r(w)[0]):
            hz, r = z_to_hz_r(z)
            st = 12 * math.log2(max(hz, 1e-6) / max(base, 1e-6))
            parts.append(f"{tag}{st_bucket(st):+d}:{q_band(r)}")
    return " ".join(parts), base


def corners():
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            st = []
            for s in cor["sections"]:
                p, z = s["pole"], s["zero"]
                st.append((conj_poly(z["hz"], z["r"]), conj_poly(p["hz"], p["r"])))
            yield "morpheus", c["name"], ci, st
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            st = []
            for sec in p["sections"]:
                g = sec["corners"][cn]

                def poly(x):
                    return (real_poly(*x["pair"]) if "pair" in x
                            else conj_poly(x["hz"], x["r"]))
                sc = float(g.get("scale", 1.0))
                st.append((sc * poly(g["zero"]), poly(g["pole"])))
            yield "p2k", p["name"], ci, st
    seen = set()
    for pat in ("ref/presets/*.bin", "ref/cubes/*.body",
                "recipes/hero/*.body", "recipes/extrusions/*.body"):
        for fn in sorted(glob.glob(os.path.join(ROOT, pat))):
            raw = open(fn, "rb").read()
            if raw in seen or len(raw) not in (240, 560):
                continue
            seen.add(raw)
            nc, ns = (8, 7) if len(raw) == 560 else (4, 6)
            w = struct.unpack(f"<{len(raw)//2}H", raw)
            nm = os.path.splitext(os.path.basename(fn))[0]
            for c in range(nc):
                ws = [tuple(w[(c * ns + s) * 5:(c * ns + s) * 5 + 5])
                      for s in range(ns)]
                if all(x == IDENTITY for x in ws):
                    continue
                yield "native", nm, c, [kernel_biquad(x) for x in ws]


def main():
    tmpl = defaultdict(lambda: dict(occ=0, filters=set(), runs=Counter(),
                                    base=[], stored=0, eff=0, corp=Counter()))
    runs_seen = 0
    for src, name, ci, stages in corners():
        n = len(stages)
        for L in range(2, MAXRUN + 1):
            for i in range(n - L + 1):
                grp = stages[i:i + L]
                if all(abs(b[0] - 1) < 1e-12 and abs(b[1]) < 1e-12
                       and abs(a[1]) < 1e-12 for b, a in grp):
                    continue
                num = np.array([1.0])
                den = np.array([1.0])
                for b, a in grp:
                    num = np.convolve(num, b)
                    den = np.convolve(den, a)
                nz, dp = cancel(roots_of(num), roots_of(den))
                stored = 2 * L
                eff = max(len(nz), len(dp))
                if eff >= stored:
                    continue                      # nothing collapsed
                sig, base = signature(dp, nz)
                key = (L, stored, eff, sig)
                t = tmpl[key]
                t["occ"] += 1
                t["filters"].add(name)
                t["runs"][f"S{i+1}-S{i+L}"] += 1
                t["base"].append(base)
                t["corp"][src] += 1
                runs_seen += 1

    rows = []
    for (L, stored, eff, sig), t in sorted(tmpl.items(),
                                           key=lambda kv: -kv[1]["occ"]):
        b = sorted(t["base"])
        rows.append({
            "stages": L,
            "stored_order": stored,
            "effective_order": eff,
            "collapse": f"{stored} -> {eff}",
            "signature": sig,
            "occurrences": t["occ"],
            "distinct_filters": len(t["filters"]),
            "by_corpus": dict(t["corp"]),
            "runs": dict(t["runs"].most_common(4)),
            "seat_frequency_hz": [round(b[0], 1), round(b[len(b) // 2], 1),
                                  round(b[-1], 1)],
            "examples": sorted(t["filters"])[:5],
        })
    json.dump({"contract": "templates are recurring stage COMBINATIONS whose "
                           "combined transfer function collapses below the "
                           "order the stages store, after cancelling roots "
                           "shared between numerator and denominator. "
                           "Frequency is free: the signature places surviving "
                           "roots in semitones from the run's lowest pole.",
               "generated_by": "dev/derive_subcascades.py",
               "datum_sr_hz": FS, "cancel_tolerance_z": CANCEL_TOL,
               "templates": rows},
              open(OUT, "w", encoding="utf-8"), indent=1)

    print(f"collapsing runs found: {runs_seen}   distinct templates: {len(rows)}")
    print(f"wrote {OUT}")
    print()
    print(f"{'collapse':>10}{'occ':>7}{'filters':>9}  {'runs':<22} signature")
    for r in rows[:22]:
        run = ",".join(list(r["runs"])[:3])
        print(f"{r['collapse']:>10}{r['occurrences']:7d}{r['distinct_filters']:9d}"
              f"  {run:<22} {r['signature'][:52]}")
    print()
    agg = Counter()
    for r in rows:
        agg[r["collapse"]] += r["occurrences"]
    print("collapse totals:")
    for k, v in sorted(agg.items(), key=lambda t: -t[1]):
        print(f"  {k:>10}  {v:6d} runs")


if __name__ == "__main__":
    main()
