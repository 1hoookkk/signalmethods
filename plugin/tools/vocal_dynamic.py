from __future__ import annotations
import json, statistics as st, subprocess, sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
SR = 48_000.0

CLOSED, OPEN = "o", "i"

ANCHOR_HZ = 120.0
ANCHOR_BW = 420.0
SIB_HZ = float(np.sqrt(4_500.0 * 12_000.0))
SIB_ZERO_R = 0.972
SIB_POLE_R = 0.870
Q_SHARPEN = 0.55

def measured(vowel: str) -> dict:
    d = json.loads((ROOT / "recipes/tables/academia/etl_mokhtari_tanaka_2000.json")
                   .read_text(encoding="utf-8", errors="replace"))
    acc = {}
    for o in d["objects"]:
        if o.get("label") != vowel:
            continue
        for x in o.get("formants", []):
            if x.get("frequency_hz") and x.get("bandwidth_hz"):
                acc.setdefault(x["mode_or_formant"], []).append(
                    (x["frequency_hz"], x["bandwidth_hz"]))
    return {k: (st.median([a for a, _ in v]), st.median([b for _, b in v]))
            for k, v in acc.items()}

def radius(bw: float) -> float:
    return float(np.exp(-np.pi * bw / SR))

def stage(pole_hz, pole_bw, zero_hz, zero_r, scale=1.0):
    return {"pole_hz": round(pole_hz, 2), "pole_r": round(radius(pole_bw), 6),
            "zero_hz": round(zero_hz, 2), "zero_r": round(zero_r, 6),
            "scale": round(scale, 6)}

def formant(fb, k: float, lift: float = 9.0):
    hz, bw = fb
    return {"pole_hz": round(hz, 2), "pole_r": round(radius(bw * k), 6),
            "zero_hz": round(hz, 2), "zero_r": round(radius(bw * k * lift), 6),
            "scale": 1.0}

def corner(vowel: dict, q_sharpen: float) -> list[dict]:
    k = q_sharpen
    return [
        stage(ANCHOR_HZ, ANCHOR_BW * k, 30.0, 0.995),
        formant(vowel["F1"], k),
        formant(vowel["F2"], k),
        formant(vowel["F3"], k),
        {"pole_hz": round(SIB_HZ, 2), "pole_r": SIB_POLE_R,
         "zero_hz": round(SIB_HZ, 2), "zero_r": SIB_ZERO_R, "scale": 1.0},
        {"pole_hz": round(SIB_HZ, 2), "pole_r": SIB_POLE_R,
         "zero_hz": round(SIB_HZ, 2), "zero_r": SIB_ZERO_R, "scale": 1.0},
    ]

def main():
    closed, open_ = measured(CLOSED), measured(OPEN)
    print(f"measured /{CLOSED}/  " + "  ".join(
        f"{k} {closed[k][0]:.0f}Hz/{closed[k][1]:.0f}BW" for k in ("F1", "F2", "F3")))
    print(f"measured /{OPEN}/  " + "  ".join(
        f"{k} {open_[k][0]:.0f}Hz/{open_[k][1]:.0f}BW" for k in ("F1", "F2", "F3")))

    corners = [corner(closed, 1.0), corner(open_, 1.0),
               corner(closed, Q_SHARPEN), corner(open_, Q_SHARPEN)]

    geo = ROOT / "recipes/sidechain/vocal_dynamic.geometry.json"
    geo.parent.mkdir(parents=True, exist_ok=True)
    geo.write_text(json.dumps({"stages": [s for c in corners for s in c]}, indent=1))

    out = ROOT / "bodies/candidates/VD_vocal_dynamic.body240"
    r = subprocess.run(["cargo", "run", "-q", "-p", "trench-core", "--release",
                        "--bin", "body-from-geometry", "--", str(geo), str(out)],
                       cwd=ROOT, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout, r.stderr)
        sys.exit(1)

    from x3_morph_compiler import apply_gain_budget, load_lib
    from dual_axis_body import response_db
    lib = load_lib()

    body, report = apply_gain_budget(out.read_bytes(), lib)
    out.write_bytes(body)
    for cname, before, after in report:
        if before != after:
            print(f"  budget {cname:9s}{before:8.1f} -> {after:6.1f} dB")
    print(f"\nBody: {out}")
    fr = np.geomspace(60, 20_000, 3072)
    print("\nformant peaks found in the probed cascade:")
    for lbl, m, q in [("M0   Q0", 0, 0), ("M100 Q0", 1, 0),
                      ("M0   Q100", 0, 1), ("M100 Q100", 1, 1)]:
        db = response_db(out.read_bytes(), m, q, lib, fr)
        pk = [i for i in range(2, len(fr) - 2)
              if db[i] > db[i - 1] and db[i] >= db[i + 1] and db[i] > db.max() - 30]
        peaks = ", ".join(f"{fr[i]:.0f}Hz" for i in pk[:4])
        sib = db[np.argmin(np.abs(fr - SIB_HZ))]
        print(f"  {lbl:9s} peak {db.max():+6.1f} dB   at [{peaks}]   "
              f"sibilance band {sib:+5.1f} dB")

if __name__ == "__main__":
    main()
