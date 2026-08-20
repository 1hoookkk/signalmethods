import json
import glob
import math
import os
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FS = 39062.5
NYQ = FS / 2.0
FGRID = np.geomspace(12.0, 19200.0, 512)
WGRID = 2.0 * np.pi * FGRID / FS

ORGANS = ["PASS", "PARKED", "LOW_SHELF", "HIGH_SHELF", "BELL", "NOTCH", "LIFT"]
ORGAN_CODE = {"PASS": ".", "PARKED": "=", "LOW_SHELF": "L", "HIGH_SHELF": "H",
              "BELL": "B", "NOTCH": "N", "LIFT": "^"}
CODE_ORGAN = {v: k for k, v in ORGAN_CODE.items()}


def resp_db(fp, rp, fz, rz, f):
    w = 2.0 * np.pi * np.asarray(f) / FS
    e1 = np.exp(-1j * w)
    e2 = np.exp(-2j * w)
    wp = 2.0 * np.pi * np.asarray(fp)[..., None] / FS
    wz = 2.0 * np.pi * np.asarray(fz)[..., None] / FS
    rp = np.asarray(rp)[..., None]
    rz = np.asarray(rz)[..., None]
    num = 1.0 - 2.0 * rz * np.cos(wz) * e1 + (rz ** 2) * e2
    den = 1.0 - 2.0 * rp * np.cos(wp) * e1 + (rp ** 2) * e2
    return 20.0 * np.log10(np.maximum(np.abs(num), 1e-30) /
                           np.maximum(np.abs(den), 1e-30))


def bw_octaves(f, r):
    if r <= 0.0 or f <= 0.0:
        return 99.0
    if r >= 1.0:
        return 0.0
    bw = -math.log(r) * FS / math.pi
    return math.log2((f + bw / 2.0) / max(1.0, f - bw / 2.0))


def classify(fp, rp, fz, rz, curve, parked):
    if parked:
        return "PARKED"
    fx = np.array([max(fp, 12.0), max(fz, 12.0)])
    extra = resp_db(fp, rp, fz, rz, np.clip(fx, 12.0, 19200.0))
    vals = np.concatenate([curve, np.atleast_1d(extra)])
    e_lo, e_hi = curve[0], curve[-1]
    pk, tr = vals.max(), vals.min()
    prom_pk = pk - max(e_lo, e_hi)
    prom_tr = min(e_lo, e_hi) - tr
    tilt = e_hi - e_lo
    if (pk - tr) < 3.0:
        return "PASS"
    if prom_tr >= 6.0 and prom_tr >= prom_pk and prom_tr >= abs(tilt):
        return "NOTCH"
    if prom_pk >= 6.0 and prom_pk >= abs(tilt) and bw_octaves(fp, rp) <= 1.5:
        return "BELL"
    if abs(tilt) >= 6.0:
        return "LOW_SHELF" if tilt < 0 else "HIGH_SHELF"
    if prom_pk >= 3.0:
        return "LIFT"
    if prom_tr >= 3.0:
        return "NOTCH"
    return "LOW_SHELF" if tilt < 0 else "HIGH_SHELF"


class Corner:
    __slots__ = ("corpus", "cube", "cube_name", "idx", "gain", "ns",
                 "fp", "rp", "fz", "rz", "sdb", "cum", "total", "organs",
                 "live", "idle", "sig")


def _finish(o):
    o.sdb = resp_db(o.fp, o.rp, o.fz, o.rz, FGRID)
    o.cum = np.cumsum(o.sdb, axis=0) + 20.0 * math.log10(max(o.gain, 1e-9))
    o.total = o.cum[-1]
    o.live = np.array([(o.rp[i] > 1e-9 or o.rz[i] > 1e-9) for i in range(o.ns)])
    return o


