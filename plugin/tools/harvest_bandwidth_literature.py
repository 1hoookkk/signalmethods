#!/usr/bin/env python3
"""harvest_bandwidth_literature.py — the bandwidth axis, from the literature.

WHY. Bandwidth is what makes a pole real (r = exp(-pi*B/fs)). Almost every
published vowel corpus reports formant FREQUENCIES only, because bandwidth is
the hard half to measure - which is why one table in this repo (ETL) carries it
and thirteen do not, and why those thirteen are not offered as endpoints.

This harvests the cited bandwidth/Q literature into `acoustic-source-v1` tables.

TWO KINDS OF OUTPUT, and the difference matters:

  ENDPOINT tables   - a source that states a FREQUENCY and a BANDWIDTH for the
                      same mode. These are compilable: the picker will offer
                      them.
  REFERENCE tables  - a source that states bandwidth, damping or Q with no
                      paired frequency (or the reverse). `frequency_hz` stays
                      null, and the picker will not offer them. They exist so
                      the numbers are in the repo, cited, for the day a paired
                      frequency arrives.

THE ONE THING NEVER DONE HERE: pairing one study's B with another study's F.
Fant's 1962 bandwidths and Fant's 1973 frequencies are different sessions with
different talkers; joining them would manufacture a measurement nobody made.
So the 1962 bandwidths land as a REFERENCE table even though a frequency table
with the same vowel labels sits next to it.

CONVERSIONS (each applied once, at the row, and named in `provenance`):
  Q  = f / B                     bandwidth to quality factor
  B  = 2 * zeta * f              damping ratio to -3 dB bandwidth
  B  = eta * f                   loss factor to bandwidth (eta = 1/Q)
  B  = 1 / (pi * tau)            decay time to bandwidth (Q = pi*f*tau)

Usage: python tools/harvest_bandwidth_literature.py [--out DIR]
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "recipes" / "tables" / "academia"

def q_from(f: float | None, b: float | None) -> float | None:
    return round(f / b, 3) if (f and b) else None

def bw_from_zeta(f: float, zeta: float) -> float:
    return round(2.0 * zeta * f, 4)

def bw_from_eta(f: float, eta: float) -> float:
    return round(eta * f, 4)

def bw_from_tau(tau: float) -> float:
    return round(1.0 / (math.pi * tau), 4)

def row(name, f=None, b=None, prov="", **extra):
    d = {"mode_or_formant": name,
         "frequency_hz": f,
         "bandwidth_hz": b,
         "q": q_from(f, b),
         "provenance": prov}
    d.update(extra)
    return d

def table(dataset, category, citation, source, method, objects, note=None,
          key="formants"):
    doc = {"schema": "acoustic-source-v1",
           "dataset": dataset,
           "category": category,
           "citation": citation,
           "source": source,
           "measurement_method": method}
    if note:
        doc["note"] = note
    doc["objects"] = [{**o, key: o.pop("rows")} for o in objects]
    return doc

FANT62 = {"i": (48, 50, 98), "y": (48, 60, 100), "e": (50, 60, 100),
          "E": (60, 70, 110), "a": (70, 80, 120), "O": (60, 70, 110),
          "o": (50, 60, 100), "u": (50, 60, 100)}

def fant_1962():
    objs = []
    for v, (b1, b2, b3) in FANT62.items():
        objs.append({
            "object_id": f"F62_{v}",
            "label": v,
            "f0_hz": None,
            "rows": [row(f"B{i+1}", None, float(b), "as printed (Fant 1962)")
                     for i, b in enumerate((b1, b2, b3))],
        })
    return table(
        "fant_1962_vowel_bandwidths", "vowel_bandwidth_reference",
        "Fant 1962, Acoustic Theory of Speech Production",
        "Fant (1962) formant bandwidth table, Swedish adult male",
        "Published formant bandwidths B1-B3 per vowel; frequencies are NOT "
        "paired in this table and are deliberately left null",
        objs,
        note="REFERENCE ONLY. Bandwidths without paired frequencies. Do not "
             "join these to fant_1973.json's frequencies - different study, "
             "different talkers. Reported Q ranges for adult males: Q1 ~8-30, "
             "Q2 ~10-25, Q3 ~6-15.",
        key="bandwidths")

def hanna_2015():
    sung = [(555.0, 68.0), (1120.0, 33.0), (2468.0, 69.0),
            (2737.0, 74.0), (3301.0, 140.0)]
    objs = [{
        "object_id": "HEW15_a_sung",
        "label": "a (sung)",
        "f0_hz": None,
        "rows": [row(f"F{i+1}", f, b,
                     "measured resonance and bandwidth through the lips, "
                     "glottis closed (Table 1)")
                 for i, (f, b) in enumerate(sung)],
    }]
    return table(
        "hanna_epps_wolfe_2015", "vowels",
        "Hanna, Epps & Wolfe 2015, JASA (vocal-tract acoustic impedance)",
        "Hanna/Epps/Wolfe 2015 Table 1",
        "Vocal tract resonances measured through the lips with the glottis "
        "closed; frequency and bandwidth measured on the same resonance",
        objs,
        note="Only the rows stated with BOTH frequency and bandwidth are "
             "recorded. The paper's summary ranges (men B ~50-90 Hz, women "
             "~70-90 Hz for R1-R3) are context, not rows, and are not "
             "invented into objects.")

def treszkai_2021():
    f, f1, f2 = 46.5, 45.8, 47.25
    bw = round(f2 - f1, 4)
    objs = [{
        "object_id": "TRE21_steel_plate_m1",
        "label": "steel plate 650x550x2 mm, bare",
        "f0_hz": None,
        "rows": [row("M1", f, bw,
                     "half-power bandwidth method: B = f2 - f1 "
                     "(45.80 / 47.25 Hz), eta = B/f = 0.031")],
    }]
    return table(
        "treszkai_2021_steel_plate", "objects",
        "Treszkai 2021, Extrica (half-power bandwidth method on a steel plate)",
        "Treszkai (2021) worked example",
        "Half-power (-3 dB) bandwidth read directly off the measured mode",
        objs,
        note="Bare-plate damping loss factor 0.002-0.006 over 400-1000 Hz; "
             "0.03-0.14 with viscoelastic bitumen treatment.")

def ramamurti_2013():
    plates = {"S1": [(138.0, 0.0145), (2262.0, 0.0030)],
              "S2": [(540.0, 0.0115), (9117.0, 0.0064)],
              "S3": [(993.0, 0.0104), (18234.0, 0.0110)]}
    objs = []
    for name, modes in plates.items():
        objs.append({
            "object_id": f"RAM13_{name}",
            "label": f"steel plate {name} (420x50x4 mm class)",
            "f0_hz": None,
            "rows": [row(f"M{'1' if i == 0 else '3'}", f, bw_from_zeta(f, z),
                         f"B = 2*zeta*f with the paper's zeta = {z}",
                         damping_ratio=z)
                     for i, (f, z) in enumerate(modes)],
        })
    return table(
        "ramamurti_2013_steel_plates", "objects",
        "Ramamurti et al. 2013, damping ratios for steel plates",
        "Ramamurti et al. (2013) Table 2",
        "Modal frequencies with measured damping ratios; bandwidth derived "
        "as B = 2*zeta*f",
        objs,
        note="Modes 1 and 3 only - the paper's table states those two. Very "
             "lightly damped: zeta ~0.003-0.015.")

def sol_2020():
    modes = [("torsion", 94.4, 0.00500), ("saddle", 241.2, 0.00237),
             ("breathing", 264.4, 0.00174), ("beam-X", 394.0, 0.00082)]
    objs = [{
        "object_id": "SOL20_composite_plate",
        "label": "composite plate (4 modes)",
        "f0_hz": None,
        "rows": [row(f"M{i+1}", f, bw_from_zeta(f, z),
                     f"B = 2*zeta*f with the paper's zeta = {z*100:.3f}%",
                     mode_shape=shape, damping_ratio=z)
                 for i, (shape, f, z) in enumerate(modes)],
    }]
    return table(
        "sol_2020_composite_plate", "objects",
        "Sol et al. 2020, measured damping ratios of composite plates",
        "Sol et al. (2020) modal damping table",
        "Measured modal frequencies and damping ratios per mode shape; "
        "bandwidth derived as B = 2*zeta*f",
        objs)

def ege_2018():
    plates = [("SOFT", 503.0, 0.025), ("SPS", 8022.0, 0.018)]
    objs = [{
        "object_id": f"EGE18_{n}",
        "label": f"multilayer plate {n}",
        "f0_hz": None,
        "rows": [row("M1", f, bw_from_eta(f, eta),
                     f"B = eta*f at the maximum-damping frequency, eta = {eta}",
                     loss_factor=eta)],
    } for n, f, eta in plates]
    return table(
        "ege_2018_multilayer_plates", "objects",
        "Ege et al. 2018, multilayer plates",
        "Ege et al. (2018) Table 2",
        "Frequency of maximum damping with its loss factor; bandwidth "
        "derived as B = eta*f",
        objs)

def shu_2022():
    data = [(1, 0.098), (2, 0.111), (3, 0.108), (4, 0.122),
            (5, 0.128), (6, 0.122), (7, 0.134), (8, 0.133)]
    objs = [{
        "object_id": f"SHU22_T{t}",
        "label": f"prestressed membrane, {t} kN/m",
        "f0_hz": None,
        "rows": [row("M1", None, None,
                     "damping ratio as printed; no modal frequency stated, so "
                     "no bandwidth can be derived",
                     damping_ratio=z, tension_kn_per_m=t)],
    } for t, z in data]
    return table(
        "shu_2022_prestressed_membranes", "objects",
        "Shu et al. 2022, damping of prestressed membranes",
        "Shu et al. (2022) Table 8",
        "Damping ratio versus tension; frequencies not stated",
        objs,
        note="REFERENCE ONLY. B = 2*zeta*f needs a frequency this table does "
             "not give.")

def shepherd_1982():
    bands = [("low-frequency modes", 1.0, 3.0), ("high-frequency modes", 0.2, 0.8)]
    objs = [{
        "object_id": f"SHE82_{'low' if lo >= 1.0 else 'high'}",
        "label": f"cymbal, {name}",
        "f0_hz": None,
        "rows": [row("M1", None, bw_from_tau(lo),
                     f"B = 1/(pi*tau) at the SLOW end of the stated decay "
                     f"({lo} s): Q = pi*f*tau", decay_time_s=lo),
                 row("M2", None, bw_from_tau(hi),
                     f"B = 1/(pi*tau) at the FAST end ({hi} s)",
                     decay_time_s=hi)],
    } for name, lo, hi in bands]
    return table(
        "shepherd_1982_cymbals", "instruments",
        "Shepherd 1982, JASA (cymbal mode decay times)",
        "Shepherd (1982) measured decay times",
        "Decay times per mode band; bandwidth follows from B = 1/(pi*tau) "
        "and is frequency-independent, but the modal frequencies are given "
        "only as bands and are left null",
        objs,
        note="REFERENCE ONLY. Worked check from the paper: f = 500 Hz with "
             "tau = 1 s gives Q ~ 1570.")

def van_houten_1997():
    eta_m, eta_a = 2e-4, 1e-4
    eta = round(eta_m + eta_a, 8)
    q = round(1.0 / (2.0 * eta), 1)
    objs = [{
        "object_id": "VH97_bell",
        "label": "church bell (4-bell study mean)",
        "f0_hz": None,
        "rows": [row("M1", None, None,
                     f"material damping {eta_m} + acoustic damping {eta_a} = "
                     f"total {eta}; the paper's own relation Q = 1/(2*eta) "
                     f"gives Q = {q:.0f}, matching its stated ~1600. No modal "
                     "frequency is stated, so no bandwidth is derived",
                     damping_ratio=eta, q_from_damping_ratio=q)],
    }]
    return table(
        "van_houten_1997_bells", "instruments",
        "van Houten et al. 1997, modal damping of bells",
        "van Houten et al. (1997)",
        "Material and acoustic damping components; the paper's Q = 1/(2*eta)",
        objs,
        note="REFERENCE ONLY. Extremely lightly damped: Q ~ 1600. Note the "
             "convention: this paper's eta is a damping ratio, while Ege 2018 "
             "reports a loss factor (Q = 1/eta). Each is converted by its own "
             "paper's relation.")

def liljencrants_2006():
    series = {"wood_open": [25, 40, 60, 80, 100],
              "metal_open": [40, 70, 110, 150, 200]}
    objs = []
    for name, qs in series.items():
        objs.append({
            "object_id": f"LIL06_{name}",
            "label": f"{name.replace('_', ' ')} pipe resonator",
            "f0_hz": 100.0,
            "rows": [row(f"M{i+1}", None, None,
                         f"measured Q{i+1} as printed; the mode frequency is "
                         "not stated per row, so it is left null rather than "
                         "assumed to be the n-th harmonic",
                         q_measured=q)
                     for i, q in enumerate(qs)],
        })
    return table(
        "liljencrants_2006_pipe_resonators", "objects",
        "Liljencrants 2006, pipe resonator Q measurements",
        "Liljencrants (2006) Table 1",
        "Measured Q1-Q5 for open pipe resonators of a stated 100 Hz "
        "fundamental; per-mode frequencies not printed",
        objs,
        note="REFERENCE ONLY. Q rises with mode number (radiation damping "
             "dominates at low frequency). Deriving mode frequencies as "
             "n*100 Hz would be an assumption, not a measurement, so the "
             "frequencies are null.")

def deshler_2016():
    fr, fwhm = 200.0, 50.0
    objs = [{
        "object_id": "DES16_bottle",
        "label": "Helmholtz bottle resonator",
        "f0_hz": None,
        "rows": [row("M1", fr, fwhm,
                     f"resonance and FWHM as measured; Q = f/B = {fr/fwhm:.1f}")],
    }]
    return table(
        "deshler_2016_helmholtz", "objects",
        "Deshler 2016, Helmholtz resonator lab measurements",
        "Deshler (2016) bottle resonator fit",
        "Resonant frequency with the fitted full width at half maximum",
        objs,
        note="Low Q by design: a Helmholtz resonator is a broad single mode.")

BUILDERS = [fant_1962, hanna_2015, treszkai_2021, ramamurti_2013, sol_2020,
            ege_2018, shu_2022, shepherd_1982, van_houten_1997,
            liljencrants_2006, deshler_2016]

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--out", default=str(OUT))
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    for build in BUILDERS:
        doc = build()
        path = out / f"{doc['dataset']}.json"
        path.write_text(json.dumps(doc, indent=1))
        key = "bandwidths" if "bandwidths" in doc["objects"][0] else "formants"
        rows = sum(len(o[key]) for o in doc["objects"])
        paired = sum(1 for o in doc["objects"] for r in o[key]
                     if r.get("frequency_hz") and r.get("bandwidth_hz"))
        kind = "ENDPOINT " if paired else "reference"
        print(f"  {kind}  {path.name:44s} {len(doc['objects']):3d} objects, "
              f"{rows:3d} rows, {paired:3d} paired (f,B)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
