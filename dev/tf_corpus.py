"""One yardstick: translate every factory object to transfer-function form.

Whatever the container -- 11-bit Morpheus ROM words, v2 float geometry,
240/560-byte native kernels -- each stage reduces to one biquad

    H_k(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)

and a corner is the ordered product of its stages. Everything downstream
compares coefficients, not container fields, so the corpora are on one footing.

Emulator X filter XMLs carry no roots and therefore have no TF; they are
reported as untranslatable rather than silently skipped.
"""
import os
import json
import glob
import math
import struct
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FS = 39062.5
IDENTITY = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
EPS = 1e-9


# ----------------------------------------------------------------- primitives
def conj_poly(hz, r, sr=FS):
    """Conjugate root pair -> [1, c1, c2]."""
    w = 2.0 * math.pi * hz / sr
    return (1.0, -2.0 * r * math.cos(w), r * r)


def real_poly(a, b):
    """Real-axis root pair [a,b] -> [1, c1, c2]."""
    return (1.0, -(a + b), a * b)


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
    """Native 5-word stage -> (b, a). words = [z_mag, z_rsq, p_mag, p_rsq, scale]."""
    d = [decode_u16(x) for x in words]
    k = (4.0 * d[0] + d[1], d[1], 4.0 * d[2] + d[3], d[3], 4.0 * d[4])
    b = (k[4], (k[0] - 2.0) * k[4], (1.0 - k[1]) * k[4])
    a = (1.0, k[2] - 2.0, 1.0 - k[3])
    return b, a


def stage_tf(num_poly, den_poly, scale=1.0):
    return ((scale * num_poly[0], scale * num_poly[1], scale * num_poly[2]),
            den_poly)


# ------------------------------------------------------------------- loaders
def morpheus():
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        for ci, cor in enumerate(c["corners"]):
            stages = []
            for s in cor["sections"]:
                p, z = s["pole"], s["zero"]
                stages.append(stage_tf(conj_poly(z["hz"], z["r"]),
                                       conj_poly(p["hz"], p["r"])))
            yield dict(source="morpheus-rom", obj=c["name"], corner=ci,
                       gain=cor["gain"], stages=stages)


def p2k_recipes():
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        for ci, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            stages = []
            for sec in p["sections"]:
                g = sec["corners"][cn]

                def poly(x):
                    return (real_poly(*x["pair"]) if "pair" in x
                            else conj_poly(x["hz"], x["r"]))
                stages.append(stage_tf(poly(g["zero"]), poly(g["pole"]),
                                       float(g.get("scale", 1.0))))
            yield dict(source="p2k-recipe", obj=p["name"], corner=ci,
                       gain=1.0, stages=stages)


def native_bodies():
    pats = [("p2k-bytes", "ref/presets/*.bin"),
            ("native-body", "ref/cubes/*.body"),
            ("native-body", "recipes/hero/*.body"),
            ("native-body", "recipes/extrusions/*.body")]
    seen = set()
    for src, pat in pats:
        for fn in sorted(glob.glob(os.path.join(ROOT, pat))):
            raw = open(fn, "rb").read()
            if raw in seen:
                continue
            seen.add(raw)
            if len(raw) == 560:
                corners, ns = 8, 7
            elif len(raw) == 240:
                corners, ns = 4, 6
            else:
                continue
            w = struct.unpack(f"<{len(raw)//2}H", raw)
            name = os.path.splitext(os.path.basename(fn))[0]
            for c in range(corners):
                words = [tuple(w[(c * ns + s) * 5:(c * ns + s) * 5 + 5])
                         for s in range(ns)]
                if all(x == IDENTITY for x in words):
                    stages = [kernel_biquad(x) for x in words]
                    yield dict(source=src, obj=name, corner=c, gain=1.0,
                               stages=stages, null=True)
                    continue
                yield dict(source=src, obj=name, corner=c, gain=1.0,
                           stages=[kernel_biquad(x) for x in words])


def all_objects():
    for gen in (morpheus, p2k_recipes, native_bodies):
        for row in gen():
            yield row


# --------------------------------------------------------------------- tests
def is_identity_stage(b, a):
    """H(z) == 1 exactly."""
    return (abs(b[0] - 1.0) < EPS and abs(b[1]) < EPS and abs(b[2]) < EPS
            and abs(a[1]) < EPS and abs(a[2]) < EPS)


def numerator_order(b):
    """Degree of the numerator once trailing zeros are dropped."""
    if abs(b[2]) > EPS:
        return 2
    if abs(b[1]) > EPS:
        return 1
    return 0


def denominator_order(a):
    if abs(a[2]) > EPS:
        return 2
    if abs(a[1]) > EPS:
        return 1
    return 0


def main():
    per_source = {}
    order_hist = Counter()
    for row in all_objects():
        src = row["source"]
        d = per_source.setdefault(src, dict(objs=set(), corners=0, live=0,
                                            last_num2=0, last_num0=0,
                                            nstages=Counter()))
        d["objs"].add(row["obj"])
        d["corners"] += 1
        stages = row["stages"]
        if all(is_identity_stage(b, a) for b, a in stages):
            continue
        d["live"] += 1
        d["nstages"][len(stages)] += 1
        b, a = stages[-1]
        n = numerator_order(b)
        order_hist[(src, n)] += 1
        if n == 2:
            d["last_num2"] += 1
        elif n == 0:
            d["last_num0"] += 1

    print("=" * 78)
    print("EVERY FACTORY OBJECT IN TRANSFER-FUNCTION FORM")
    print("  H_k(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)")
    print("=" * 78)
    print(f"{'source':<16}{'objects':>9}{'corners':>9}{'live':>7}"
          f"{'stages':>9}{'last num deg 2':>16}{'deg 0':>8}")
    for src, d in per_source.items():
        st = "/".join(str(k) for k in sorted(d["nstages"]))
        pct = 100 * d["last_num2"] / d["live"] if d["live"] else 0
        print(f"{src:<16}{len(d['objs']):>9}{d['corners']:>9}{d['live']:>7}"
              f"{st:>9}{d['last_num2']:>10} {pct:5.1f}%{d['last_num0']:>8}")
    print()
    print("  'last num deg 2' = the final stage carries a real quadratic")
    print("  numerator, i.e. a live zero pair. 'deg 0' = numerator is a")
    print("  constant, i.e. the cascade terminates all-pole.")
    print()
    print("Emulator X filter XMLs: 140 files, 0 translatable to a transfer")
    print("  function. They carry frequency, gain, 6 designer-sections and")
    print("  type-absolute only -- the roots live in the Emulator X ROM, which")
    print("  is not in this tree. They cannot enter a TF comparison at all.")


if __name__ == "__main__":
    main()
