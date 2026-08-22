from __future__ import annotations

import json
from pathlib import Path

import numpy as np

import runtime_probe as rp

ROOT = Path(__file__).resolve().parents[1]
ENVELOPE_PATH = ROOT / "recipes" / "tables" / "hardware_envelope.json"

ENCODING_SPAN_DB = 100.0
DC_UNITY_TOL_DB = 0.01

MORPH_STEPS = 33
Q_ROWS = (0.0, 1.0)

_envelope: dict | None = None

def envelope() -> dict:
    global _envelope
    if _envelope is None:
        if not ENVELOPE_PATH.exists():
            raise SystemExit(
                f"{ENVELOPE_PATH.relative_to(ROOT)} is missing - run "
                "tools/measure_corpus_envelope.py; the compiler refuses to "
                "certify against a law it has not measured")
        _envelope = json.loads(ENVELOPE_PATH.read_text())
    return _envelope

def states():
    return [(i / (MORPH_STEPS - 1), q)
            for q in Q_ROWS for i in range(MORPH_STEPS)]

def _stage_db(body: bytes, morph: float, q: float, rate: float):
    bq = rp.biquads_at(body, morph, q, rate)
    if bq is None:
        return None, None
    per = np.stack([rp.to_db(rp.stage_complex(bq[s], rate))
                    for s in range(rp.NUM_STAGES)])
    return per, per.sum(axis=0)

def peak_report(body: bytes, rate: float) -> dict:
    band = rp.GRID < rate * 0.5
    worst = None
    for morph, q in states():
        per, total = _stage_db(body, morph, q, rate)
        if total is None:
            continue
        i = int(np.argmax(np.where(band, total, -np.inf)))
        if worst is None or total[i] > worst["peak_db"]:
            worst = dict(peak_db=float(total[i]), morph=float(morph),
                         q=float(q), hz=float(rp.GRID[i]),
                         lane=int(np.argmax(per[:, i])) + 1,
                         lane_db=float(np.max(per[:, i])))
    return worst or dict(peak_db=float("nan"), morph=0.0, q=0.0, hz=0.0,
                         lane=0, lane_db=float("nan"))

def dc_gain_db(body: bytes, morph: float, q: float, rate: float):
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

def high_band_db(body: bytes, morph: float, q: float, rate: float):
    _per, total = _stage_db(body, morph, q, rate)
    if total is None:
        return None
    band = (rp.GRID >= rate * 0.25) & (rp.GRID < rate * 0.5)
    return float(np.mean(total[band]))

def crossing_report(body: bytes, rate: float) -> list[dict]:
    top = rate * 0.5 - 1.0
    out = []
    for q in Q_ROWS:
        ms = [i / (MORPH_STEPS - 1) for i in range(MORPH_STEPS)]
        paths = []
        for m in ms:
            words = rp.interpolate_words(body, m, q)
            row = []
            for s in range(rp.NUM_STAGES):
                r = rp.roots_from_words(words[s], rate)
                if r is None:
                    row.append((None, None))
                    continue
                pole_hz, _pr, zero_hz, _zr, _sc = r
                row.append((pole_hz if 20.0 < pole_hz < top else None,
                            zero_hz if 20.0 < zero_hz < top else None))
            paths.append(row)
        for i in range(rp.NUM_STAGES):
            for j in range(rp.NUM_STAGES):
                for kind, other in (("pole/zero", 1), ("pole/pole", 0)):
                    if i == j and kind == "pole/pole":
                        continue
                    if kind == "pole/pole" and j < i:
                        continue
                    prev = None
                    for k, m in enumerate(ms):
                        a, b = paths[k][i][0], paths[k][j][other]
                        if a is None or b is None:
                            prev = None
                            continue
                        d = a - b
                        if prev is not None and (prev[1] > 0) != (d > 0):
                            out.append(dict(
                                kind=kind, q=q, lanes=f"S{i + 1}/S{j + 1}",
                                morph=round((prev[0] + m) / 2.0, 4),
                                hz=round((a + b) / 2.0, 2)))
                        prev = (m, d)
    return out

CORNERS = ((0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0))

def certify(body: bytes, rate: float) -> dict:
    law = envelope()["law"]
    corpus_ceiling = float(law["max_state_peak_db"]["max"])
    peak = peak_report(body, rate)
    if peak["peak_db"] > ENCODING_SPAN_DB:
        raise SystemExit(
            f"UNREPRESENTABLE: S{peak['lane']} drives the cascade to "
            f"{peak['peak_db']:.1f} dB at {peak['hz']:.0f} Hz "
            f"(morph {peak['morph']:.2f}, Q{int(peak['q'] * 100)}) - the "
            f"ARMAdillo encoding spans {ENCODING_SPAN_DB:.0f} dB of resonance "
            "height. This is outside the format, not a hot filter.")
    corner_dc = [dc_gain_db(body, m, q, rate) for m, q in CORNERS]
    over_unity = [g for g in corner_dc if g is not None and g > DC_UNITY_TOL_DB]
    if over_unity:
        raise SystemExit(
            f"DC ABOVE UNITY: {max(over_unity):+.3f} dB at a corner - the "
            "patent fixes DC gain to unity regardless of coefficient choice, "
            "and the corpus never violates it.")
    warnings = []
    if peak["peak_db"] > corpus_ceiling:
        warnings.append(
            f"peak {peak['peak_db']:.1f} dB is past the {corpus_ceiling:.1f} "
            f"dB the corpus reaches ({law['max_state_peak_db']['n']} "
            "references) - representable, but hotter than E-mu's designers "
            "ever went; Filter Level is the player's remedy")
    crossings = crossing_report(body, rate)
    return {
        "peak_db": round(peak["peak_db"], 3),
        "peak_at": f"S{peak['lane']} {peak['hz']:.0f} Hz "
                   f"M{peak['morph']:.2f} Q{int(peak['q'] * 100)}",
        "corpus_peak_db": corpus_ceiling,
        "dc_gain_db": [None if g is None else round(g, 3) for g in corner_dc],
        "high_band_db": [None if (h := high_band_db(body, m, q, rate)) is None
                         else round(h, 3) for m, q in CORNERS],
        "crossings": len(crossings),
        "crossing_detail": crossings,
        "warnings": warnings,
    }
