"""Measure the hardware-law envelope from the 33 P2k references.

The checklist in dev/RECIPE_LAYER_AND_HARDWARE_AUDIT.md says: verify, do not
invent. So the numbers the compiler refuses on are not chosen here - they are
read off the ROM corpus and written to recipes/tables/hardware_envelope.json,
which tools/hardware_law.py then enforces.

Three readings per reference, all through tools/runtime_probe.py (the one
shared numeric path - never re-derived):
  peak dB     the whole cascade's loudest point, in band, at every stored
              corner and across the interpolated states between them
  DC gain dB  the cascade at z = 1 (US 5,170,369's unity-DC anchor)
  crossings   whether one lane's pole path passes through another lane's
              zero path along the REAL encoded word interpolation

Usage: python tools/measure_corpus_envelope.py [--out PATH]
"""
from __future__ import annotations

import argparse
import json
from datetime import date
from pathlib import Path

import numpy as np

import runtime_probe as rp

ROOT = Path(__file__).resolve().parents[1]
BODIES = ROOT / "plots" / "inspector"
DOSSIERS = ROOT / "dossiers" / "characters"
OUT = ROOT / "recipes" / "tables" / "hardware_envelope.json"

MORPH_STEPS = 33
Q_ROWS = (0.0, 1.0)
CORNERS = ("M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100")
CORNER_STATE = ((0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0))

def in_band(rate: float) -> np.ndarray:
    return rp.GRID < rate * 0.5

def peak_db(body: bytes, morph: float, q: float, rate: float) -> float | None:
    bq = rp.biquads_at(body, morph, q, rate)
    if bq is None:
        return None
    total = np.sum([rp.to_db(rp.stage_complex(bq[s], rate))
                    for s in range(rp.NUM_STAGES)], axis=0)
    return float(np.max(total[in_band(rate)]))

def dc_gain_db(body: bytes, morph: float, q: float, rate: float) -> float | None:
    bq = rp.biquads_at(body, morph, q, rate)
    if bq is None:
        return None
    gain = 1.0
    for b0, b1, b2, a1, a2 in bq:
        den = 1.0 + a1 + a2
        if abs(den) < 1e-12:
            return None
        gain *= (b0 + b1 + b2) / den
    return float(20.0 * np.log10(max(abs(gain), 1e-12)))

def lane_roots(body: bytes, morph: float, q: float, rate: float) -> list:
    words = rp.interpolate_words(body, morph, q)
    top = rate * 0.5 - 1.0
    out = []
    for s in range(rp.NUM_STAGES):
        r = rp.roots_from_words(words[s], rate)
        if r is None:
            out.append((None, None))
            continue
        pole_hz, _pr, zero_hz, _zr, _sc = r
        out.append((pole_hz if 20.0 < pole_hz < top else None,
                    zero_hz if 20.0 < zero_hz < top else None))
    return out

def crossings(body: bytes, rate: float, steps: int = MORPH_STEPS) -> list[dict]:
    found = []
    for q in Q_ROWS:
        ms = [i / (steps - 1) for i in range(steps)]
        paths = [lane_roots(body, m, q, rate) for m in ms]
        for i in range(rp.NUM_STAGES):
            for j in range(rp.NUM_STAGES):
                if i == j:
                    continue
                prev = None
                for k, m in enumerate(ms):
                    p, z = paths[k][i][0], paths[k][j][1]
                    if p is None or z is None:
                        prev = None
                        continue
                    d = p - z
                    if prev is not None and (prev[1] > 0) != (d > 0):
                        found.append(dict(
                            q=q, pole_lane=i + 1, zero_lane=j + 1,
                            morph=round((prev[0] + m) / 2.0, 4),
                            pole_hz=round(p, 2), zero_hz=round(z, 2)))
                    prev = (m, d)
    return found

def measure_one(path: Path, rate: float) -> dict:
    body = rp.load_body(path)
    rp.verify_word_law(body)
    corner_peaks, corner_dc = [], []
    for m, q in CORNER_STATE:
        corner_peaks.append(peak_db(body, m, q, rate))
        corner_dc.append(dc_gain_db(body, m, q, rate))
    state_peaks = [peak_db(body, i / (MORPH_STEPS - 1), q, rate)
                   for q in Q_ROWS for i in range(MORPH_STEPS)]
    state_peaks = [p for p in state_peaks if p is not None]
    return dict(
        name=path.stem, sha256=rp.body_sha256(body), datum_sr_hz=rate,
        corner_peak_db={c: round(p, 3) for c, p in zip(CORNERS, corner_peaks)},
        corner_dc_gain_db={c: round(g, 3)
                           for c, g in zip(CORNERS, corner_dc)},
        max_state_peak_db=round(max(state_peaks), 3),
        crossings=crossings(body, rate))

def spread(values: list[float]) -> dict:
    a = np.array(values, dtype=float)
    return dict(min=round(float(a.min()), 3), max=round(float(a.max()), 3),
                median=round(float(np.median(a)), 3),
                mean=round(float(a.mean()), 3), n=int(a.size))

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()

    refs = []
    for body_path in sorted(BODIES.glob("P2k_*.body240")):
        key = body_path.stem.split("_")[1]
        doss = sorted(DOSSIERS.glob(f"P2k_{key}_*.json"))
        if not doss:
            raise SystemExit(f"{body_path.name}: no dossier, no datum rate")
        rate = float(json.loads(doss[0].read_text())["datum_sr_hz"])
        refs.append(measure_one(body_path, rate))
        r = refs[-1]
        print(f"{r['name']:34s} peak {r['max_state_peak_db']:8.2f} dB   "
              f"DC {max(r['corner_dc_gain_db'].values()):7.2f} dB   "
              f"crossings {len(r['crossings'])}", flush=True)

    corner_peaks = [v for r in refs for v in r["corner_peak_db"].values()]
    state_peaks = [r["max_state_peak_db"] for r in refs]
    dc = [v for r in refs for v in r["corner_dc_gain_db"].values()]
    crossed = [r["name"] for r in refs if r["crossings"]]
    doc = dict(
        format="trench-hardware-envelope-v1",
        provenance=dict(
            source=f"the {len(refs)} P2k reference bodies in plots/inspector, "
                   "each probed at its dossier datum_sr_hz",
            measured_by="tools/measure_corpus_envelope.py",
            measured_on=str(date.today()),
            path="tools/runtime_probe.py (biquads and geometry from "
                 "trench_core.dll; word interpolation the bit-exact port "
                 "verified against the kernel on every load)",
            states=f"4 stored corners plus {MORPH_STEPS} morph steps on each "
                   f"of Q{'/Q'.join(str(int(q * 100)) for q in Q_ROWS)}",
            band="grid points below the body's own Nyquist only",
            use="the lawful envelope tools/hardware_law.py refuses against - "
                "measured, never chosen"),
        law=dict(corner_peak_db=spread(corner_peaks),
                 max_state_peak_db=spread(state_peaks),
                 dc_gain_db=spread(dc),
                 references_with_crossings=len(crossed),
                 crossing_names=crossed),
        references=refs)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(doc, indent=1))
    print(f"\ncorner peak dB   {doc['law']['corner_peak_db']}")
    print(f"state peak dB    {doc['law']['max_state_peak_db']}")
    print(f"DC gain dB       {doc['law']['dc_gain_db']}")
    print(f"references with pole/zero crossings: {len(crossed)}/{len(refs)}"
          + (f"  {crossed}" if crossed else ""))
    print(f"-> {args.out.relative_to(ROOT)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
