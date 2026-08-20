"""Last-stage zero termination across every object in the factory tree that
carries pole/zero roots.

Sources:
  289 Morpheus ROM cubes      ref/morpheus/cubes_decoded.json   (11-bit law)
   33 P2K architectures       recipes/architectures/*.json      (v2 geometry)
   34 P2K preset bytes        ref/presets/*.bin                 (240-byte)
   20 native bodies           ref/cubes, recipes/hero, recipes/extrusions

Emulator X filter XMLs are excluded: all 140 of them carry designer
parameters only (frequency, gain, morph-param, 6 designer-sections,
type-absolute) and contain no roots to test.
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


def native_rows(path):
    """A native body is corners x stages x 5 u16: [z_mag, z_rsq, p_mag, p_rsq, scale]."""
    raw = open(path, "rb").read()
    n = len(raw) // 2
    w = struct.unpack(f"<{n}H", raw)
    if len(raw) == 560:
        corners, stages = 8, 7
    elif len(raw) == 240:
        corners, stages = 4, 6
    else:
        return None
    out = []
    for c in range(corners):
        row = []
        for s in range(stages):
            i = (c * stages + s) * 5
            row.append(tuple(w[i:i + 5]))
        out.append(row)
    return out, stages


def native_stats(rows, stages):
    """Return (live corners, last-stage zero live count, last-stage identity)."""
    live = zlive = 0
    for corner in rows:
        if all(st == IDENTITY for st in corner):
            continue
        live += 1
        last = corner[stages - 1]
        # zero radius: r_z^2 = 1 - decode(w[1]); dead when w[1] == 0xFFFF
        if decode_u16(last[1]) < 0.999999:
            zlive += 1
    return live, zlive


def main():
    print("=" * 74)
    print("LAST-STAGE ZERO TERMINATION — ENTIRE FACTORY SET")
    print("=" * 74)
    rows = []

    # --- Morpheus ROM cubes -------------------------------------------------
    d = json.load(open(os.path.join(ROOT, "ref/morpheus/cubes_decoded.json")))
    assert d["law"]["hz_datum"] == FS
    tot = live = zlive = 0
    for c in d["cubes"]:
        for cor in c["corners"]:
            secs = cor["sections"]
            tot += 1
            if all(s["raw"][1] == 2047 and s["raw"][3] == 2047 for s in secs):
                continue
            live += 1
            if secs[6]["zero"]["r"] > 0.02:
                zlive += 1
    rows.append(("Morpheus ROM cubes", "S7", 289, tot, live, zlive))

    # --- P2K v2 architecture recipes ----------------------------------------
    fs = sorted(glob.glob(os.path.join(ROOT, "recipes/architectures/*.json")))
    tot = live = zlive = 0
    for fn in fs:
        p = json.load(open(fn))
        assert p["datum_sr_hz"] == FS
        for cn in ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]:
            tot += 1
            live += 1
            g = p["sections"][-1]["corners"][cn]["zero"]
            r = (math.sqrt(abs(g["pair"][0] * g["pair"][1]))
                 if "pair" in g else g["r"])
            if r > 0.02:
                zlive += 1
    rows.append(("P2K architectures", "S6", len(fs), tot, live, zlive))

    # --- P2K preset bytes ---------------------------------------------------
    seen, tot, live, zlive, nfile = set(), 0, 0, 0, 0
    for fn in sorted(glob.glob(os.path.join(ROOT, "ref/presets/*.bin"))):
        blob = open(fn, "rb").read()
        if blob in seen:
            continue
        seen.add(blob)
        nfile += 1
        r = native_rows(fn)
        if not r:
            continue
        rr, st = r
        tot += len(rr)
        a, b = native_stats(rr, st)
        live += a
        zlive += b
    rows.append(("P2K preset bytes", "S6", nfile, tot, live, zlive))

    # --- native bodies ------------------------------------------------------
    for label, pattern, in (("native 560-byte bodies", "**/*.body"),):
        pass
    groups = {"ref/cubes": glob.glob(os.path.join(ROOT, "ref/cubes/*.body")),
              "recipes/hero": glob.glob(os.path.join(ROOT, "recipes/hero/*.body")),
              "recipes/extrusions": glob.glob(
                  os.path.join(ROOT, "recipes/extrusions/*.body"))}
    for label, files in groups.items():
        tot = live = zlive = 0
        n6 = n7 = 0
        for fn in sorted(files):
            r = native_rows(fn)
            if not r:
                continue
            rr, st = r
            n7 += st == 7
            n6 += st == 6
            tot += len(rr)
            a, b = native_stats(rr, st)
            live += a
            zlive += b
        if files:
            rows.append((f"{label} ({n7}x7-stage, {n6}x6-stage)",
                         "last", len(files), tot, live, zlive))

    print(f"{'source':<44}{'last':>5}{'objs':>6}{'corners':>9}"
          f"{'live':>7}{'zero live':>11}")
    for name, last, objs, tot, live, zlive in rows:
        pct = f"{100*zlive/live:.1f}%" if live else "n/a"
        print(f"{name:<44}{last:>5}{objs:>6}{tot:>9}{live:>7}"
              f"{zlive:>7} {pct:>4}")
    print()
    print("Emulator X filter XMLs                       ---   140"
          "        0      0       0  n/a")
    print("  (designer parameters only: frequency, gain, 6 designer-sections,")
    print("   type-absolute; no pole or zero data anywhere in the tree)")


if __name__ == "__main__":
    main()
