#!/usr/bin/env python3
"""make_303_table.py — the acid endpoints, measured, as an academia-style table.

WHY THIS EXISTS. A TB-303 is a 4-pole lowpass with ONE resonance. Measured
honestly (harmonic envelope of a dry one-note sweep, so the note's partials
are removed and the filter is what's left), each endpoint yields exactly one
peak — 709 Hz at the squelch end, 3.8 kHz wide open. The P2K lane grammar
wants four voices. The gap is not filled by inventing three more peaks: it is
filled by INTENT's Tadpole recipe, which says what the missing three are —

    "A harmonic ladder above: 3-4 poles spaced at near-harmonic intervals
     over the root, strict order ... KEY TRACKING: poles tuned to harmonic
     intervals over a root."   (recipes/INTENT.md, Tadpole)

So the ladder is placed on the harmonic series of the MEASURED root, bracketing
the MEASURED resonance at that endpoint. Every number below is either measured
or derived from a measured one, and each row says which:

  root f0                 MEASURED (autocorrelation on the sweep)
  resonance f, bw         MEASURED (-3 dB width of the harmonic envelope peak)
  ladder frequencies      DERIVED  (harmonics of the measured root)
  ladder bandwidths       DERIVED  f / Q, where Q is the resonance's OWN
                                   measured Q at that endpoint - the
                                   instrument has one resonance setting per
                                   wheel position, so every pole in the ladder
                                   shares that character. No invented widths.

Usage: python tools/make_303_table.py <m0.wav> <m100.wav> [--out PATH]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
sys.path.insert(0, str(ROOT / "tools"))

from arma_measure_lib import load_wav                                # noqa: E402
from batch_ingest import FREQS as TF_FREQS, _detect_f0, tonal_to_tf  # noqa: E402
from extractor import LOG_POINTS, extract                            # noqa: E402

BAND = (60.0, 16_000.0)
NLADDER = 4

def measure(path: Path) -> dict:
    x, sr = load_wav(path)
    f0, clarity = _detect_f0(x, sr)
    if not f0 or clarity < 0.30:
        raise SystemExit(f"{path.name}: no stable root (clarity {clarity:.2f}) "
                         "- the ladder needs a measured root")
    env, _ = tonal_to_tf(x, sr, f0=f0)
    lo, hi = max(BAND[0], TF_FREQS[0]), min(BAND[1], sr * 0.45, TF_FREQS[-1])
    grid = np.geomspace(lo, hi, LOG_POINTS)
    gdb = np.interp(np.log(grid), np.log(TF_FREQS), env)
    peaks = extract(grid, gdb)
    if not peaks:
        raise SystemExit(f"{path.name}: no resonance in the envelope")
    hz, bw, lvl = max((p for p in peaks if p[0] > 2.5 * f0), key=lambda p: p[2],
                      default=peaks[-1])
    return {"source": path.name, "rate_hz": float(sr), "f0_hz": float(f0),
            "clarity": float(clarity), "res_hz": float(hz), "res_bw_hz": float(bw),
            "res_level_db": float(lvl)}

def ladder(m: dict) -> list[dict]:
    f0, res, q = m["f0_hz"], m["res_hz"], m["res_hz"] / m["res_bw_hz"]
    top_n = max(2, int(round(res / f0)))
    ns, n = [], top_n
    while len(ns) < NLADDER - 1 and n > 1:
        ns.append(n)
        n = max(1, n // 2)
    ns = sorted(set(ns))[-(NLADDER - 1):]
    ns = [1] + ns
    while len(ns) < NLADDER:
        ns = sorted(set(ns + [max(ns) * 2]))
    rows = []
    for i, k in enumerate(ns, 1):
        hz = f0 * k
        is_res = k == top_n
        rows.append({
            "mode_or_formant": f"L{i}",
            "frequency_hz": round(hz, 2),
            "bandwidth_hz": round(m["res_bw_hz"] if is_res else hz / q, 2),
            "q": round(q, 3),
            "harmonic_of_root": k,
            "provenance": ("measured resonance (-3 dB width of the harmonic "
                           "envelope peak)" if is_res else
                           f"derived: harmonic {k} of the measured root, "
                           f"bandwidth = f / Q with Q measured at this endpoint"),
        })
    return rows

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("m0"), ap.add_argument("m100")
    ap.add_argument("--out", default=str(ROOT / "recipes" / "tables"
                                         / "tb303_acid_sweep.json"))
    ap.add_argument("--label", default="AM_OneNoteSweepDry_120A")
    args = ap.parse_args()

    objects, meta = [], []
    for path, oid, name in ((args.m0, "tb303_m0_squelch", "303 squelch (filter closed)"),
                            (args.m100, "tb303_m100_open", "303 open (filter wide)")):
        m = measure(Path(path))
        meta.append(m)
        print(f"  {Path(path).name}: root {m['f0_hz']:.2f} Hz "
              f"(clarity {m['clarity']:.2f})  resonance {m['res_hz']:.0f} Hz "
              f"bw {m['res_bw_hz']:.0f}  Q {m['res_hz']/m['res_bw_hz']:.2f}")
        rows = ladder(m)
        for r in rows:
            print(f"      L{rows.index(r)+1} h{r['harmonic_of_root']:<3d} "
                  f"{r['frequency_hz']:8.1f} Hz  bw {r['bandwidth_hz']:7.1f}")
        objects.append({"object_id": oid, "label": name,
                        "f0_hz": round(m["f0_hz"], 2),
                        "measured_resonance_hz": round(m["res_hz"], 1),
                        "measured_resonance_bw_hz": round(m["res_bw_hz"], 1),
                        "source_window": m["source"],
                        "formants": rows})

    doc = {
        "schema": "acoustic-source-v1",
        "dataset": "tb303_acid_sweep",
        "category": "instruments",
        "citation": "MusicRadar '303-style acid samples' free sample pack, "
                    "One Note Sweeps (dry), AM_OneNoteSweepDry_120A.wav",
        "source": "musicradar-303-style-acid-samples.zip",
        "measurement_method":
            "Harmonic envelope (spectrum sampled at n*f0, interpolated) of two "
            "windows of one dry one-note filter sweep, so the note's partials "
            "are removed and the filter remains; resonance frequency and -3 dB "
            "bandwidth read off that envelope by find_peaks. Ladder members are "
            "harmonics of the measured root per INTENT's Tadpole recipe, "
            "bandwidth = f / Q with Q measured at that endpoint. Never fitted.",
        "rate_hz": meta[0]["rate_hz"],
        "note": "A 303 is a 4-pole lowpass with ONE resonance: exactly one peak "
                "is measurable per endpoint. The other three lanes are the "
                "Tadpole harmonic ladder, marked per row in 'provenance'.",
        "objects": objects,
    }
    out = Path(args.out)
    out.write_text(json.dumps(doc, indent=1))
    print(f"  wrote {out}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
