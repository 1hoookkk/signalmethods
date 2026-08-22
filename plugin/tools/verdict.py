from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np
from scipy.signal import find_peaks

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import DATUM  # noqa: E402
from joint_fit import cascade_db, certify, probe  # noqa: E402

T = dict(surface_rms_db=5.0, corner_rms_db=3.0,
         feature_hz_cents=100.0, feature_db=6.0, feature_capture=0.8)

def main():
    body = Path(sys.argv[1]).read_bytes()
    ref = Path(sys.argv[2]).read_bytes()
    out = Path(sys.argv[3]) if len(sys.argv) > 3 else None
    freqs = np.geomspace(40.0, 16_000.0, 1024)
    g = np.linspace(0, 1, 9)
    errs, feats = [], []
    for q in g:
        for m in g:
            r1, _ = probe(ref, float(m), float(q), DATUM)
            r2, _ = probe(body, float(m), float(q), DATUM)
            t = cascade_db(r1, freqs, DATUM)
            o = cascade_db(r2, freqs, DATUM)
            e = o - t
            errs.append(e)
            for sgn in (1.0, -1.0):
                idx, _ = find_peaks(sgn * t, prominence=8.0)
                for i in idx:
                    fi, di = freqs[i], t[i]
                    w = (freqs > fi * 0.944) & (fi * 1.059 > freqs)
                    oi = (o[w].max() if sgn > 0 else o[w].min())
                    j = np.argmax(sgn * o[w])
                    fo = freqs[w][j]
                    cents = abs(1200 * np.log2(fo / fi))
                    feats.append(bool(cents <= T["feature_hz_cents"]
                                      and abs(oi - di) <= T["feature_db"]))
    e = np.concatenate(errs); e -= e.mean()
    ec = np.concatenate([errs[0], errs[8], errs[72], errs[80]])
    ec -= ec.mean()
    ok, mr = certify(body)
    v = dict(
        surface_rms_db=round(float(np.sqrt((e ** 2).mean())), 3),
        surface_max_db=round(float(np.abs(e).max()), 1),
        corner_rms_db=round(float(np.sqrt((ec ** 2).mean())), 3),
        features_total=len(feats),
        features_captured=int(sum(feats)),
        feature_capture=round(sum(feats) / max(len(feats), 1), 3),
        certify=bool(ok), max_pole_r=round(mr, 6),
        thresholds=T)
    v["pass"] = bool(ok and v["surface_rms_db"] <= T["surface_rms_db"]
                     and v["corner_rms_db"] <= T["corner_rms_db"]
                     and v["feature_capture"] >= T["feature_capture"])
    s = json.dumps(v, indent=1)
    print(s)
    if out:
        out.write_text(s)
    sys.exit(0 if v["pass"] else 1)

if __name__ == "__main__":
    main()
