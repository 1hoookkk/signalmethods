"""Build the normalized E-mu corpus tables.

One row per section occurrence, plus derived state / subset / scaffold tables.
Writes Parquet under plotdata/emu/ for the marimo notebook to read.

Binary contract: the Morpheus side is read from ref/morpheus/cubes_decoded.json,
which is the bit-verified decode of the 289-cube ROM; the P2K side is read from
the v2 architecture recipes, which round-trip the factory bytes bit-exactly.
Byte-level verification of the containers belongs in ImHex, not here.
"""
import os
import json
import glob
import math
import itertools
from collections import defaultdict, Counter

import polars as pl

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plotdata", "emu")
os.makedirs(OUT, exist_ok=True)

FS = 39062.5
NYQ = FS / 2.0
IDLE_POLE = (1909, 2015)


def dewarp(f):
    """Inverse bilinear, in Hz: f_a = (fs/pi) tan(pi f_d / fs)."""
    if f <= 0.0:
        return 0.0
    return (FS / math.pi) * math.tan(math.pi * min(f, NYQ * 0.999) / FS)


def cents(a, b):
    return 1200.0 * math.log2(max(b, 1e-9) / max(a, 1e-9))


def role_of(rp, fp, rz, fz, sentinel, idle):
    if sentinel or (rp < 0.05 and rz < 0.05):
        return "sentinel"
    if idle or (rp > 0.05 and fp >= 12000.0):
        return "air cap / pad"
    if rz >= 0.95 and 6000.0 <= fz <= 9000.0:
        return "pinna notch"
    if rp >= 0.90 and 150.0 <= fp <= 4500.0:
        return "formant peak"
    if fp < 60.0 or fz < 60.0 or rp >= 0.95:
        return "shelf / termination"
    return "other"


def state_key(fp, rp, fz, rz):
    """Corpus-agnostic state identity: de-warped roots to 2 cents, radius to
    4 decimals. Fine enough to separate authored states, coarse enough that
    the two corpora's different storage laws still land on the same key."""
    def part(f, r):
        if r <= 0.02:
            return "off"
        return f"{round(cents(20.0, dewarp(f)) / 2.0)}_{r:.4f}"
    return part(fp, rp) + "|" + part(fz, rz)


def build_rows():
    rows = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS, "morpheus datum must be the native clock"
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            for si, s in enumerate(cor["sections"]):
                w = s["raw"]
                fp, rp = s["pole"]["hz"], s["pole"]["r"]
                fz, rz = s["zero"]["hz"], s["zero"]["r"]
                sentinel = (w[1] == 2047 and w[3] == 2047)
                selfc = (w[0] == w[2] and w[1] == w[3]) and not sentinel
                idle = tuple(w[:2]) == IDLE_POLE and w[3] == 2047
                rows.append(dict(
                    corpus="morpheus", era="Morpheus 1993", filter=c["name"],
                    filter_idx=c["index"], corner=ci,
                    t2=ci & 1, m=(ci >> 1) & 1, q=(ci >> 2) & 1,
                    stage=si, lane=f"S{si+1}",
                    w0=w[0], w1=w[1], w2=w[2], w3=w[3],
                    state_exact=f"{w[0]},{w[1]},{w[2]},{w[3]}",
                    state_key=state_key(fp, rp, fz, rz),
                    pole_hz=fp, pole_r=rp, zero_hz=fz, zero_r=rz,
                    pole_hz_ac=dewarp(fp), zero_hz_ac=dewarp(fz),
                    dst=(12 * math.log2(max(dewarp(fz), 1e-9) /
                                        max(dewarp(fp), 1e-9))
                         if rz > 0.02 and rp > 0.02 else None),
                    is_sentinel=sentinel, is_selfcancel=selfc, is_idle=idle,
                    live=not (sentinel or selfc or idle),
                    role=role_of(rp, fp, rz, fz, sentinel, idle),
                ))
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS, "p2k recipes must carry the same datum"
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            for si, sec in enumerate(p["sections"]):
                g = sec["corners"][cn]

                def rd(x):
                    if "pair" in x:
                        a, b = x["pair"]
                        return (0.3 if (a + b) >= 0 else NYQ,
                                min(math.sqrt(abs(a * b)), 0.99999))
                    return (x["hz"], min(x["r"], 0.99999))
                fp, rp = rd(g["pole"])
                fz, rz = rd(g["zero"])
                sentinel = rp < 1e-6 and rz < 1e-6
                selfc = (abs(cents(fp, fz)) < 1.0 and abs(rp - rz) < 1e-4
                         and rp > 0.05)
                rows.append(dict(
                    corpus="p2k", era="P2K 1999", filter=p["name"],
                    filter_idx=p["index"], corner=ci,
                    t2=0, m=ci & 1, q=(ci >> 1) & 1,
                    stage=si, lane=f"S{si+1}",
                    w0=None, w1=None, w2=None, w3=None,
                    state_exact=f"{fp:.4f},{rp:.6f},{fz:.4f},{rz:.6f}",
                    state_key=state_key(fp, rp, fz, rz),
                    pole_hz=fp, pole_r=rp, zero_hz=fz, zero_r=rz,
                    pole_hz_ac=dewarp(fp), zero_hz_ac=dewarp(fz),
                    dst=(12 * math.log2(max(dewarp(fz), 1e-9) /
                                        max(dewarp(fp), 1e-9))
                         if rz > 0.02 and rp > 0.02 else None),
                    is_sentinel=sentinel, is_selfcancel=selfc, is_idle=False,
                    live=not (sentinel or selfc),
                    role=role_of(rp, fp, rz, fz, sentinel, False),
                ))
    return rows


