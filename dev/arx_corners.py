"""ARMAX-style corner extraction from rendered WAVs.

Takes recordings of one object at its four P2K corners, estimates the usable
band from the excitation, fits a 12th-order pole-zero cascade per corner over
that band only, and reports lane correspondence across the morph.

Poles come from A(z) and zeros from B(z), so this is ARMAX rather than ARX: an
all-pole ARX model cannot locate anti-formants at all.

When the takes share one excitation, corner ratios are exact filter ratios with
the source cancelled outright. Absolute per-corner geometry still carries the
source; pass --source to divide a known excitation out first.

Usage:
  python dev/arx_corners.py <m0.wav> <m100.wav> [<m0q100.wav> <m100q100.wav>]
                            [--source <excitation.wav>] [--floor 70]
"""
import sys, os, math, warnings
import numpy as np
from scipy.io import wavfile
from scipy import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import df2_compile as C

warnings.filterwarnings("ignore")

NPERSEG = 16384
CORNERS4 = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
CORNERS2 = ["M0", "M100"]


def load(path):
    sr, x = wavfile.read(path)
    x = np.asarray(x, dtype=float)
    if x.ndim > 1:
        x = x.mean(1)
    return sr, x


def psd_db(x, sr, nperseg=None):
    n = nperseg
    if n is None:
        n = NPERSEG
        while n > 256 and n > len(x) // 2:
            n //= 2
    f, p = signal.welch(x, sr, nperseg=min(n, len(x)))
    return f, 10.0 * np.log10(np.maximum(p, 1e-30))


def usable_band(f, curves, floor_db):
    """Widest span where at least one corner stands above its own noise floor."""
    best = np.max(np.vstack(curves), axis=0)
    top = best.max()
    live = (f > 30.0) & (best > top - floor_db)
    if not live.any():
        return None
    idx = np.flatnonzero(live)
    return f[idx[0]], f[idx[-1]]


def estimate_f0(f, db, lo=40.0, hi=400.0):
    """Lowest strong partial, used to set the peak-hold width."""
    band = (f >= lo) & (f <= hi)
    if not band.any():
        return None
    sub_f, sub = f[band], db[band]
    peak = sub.max()
    strong = sub_f[sub > peak - 6.0]
    return float(strong[0]) if strong.size else None


def envelope_on(f, db, lo, hi, f0=None, bins=512):
    """Peak-hold across one partial spacing, the way author::wav does it, so a
    harmonic comb becomes an envelope instead of a set of nulls."""
    grid = np.geomspace(lo, hi, bins)
    out = np.empty(grid.size)
    for i, hz in enumerate(grid):
        half = 0.5 * f0 if f0 else max(0.06 * hz, f[1] - f[0])
        sel = (f >= hz - half) & (f <= hz + half)
        out[i] = db[sel].max() if sel.any() else np.interp(hz, f, db)
    return grid, out


def main(argv):
    paths = [a for a in argv if not a.startswith("--")]
    floor_db = 70.0
    if "--floor" in argv:
        floor_db = float(argv[argv.index("--floor") + 1])
    source = None
    if "--source" in argv:
        source = argv[argv.index("--source") + 1]
        paths = [p for p in paths if p != source]
    if len(paths) not in (2, 4):
        print(__doc__)
        return
    CORNERS = CORNERS4 if len(paths) == 4 else CORNERS2

    sr0, sig = None, []
    for p in paths:
        sr, x = load(p)
        sr0 = sr0 or sr
        if sr != sr0:
            print("rate mismatch: %s is %d Hz, expected %d" % (p, sr, sr0))
            return
        sig.append(x)
    print("%d corners at %d Hz, lengths %s s"
          % (len(sig), sr0, " / ".join("%.3f" % (len(x) / sr0) for x in sig)))

    shortest = min(len(x) for x in sig)
    nper = NPERSEG
    while nper > 256 and nper > shortest // 2:
        nper //= 2
    print("analysis window %d samples (%.0f ms), set by the shortest take\n"
          % (nper, 1000.0 * nper / sr0))

    curves = []
    for x in sig:
        f, db = psd_db(x, sr0, nper)
        curves.append(db)
    if source:
        _, sx = load(source)
        _, sdb = psd_db(sx, sr0)
        curves = [c - sdb for c in curves]
        print("source divided out: %s\n" % os.path.basename(source))

    band = usable_band(f, curves, floor_db)
    if band is None:
        print("no usable band")
        return
    lo, hi = band
    print("usable band (within %.0f dB of the loudest corner): %.0f .. %.0f Hz" % (floor_db, lo, hi))
    if not source:
        print("NOTE: no --source given, so per-corner geometry still contains the excitation.")
    print()

    f0 = estimate_f0(f, curves[0])
    print("excitation f0 estimate: %s\n" % ("%.1f Hz" % f0 if f0 else "none found"))

    SPAN_DB = 60.0
    fits = []
    for name, db in zip(CORNERS, curves):
        grid, target = envelope_on(f, db, lo, hi, f0)
        live = grid[target > target.max() - SPAN_DB]
        top = float(live.max()) if live.size else hi
        sub = grid <= top
        res = C.compile_target(target[sub] - target[sub].max(), grid[sub])
        res["ceiling_hz"] = top
        fits.append(res)
        poles = [p for p, _ in res["sections"]]
        hzs = [round(t * sr0 / (2 * math.pi)) if k == "conjugate" else None
               for k, _, t in poles]
        hzs = [h if h is None or h <= res["ceiling_hz"] else "%d(unconstrained)" % h for h in hzs]
        print("%-11s residual %5.2f dB RMS   fit to %5.0f Hz   poles: %s"
              % (name, res["rms_db"], res["ceiling_hz"], [h for h in hzs if h]))
    print()

    print("lane correspondence across the morph (pole Hz per section):")
    print("  %-4s" % "S" + "".join("%10s" % c for c in CORNERS))
    for si in range(C.SECTIONS):
        row = []
        for res in fits:
            kind, val, theta = res["sections"][si][0]
            row.append(theta * sr0 / (2 * math.pi) if kind == "conjugate" else None)
        cells = "".join("%10s" % ("real" if v is None else "%.0f" % v) for v in row)
        print("  S%-3d%s" % (si + 1, cells))
    travel = []
    for si in range(C.SECTIONS):
        a, b = fits[0]["sections"][si][0], fits[1]["sections"][si][0]
        ok = a[0] == "conjugate" and b[0] == "conjugate" and a[2] and b[2]
        travel.append(12.0 * math.log2(b[2] / a[2]) if ok else None)
    print("\n  morph travel M0_Q0 -> M100_Q0 (semitones):")
    print("   ", "  ".join("--" if t is None else "%+.1f" % t for t in travel))
    print("\n  Sections are ordered here by ascending pole angle, so lane crossings")
    print("  cannot appear. That ordering is this script's, not the object's.")

    print("\nratios against %s (source cancels exactly, no model needed):" % CORNERS[0])
    print("  %-8s" % "Hz" + "".join("%10s" % c for c in CORNERS[1:]))
    for hz in [80, 160, 320, 640, 1280, 2560, 5120]:
        if hz < lo or hz > hi:
            continue
        i = int(np.argmin(abs(f - hz)))
        print("  %-8d" % hz + "".join("%10.1f" % (c[i] - curves[0][i]) for c in curves[1:]))


if __name__ == "__main__":
    main(sys.argv[1:])
