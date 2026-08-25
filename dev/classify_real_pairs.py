"""Empirical pole-type census of all 33 captured P2K 4-corner bodies.

For every stage of every corner, decode via the ARMAdillo law exactly as
stage_words_to_biquad does in trench-core/src/minifloat.rs:

    d = minifloat_decode(word)
    b0  = 4*d4
    b1  = (4*d0 + d1 - 2) * 4*d4
    b2  = (1 - d1) * 4*d4
    a1  = 4*d2 + d3 - 2
    a2  = 1 - d3

Denominator  z^2 + a1 z + a2 = 0  ->  discriminant  a1^2 - 4 a2.
disc >= 0  =>  a real-axis pole.  A body is 'Contains Real Pairs' if ANY of
its 24 stage-poles is on the real axis (including trivial origins, reported
separately).  Otherwise 'Strictly Complex Conjugate'.

Input: ref/presets/*.bin  (240 B: 4 corners x 6 stages x 5 u16 LE words)
Output: console table; writes ref/p2k_pair_classification.json
"""
import glob
import json
import os
import struct
from collections import defaultdict


MINIFLOAT_MAX = 65536.0


def minifloat_decode(word):
    u = word + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = float(u & 0xFFF)
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))


def read_body(path):
    data = open(path, "rb").read()
    assert len(data) == 240
    words = struct.unpack("<120H", data)
    corners = []
    i = 0
    for _ci in range(4):
        stages = []
        for _si in range(6):
            stages.append(words[i : i + 5])
            i += 5
        corners.append(stages)
    return corners


def stage_biquad_arma(words):
    d0, d1, d2, d3, d4 = (minifloat_decode(w) for w in words)
    c0 = 4.0 * d0 + d1
    c1 = d1
    c2 = 4.0 * d2 + d3
    c3 = d3
    c4 = 4.0 * d4
    b0 = c4
    b1 = (c0 - 2.0) * c4
    b2 = (1.0 - c1) * c4
    a1 = c2 - 2.0
    a2 = 1.0 - c3
    return b0, b1, b2, a1, a2


def classify(path):
    """Return (category, real_hits, trivial_origin_count, stage_notes)"""
    corners = read_body(path)
    real_hits = []      # (corner, stage, disc, a1, a2)
    origin = 0           # real poles exactly at z = 0 (a1==a2==0)
    for ci in range(4):
        for si in range(6):
            b0, b1, b2, a1, a2 = stage_biquad_arma(corners[ci][si])
            disc = a1 * a1 - 4.0 * a2
            if disc >= 0:
                real_hits.append((ci, si, disc, a1, a2))
                if a1 == 0.0 and a2 == 0.0:
                    origin += 1
    category = (
        "Contains Real Pairs" if real_hits else "Strictly Complex Conjugate"
    )
    return category, real_hits, origin


def main():
    bodies = sorted(glob.glob(os.path.join(os.path.dirname(__file__), "..",
                                           "ref", "presets", "*.bin")))
    rows = []
    for p in bodies:
        name = os.path.basename(p)
        cat, hits, origin = classify(p)
        rows.append(
            {
                "name": name,
                "category": cat,
                "real_poles": len(hits),
                "trivial_origins": origin,
                "hits": [(f"{ci}.{si}", round(disc, 6))
                         for ci, si, disc, _, _ in hits],
            }
        )

    print(f"{'body':34s} {'category':28s} real  trivial")
    for r in rows:
        print(f"{r['name']:34s} {r['category']:28s} {r['real_poles']:4d}  {r['trivial_origins']:3d}")

    strict = [r for r in rows if r["category"] == "Strictly Complex Conjugate"]
    real = [r for r in rows if r["category"] == "Contains Real Pairs"]
    print()
    print(f"Strictly Complex Conjugate : {len(strict)}")
    for r in strict:
        print(f"   {r['name']}")
    print(f"Contains Real Pairs        : {len(real)}")
    for r in real:
        stages = ", ".join(cs for cs, _disc in r["hits"])
        print(f"   {r['name']}  (pole stages: {stages})")

    # Write authoritative table
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ref",
                       "p2k_pair_classification.json")
    with open(out, "w", encoding="utf8") as fp:
        json.dump(rows, fp, indent=2, sort_keys=True)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()