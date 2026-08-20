"""Compound peak-picking vs raw-pole sorting: does the diagnosis hold?

Claim under test: sorting raw pole frequencies puts the S1 shelf / sub-bass
root into the F1 slot and destroys the formant correlation; picking peaks off
the COMPOUND cascade magnitude instead recovers the published vowel tables.

Both methods are run on the same corners so the difference is attributable.
"""
import os
import json
import glob
import math
import numpy as np
from scipy.signal import find_peaks

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FS = 39062.5
NYQ = FS / 2.0
GRID = np.geomspace(40.0, 16000.0, 4096)
W = 2.0 * np.pi * GRID / FS
E1 = np.exp(-1j * W)
E2 = np.exp(-2j * W)

# Published reference formants, as supplied (Bell Labs / Peterson-Barney men).
BELL_1961_VOWELS = {
    "i":  [270, 2290, 3010], "I":  [390, 1990, 2550],
    "E":  [530, 1840, 2480], "ae": [660, 1720, 2410],
    "a":  [730, 1090, 2440], "c":  [570, 840, 2410],
    "U":  [440, 1020, 2240], "u":  [300, 870, 2240],
    "uh": [520, 1190, 2390], "er": [490, 1350, 1690],
}
VOCAL_P2K = {"TalkingHedz", "Ooh-To-Eee", "UbuOrator", "DeepBouche",
             "Eeh-To-Aah", "MultiQVox"}
VOCAL_MORPH_TOKENS = ("vow", "vocal", "voce", "hedz", "be-ye", "ee-yi",
                      "ii-yi", "uhr", "yeah", "yah", "yoyo", "bouche",
                      "orator", "shaper", "para", "choral")


def sec_db(fp, rp, fz, rz):
    num = 1 - 2 * rz * math.cos(2 * math.pi * fz / FS) * E1 + rz ** 2 * E2
    den = 1 - 2 * rp * math.cos(2 * math.pi * fp / FS) * E1 + rp ** 2 * E2
    return 20 * np.log10(np.maximum(np.abs(num), 1e-30) /
                         np.maximum(np.abs(den), 1e-30))


def load():
    out = []
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    for c in d["cubes"]:
        vocal = any(t in c["name"].lower() for t in VOCAL_MORPH_TOKENS)
        for ci, cor in enumerate(c["corners"]):
            secs = []
            for s in cor["sections"]:
                w = s["raw"]
                if w[1] == 2047 and w[3] == 2047:
                    continue
                if w[0] == w[2] and w[1] == w[3]:
                    continue
                if tuple(w[:2]) == (1909, 2015) and w[3] == 2047:
                    continue
                secs.append((s["pole"]["hz"], s["pole"]["r"],
                             s["zero"]["hz"], s["zero"]["r"]))
            if secs:
                out.append(("morpheus", c["name"], ci, secs, vocal))
    for fn in sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json"))):
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        vocal = p["name"] in VOCAL_P2K
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
                out.append(("p2k", p["name"], ci, secs, vocal))
    return out


def compound_peaks(secs, prom=2.0, lo=180.0, hi=5200.0):
    tot = np.zeros(len(GRID))
    for fp, rp, fz, rz in secs:
        tot += sec_db(fp, rp, fz, rz)
    idx, props = find_peaks(tot, prominence=prom)
    f = GRID[idx]
    keep = (f >= lo) & (f <= hi)
    return f[keep], tot


def raw_sorted(secs, thr=0.3):
    return sorted(fp for fp, rp, fz, rz in secs if rp > thr)


def match(tri):
    if len(tri) < 3:
        return None
    best = None
    for k, v in BELL_1961_VOWELS.items():
        d = math.sqrt(np.mean([(1200 * math.log2(tri[i] / v[i])) ** 2
                               for i in range(3)]))
        if best is None or d < best[1]:
            best = (k, d)
    return best


def main():
    data = load()
    rows = []
    for corpus, name, ci, secs, vocal in data:
        pk, _ = compound_peaks(secs)
        cm = match(list(pk[:3])) if len(pk) >= 3 else None
        rw = raw_sorted(secs)
        rm = match(rw[:3]) if len(rw) >= 3 else None
        rows.append((corpus, name, ci, vocal, cm, rm,
                     list(pk[:3]), rw[:3]))

    def stats(sel, key):
        vals = [r[key][1] for r in sel if r[key]]
        if not vals:
            return "n/a"
        v = np.array(vals)
        return (f"n={len(v):4d}  median {np.median(v):6.0f}c  "
                f"<35c {100*(v<35).mean():4.1f}%  <100c {100*(v<100).mean():4.1f}%  "
                f"<400c {100*(v<400).mean():4.1f}%")

    voc = [r for r in rows if r[3]]
    non = [r for r in rows if not r[3]]
    print("MATCH TO THE PUBLISHED VOWEL TABLE (RMS cents over F1,F2,F3)")
    print(f"  all corners      compound peaks : {stats(rows,4)}")
    print(f"                   raw sorted poles: {stats(rows,5)}")
    print(f"  vocal-named      compound peaks : {stats(voc,4)}")
    print(f"                   raw sorted poles: {stats(voc,5)}")
    print(f"  non-vocal        compound peaks : {stats(non,4)}")
    print(f"                   raw sorted poles: {stats(non,5)}")
    print()
    print("P2K vocal presets, best corner by compound peak-picking:")
    best = {}
    for r in rows:
        if r[0] != "p2k" or not r[3] or not r[4]:
            continue
        if r[1] not in best or r[4][1] < best[r[1]][4][1]:
            best[r[1]] = r
    for nm, r in sorted(best.items(), key=lambda t: t[1][4][1]):
        pk = ", ".join(f"{x:.0f}" for x in r[6])
        rw = ", ".join(f"{x:.0f}" for x in r[7])
        print(f"  {nm:14s} c{r[2]}  peaks [{pk:22s}] -> /{r[4][0]}/ "
              f"{r[4][1]:5.0f}c   |  raw [{rw:22s}] -> "
              f"/{r[5][0] if r[5] else '-'}/ {r[5][1] if r[5] else float('nan'):5.0f}c")
    print()
    print("Morpheus vocal cubes, best corner:")
    bestm = {}
    for r in rows:
        if r[0] != "morpheus" or not r[3] or not r[4]:
            continue
        if r[1] not in bestm or r[4][1] < bestm[r[1]][4][1]:
            bestm[r[1]] = r
    for nm, r in sorted(bestm.items(), key=lambda t: t[1][4][1])[:12]:
        pk = ", ".join(f"{x:.0f}" for x in r[6])
        print(f"  {nm:14s} c{r[2]}  peaks [{pk:22s}] -> /{r[4][0]}/ {r[4][1]:5.0f}c")
    print()
    n_shelf = sum(1 for r in rows if r[7] and r[7][0] < 180.0)
    print(f"corners whose LOWEST raw pole is below 180 Hz (shelf/sub-bass that "
          f"raw-sorting would call F1): {n_shelf} of {len(rows)} "
          f"({100*n_shelf/len(rows):.0f}%)")


if __name__ == "__main__":
    main()
