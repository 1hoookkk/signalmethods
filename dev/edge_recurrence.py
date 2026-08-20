"""Do the same spectral transformations and the same root transformations recur?

For every legitimate factory edge A->B (both corners authored):
    dH(f) = H_B(f)_dB - H_A(f)_dB          on a fixed 1024-pt log grid
    dS    = {d_theta_p, dR_p, d_theta_z, dR_z} per lane
and then measure recurrence of each, and their overlap.
"""
import os
import json
import glob
import math
import numpy as np
import polars as pl
from collections import Counter, defaultdict
from scipy.spatial.distance import cdist

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plotdata", "emu")
FS = 39062.5
NYQ = FS / 2.0
GRID = np.geomspace(40.0, 16000.0, 1024)
W = 2.0 * np.pi * GRID / FS
E1, E2 = np.exp(-1j * W), np.exp(-2j * W)
BANDS = 64
EDGE = np.geomspace(40.0, 16000.0, BANDS + 1)
BIDX = [np.where((GRID >= EDGE[i]) & (GRID < EDGE[i + 1]))[0]
        for i in range(BANDS)]
LOG = []


def say(s=""):
    print(s)
    LOG.append(s)


def sec_db(fp, rp, fz, rz):
    num = 1 - 2 * rz * math.cos(2 * math.pi * fz / FS) * E1 + rz ** 2 * E2
    den = 1 - 2 * rp * math.cos(2 * math.pi * fp / FS) * E1 + rp ** 2 * E2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


def st(a, b):
    return 12.0 * math.log2(max(b, 1e-9) / max(a, 1e-9))


def load():
    objs = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        cs = []
        for cor in c["corners"]:
            secs = []
            for s in cor["sections"]:
                w = s["raw"]
                secs.append(dict(fp=s["pole"]["hz"], rp=s["pole"]["r"],
                                 fz=s["zero"]["hz"], rz=s["zero"]["r"],
                                 dead=(w[1] == 2047 and w[3] == 2047)))
            cs.append(secs)
        objs.append(dict(corpus="morpheus", name=c["name"], ns=7, corners=cs,
                         axes={0: "T", 1: "M", 2: "Q"}))
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        cs = []
        for cn in ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]:
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
                secs.append(dict(fp=fp, rp=rp, fz=fz, rz=rz,
                                 dead=(rp < 1e-6 and rz < 1e-6)))
            cs.append(secs)
        objs.append(dict(corpus="p2k", name=p["name"], ns=6, corners=cs,
                         axes={0: "M", 1: "Q"}))
    return objs


def corner_db(secs):
    tot = np.zeros(len(GRID))
    for s in secs:
        if s["dead"]:
            continue
        tot += sec_db(s["fp"], s["rp"], s["fz"], s["rz"])
    return tot


