from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import (DATUM, fit_arma, formant_peaks,  # noqa: E402
                              pack_and_certify, trim_gain_budget)
from joint_fit import OPT_GRID, certify as certify_bytes  # noqa: E402
from joint_fit import joint_refine, surface_rms  # noqa: E402

FREQS = np.geomspace(60.0, 15_000.0, 288)

DEMO = {
    "name": "PATH_VOX_ah_ee",
    "tilt_db_oct": 0.0,
    "lanes": [
        {"role": "F1", "hz": [700.0, 270.0], "bw": [130.0, 52.0],
         "q_ring": 1.0},
        {"role": "F2", "hz": [1220.0, 2290.0], "bw": [70.0, 200.0],
         "q_ring": 1.0},
        {"role": "F3", "hz": [2600.0, 3010.0], "bw": [160.0, 400.0],
         "q_ring": 0.5},
        {"role": "F4", "hz": [3300.0, 3300.0], "bw": [250.0, 250.0],
         "q_ring": 0.5},
    ],
}

def lane_db(hz, bw, freqs):
    q = max(hz / max(bw, 1.0), 0.2)
    x = freqs / hz
    return -10.0 * np.log10((1 - x * x) ** 2 + (x / q) ** 2)

def render(recipe, m, q, freqs):
    total = np.zeros_like(freqs)
    for lane in recipe["lanes"]:
        hz = lane["hz"][0] * (lane["hz"][1] / lane["hz"][0]) ** m
        bw = lane["bw"][0] * (lane["bw"][1] / lane["bw"][0]) ** m
        bw = bw * 2.0 ** (-lane.get("q_ring", 1.0) * q)
        total += lane_db(hz, bw, freqs)
    tilt = recipe.get("tilt_db_oct", 0.0)
    total += tilt * np.log2(freqs / freqs[0])
    total = np.maximum(total - total.mean(), -60.0)
    return total - total.mean()

def main():
    if "--demo" in sys.argv:
        rp = ROOT / "evidence" / "path_bodies" / "PATH_VOX_ah_ee.recipe.json"
        rp.parent.mkdir(parents=True, exist_ok=True)
        rp.write_text(json.dumps(DEMO, indent=1))
        recipe = DEMO
    else:
        rp = Path(sys.argv[1])
        recipe = json.loads(rp.read_text())
    name = recipe["name"]
    assert 1 <= len(recipe["lanes"]) <= 6, "1..6 lanes"

    corners = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)]
    corner_curves = [(FREQS, render(recipe, m, q, FREQS))
                     for m, q in corners]
    tgt = {(float(m), float(q)): render(recipe, float(m), float(q), FREQS)
           for q in OPT_GRID for m in OPT_GRID}

    corner_roots, corner_words = [], []
    for grid, dbs in corner_curves:
        pins = formant_peaks(grid, dbs)
        try:
            roots, _, _ = fit_arma(grid, dbs, pinned_hz=pins)
        except AssertionError:
            roots, _, _ = fit_arma(grid, dbs)
        words, trimmed, _ = trim_gain_budget(roots)
        corner_roots.append(trimmed)
        corner_words.append(list(words))
    body_naive, _ = pack_and_certify(sum(corner_words, []))

    print(f"{name}: joint stage against the authored path surface...")
    body, info = joint_refine(corner_roots, corner_curves,
                              targets=(FREQS, tgt))
    rms_naive = surface_rms(body_naive, tgt, FREQS, DATUM)
    if body is None:
        raise SystemExit("encoder refused; recipe needs rethinking")
    ok, mr = certify_bytes(body)
    rms = surface_rms(body, tgt, FREQS, DATUM)
    print(f"path surface: naive {rms_naive:.2f} dB -> joint {rms:.2f} dB "
          f"(seed {info['winning_seed']}, {info['seconds']:.0f}s, "
          f"certify {'PASS' if ok else 'FAIL'} max_r={mr:.6f})")
    if not ok:
        raise SystemExit("certify FAIL — not written")
    out = ROOT / "bodies" / "candidates" / f"{name}.body240"
    out.write_bytes(body)
    print(f"wrote {out}")

if __name__ == "__main__":
    main()