def load_morpheus():
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS, "morpheus must be read at its native clock"
    out = []
    for c in d["cubes"]:
        for k, cor in enumerate(c["corners"]):
            o = Corner()
            o.corpus = "morpheus"
            o.cube, o.cube_name, o.idx = c["index"], c["name"], k
            o.gain = cor["gain"]
            secs = cor["sections"]
            o.ns = 7
            o.fp = np.array([s["pole"]["hz"] for s in secs])
            o.rp = np.array([s["pole"]["r"] for s in secs])
            o.fz = np.array([s["zero"]["hz"] for s in secs])
            o.rz = np.array([s["zero"]["r"] for s in secs])
            _finish(o)
            raw = [s["raw"] for s in secs]
            o.idle = np.array([r[:2] == [1909, 2015] for r in raw])
            park = [abs(r[0] - r[2]) <= 1 and abs(r[1] - r[3]) <= 1 and
                    o.rp[i] > 0.3 for i, r in enumerate(raw)]
            o.organs = [classify(o.fp[i], o.rp[i], o.fz[i], o.rz[i],
                                 o.sdb[i], park[i]) for i in range(7)]
            o.sig = "".join(ORGAN_CODE[x] for x in o.organs)
            out.append(o)
    return out


def load_p2k():
    out = []
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        d = json.load(open(fn))
        assert d["datum_sr_hz"] == FS, "p2k recipes must be read at their datum"
        for k, cn in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            o = Corner()
            o.corpus = "p2k"
            o.cube, o.cube_name, o.idx = d["index"], d["name"], k
            secs = [s["corners"][cn] for s in d["sections"]]
            o.gain = float(np.prod([s["scale"] for s in secs]))
            o.ns = len(secs)
            f = {"pole": [], "zero": []}
            r = {"pole": [], "zero": []}
            for s in secs:
                for key in ("pole", "zero"):
                    g = s[key]
                    if "pair" in g:
                        a, b = g["pair"]
                        f[key].append(0.3 if (a + b) >= 0 else NYQ)
                        r[key].append(min(math.sqrt(abs(a * b)), 0.99999))
                    else:
                        f[key].append(g["hz"])
                        r[key].append(min(g["r"], 0.99999))
            o.fp = np.array(f["pole"])
            o.rp = np.array(r["pole"])
            o.fz = np.array(f["zero"])
            o.rz = np.array(r["zero"])
            _finish(o)
            o.idle = np.zeros(o.ns, bool)
            park = [abs(12 * math.log2(max(o.fz[i], .3) / max(o.fp[i], .3))) < .05
                    and abs(o.rp[i] - o.rz[i]) < 5e-4 and o.rp[i] > 0.3
                    for i in range(o.ns)]
            o.organs = [classify(o.fp[i], o.rp[i], o.fz[i], o.rz[i],
                                 o.sdb[i], park[i]) for i in range(o.ns)]
            o.sig = "".join(ORGAN_CODE[x] for x in o.organs)
            out.append(o)
    return out


def is_null(o):
    return bool(np.all(~o.live))


if __name__ == "__main__":
    from collections import Counter
    cs = load_morpheus()
    act = [o for o in cs if not is_null(o)]
    print(f"morpheus corners {len(cs)} active {len(act)} null {len(cs)-len(act)}")
    cnt = Counter(x for o in act for x in o.organs)
    tot = sum(cnt.values())
    for k in ORGANS:
        print(f"  {k:11s} {cnt[k]:6d} {100*cnt[k]/tot:5.1f}%")
    print("idle-pole sections:", sum(int(o.idle.sum()) for o in act))
    print("S7 zero dead:", sum(1 for o in act if o.rz[6] < 1e-9), "/", len(act))
    for nm in ("LPFlange.4", "Phaser", "Vocal Cube", "Ntches2Oct.4",
               "AEParaVowel", "BassEQ 1.4"):
        for o in cs:
            if o.cube_name == nm and o.idx == 1:
                print(f"{nm:13s} {o.sig}  " + " ".join(
                    f"[p{f:5.0f}/{r:.2f} z{g:5.0f}/{q:.2f}]"
                    for f, r, g, q in zip(o.fp, o.rp, o.fz, o.rz)))
    p = load_p2k()
    c2 = Counter(x for o in p for x in o.organs)
    t2 = sum(c2.values())
    print(f"p2k corners {len(p)}")
    for k in ORGANS:
        print(f"  {k:11s} {c2[k]:5d} {100*c2[k]/t2:5.1f}%")