def main():
    objs = load()
    dH, dS, meta = [], [], []
    for o in objs:
        n = len(o["corners"])
        cache = {}
        for bit, axis in o["axes"].items():
            for i in range(n):
                if i & (1 << bit):
                    continue
                j = i | (1 << bit)
                if j >= n:
                    continue
                A, B = o["corners"][i], o["corners"][j]
                if all(s["dead"] for s in A) or all(s["dead"] for s in B):
                    continue
                for idx, secs in ((i, A), (j, B)):
                    if idx not in cache:
                        cache[idx] = corner_db(secs)
                d = cache[j] - cache[i]
                dH.append([float(d[b].mean()) for b in BIDX])
                v = []
                for k in range(6):
                    a, b = A[k], B[k]
                    v += [
                        np.clip(st(a["fp"], b["fp"]), -72, 72)
                        if (a["rp"] > 0.05 and b["rp"] > 0.05) else 0.0,
                        b["rp"] - a["rp"],
                        np.clip(st(a["fz"], b["fz"]), -72, 72)
                        if (a["rz"] > 0.05 and b["rz"] > 0.05) else 0.0,
                        b["rz"] - a["rz"]]
                dS.append(v)
                meta.append((o["corpus"], o["name"], axis, i, j))
    H = np.array(dH)
    S = np.array(dS)
    say("=" * 78)
    say("EDGE TRANSFORMATION RECURRENCE")
    say(f"  legitimate edges (both corners authored): {len(H)}")
    say(f"  dH sampled to {BANDS} log bands, 40 Hz - 16 kHz")
    say(f"  dS = {S.shape[1]} numbers per edge (6 lanes x "
        f"[d_theta_p, dR_p, d_theta_z, dR_z])")
    say()

    same_cube = np.array([m[1] for m in meta])
    DH = cdist(H, H) / math.sqrt(BANDS)          # RMS dB difference
    Sw = S.copy()
    Sw[:, 0::4] /= 12.0
    Sw[:, 2::4] /= 12.0
    DS = cdist(Sw, Sw) / math.sqrt(S.shape[1])
    np.fill_diagonal(DH, np.inf)
    np.fill_diagonal(DS, np.inf)
    diff_cube = same_cube[:, None] != same_cube[None, :]

    for tol, lab in ((0.5, "0.5 dB"), (1.0, "1 dB"), (2.0, "2 dB")):
        m = (DH < tol) & diff_cube
        say(f"  dH recurs within {lab:6s} RMS in a DIFFERENT object: "
            f"{int((m.any(axis=1)).sum()):5d} of {len(H)} edges "
            f"({100*m.any(axis=1).mean():5.1f}%)")
    say()
    for tol, lab in ((0.02, "tight"), (0.05, "loose")):
        m = (DS < tol) & diff_cube
        say(f"  dS recurs ({lab}) in a DIFFERENT object:            "
            f"{int((m.any(axis=1)).sum()):5d} of {len(S)} edges "
            f"({100*m.any(axis=1).mean():5.1f}%)")
    say()
    hs = (DH < 1.0) & diff_cube
    ss = (DS < 0.02) & diff_cube
    both = hs & ss
    honly = hs & ~ss
    sonly = ss & ~hs
    say("  cross-tab over edge PAIRS from different objects:")
    say(f"    same dH and same dS : {int(both.sum())//2:6d} pairs")
    say(f"    same dH, DIFFERENT dS: {int(honly.sum())//2:6d} pairs   "
        f"<- same spectral gesture built from different geometry")
    say(f"    same dS, different dH: {int(sonly.sum())//2:6d} pairs")
    say()
    say(f"  edges whose dH recurs but whose dS never does: "
        f"{int((hs.any(axis=1) & ~ss.any(axis=1)).sum())}")
    say()

    lab_h = -np.ones(len(H), int)
    nxt = 0
    for i in range(len(H)):
        if lab_h[i] >= 0:
            continue
        grp = np.where(DH[i] < 1.0)[0]
        lab_h[i] = nxt
        for g in grp:
            if lab_h[g] < 0:
                lab_h[g] = nxt
        nxt += 1
    sizes = Counter(lab_h.tolist())
    say(f"  dH gesture families (single-link, 1 dB): {nxt} families for "
        f"{len(H)} edges")
    say(f"     families with >1 edge: {sum(1 for v in sizes.values() if v > 1)}"
        f"   largest {max(sizes.values())}")
    say("     biggest recurring spectral gestures:")
    for fam, n in sizes.most_common(6):
        idx = np.where(lab_h == fam)[0]
        names = sorted({meta[i][1] for i in idx})
        ax = Counter(meta[i][2] for i in idx).most_common(1)[0]
        band = H[idx].mean(axis=0)
        pk = int(np.argmax(np.abs(band)))
        say(f"       x{n:4d} edges, {len(names):3d} objects, axis {ax[0]} "
            f"({100*ax[1]/n:.0f}%), peak {band[pk]:+.1f} dB near "
            f"{EDGE[pk]:.0f} Hz   {', '.join(names[:4])}")

    pl.DataFrame(dict(
        corpus=[m[0] for m in meta], name=[m[1] for m in meta],
        axis=[m[2] for m in meta], src=[m[3] for m in meta],
        dst=[m[4] for m in meta], gesture=lab_h.tolist(),
        dH_peak=[float(h[int(np.argmax(np.abs(h)))]) for h in H],
        dH_rms=[float(np.sqrt(np.mean(h ** 2))) for h in H],
    )).write_parquet(os.path.join(OUT, "edge_gestures.parquet"))
    open(os.path.join(ROOT, "plots", "corpus", "edge_recurrence.txt"),
         "w").write("\n".join(LOG))


if __name__ == "__main__":
    main()