def main():
    sections = pl.DataFrame(build_rows())
    sections = sections.with_columns(
        (pl.col("corpus") + ":" + pl.col("filter") + ":c" +
         pl.col("corner").cast(pl.Utf8)).alias("corner_id"))
    sections.write_parquet(os.path.join(OUT, "sections.parquet"))

    live = sections.filter(pl.col("live"))
    states = (live.group_by("state_key")
              .agg(pl.len().alias("uses"),
                   pl.col("filter").n_unique().alias("filters"),
                   pl.col("corpus").n_unique().alias("corpora"),
                   pl.col("lane").mode().first().alias("modal_lane"),
                   pl.col("pole_hz").median().alias("pole_hz"),
                   pl.col("pole_hz_ac").median().alias("pole_hz_ac"),
                   pl.col("pole_r").median().alias("pole_r"),
                   pl.col("zero_hz").median().alias("zero_hz"),
                   pl.col("zero_hz_ac").median().alias("zero_hz_ac"),
                   pl.col("zero_r").median().alias("zero_r"),
                   pl.col("role").mode().first().alias("role"))
              .sort("uses", descending=True))
    states.write_parquet(os.path.join(OUT, "states.parquet"))

    per_corner = defaultdict(dict)
    meta = {}
    for r in live.iter_rows(named=True):
        per_corner[r["corner_id"]][r["stage"]] = r["state_key"]
        meta[r["corner_id"]] = (r["corpus"], r["filter"], r["corner"])

    subsets = defaultdict(list)
    for cid, lanes in per_corner.items():
        keys = sorted(lanes)
        for n in (2, 3, 4):
            for combo in itertools.combinations(keys, n):
                sig = (tuple(combo),
                       tuple(lanes[s] for s in combo))
                subsets[sig].append(cid)
    rows = []
    for (combo, sig), cids in subsets.items():
        filters = {meta[c][1] for c in cids}
        if len(filters) < 2:
            continue
        rows.append(dict(
            lanes="+".join(f"S{s+1}" for s in combo),
            lane_mask="".join("1" if i in combo else "0" for i in range(7)),
            n_lanes=len(combo),
            contiguous=bool(max(combo) - min(combo) == len(combo) - 1),
            state_sig="|".join(sig),
            occurrences=len(cids),
            n_filters=len(filters),
            n_corpora=len({meta[c][0] for c in cids}),
            examples=", ".join(sorted(filters)[:6]),
        ))
    pl.DataFrame(rows).sort("occurrences", descending=True).write_parquet(
        os.path.join(OUT, "subsets.parquet"))

    inv = defaultdict(set)
    for cid, lanes in per_corner.items():
        for s, k in lanes.items():
            inv[(s, k)].add(cid)
    shared = Counter()
    for pair_set in inv.values():
        if 1 < len(pair_set) <= 80:
            for a, b in itertools.combinations(sorted(pair_set), 2):
                shared[(a, b)] += 1
    edges = []
    for (a, b), n in shared.items():
        la, lb = per_corner[a], per_corner[b]
        common = set(la) & set(lb)
        same = [s for s in common if la[s] == lb[s]]
        diff = [s for s in common if la[s] != lb[s]]
        only = (set(la) ^ set(lb))
        if len(same) >= 4 and (len(diff) + len(only)) <= 1 and \
                meta[a][1] != meta[b][1]:
            edges.append(dict(
                src=a, dst=b,
                src_filter=meta[a][1], dst_filter=meta[b][1],
                src_corpus=meta[a][0], dst_corpus=meta[b][0],
                shared_lanes=len(same),
                replaced_lane=(f"S{diff[0]+1}" if diff else
                               (f"S{sorted(only)[0]+1}" if only else "")),
                scaffold="+".join(f"S{s+1}" for s in sorted(same)),
            ))
    pl.DataFrame(edges).write_parquet(os.path.join(OUT, "scaffolds.parquet"))

    print(f"sections  {sections.height:7d} rows  "
          f"({live.height} live, {sections.height - live.height} inert)")
    print(f"states    {states.height:7d} distinct live states")
    print(f"          reused by >1 filter: "
          f"{states.filter(pl.col('filters') > 1).height}")
    print(f"          spanning both corpora: "
          f"{states.filter(pl.col('corpora') > 1).height}")
    print(f"subsets   {len(rows):7d} lane-combinations recurring across filters")
    print(f"scaffolds {len(edges):7d} 'N shared + 1 replaced' edges")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
