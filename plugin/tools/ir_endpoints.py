from __future__ import annotations
import json, math, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from tools.tf_ingest import ir_to_rows, ir_to_tf, SR_RUNTIME

IR_DIR = Path("recipes/measured_objects/ir_library")
OUT = Path("recipes/endpoints")


def bandwidth_hz(r, sr):
    return -math.log(max(min(r, 0.999999), 1e-12)) * sr / math.pi


def source_prominence(wav, freqs, sr):
    import numpy as np
    from tools.tf_ingest import _load_ir
    x = _load_ir(wav, sr)
    x = x[int(np.argmax(np.abs(x))):]
    nfft = int(2 ** np.ceil(np.log2(len(x))))
    db = 20 * np.log10(np.abs(np.fft.rfft(x, nfft)) + 1e-12)
    f = np.fft.rfftfreq(nfft, 1.0 / sr)
    grid = np.geomspace(40.0, 16000.0, 1200)
    g = np.interp(grid, f, db)
    g = np.convolve(g, np.ones(9) / 9, mode="same")
    out = []
    for hz in freqs:
        at = float(np.interp(hz, grid, g))
        lo = float(np.interp(hz * 2 ** -0.5, grid, g))
        hi = float(np.interp(hz * 2 ** 0.5, grid, g))
        out.append(max(at - max(lo, hi), 0.0))
    return out


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    wavs = sorted(IR_DIR.glob("*/*.wav"))
    made, failed = [], []
    for wav in wavs:
        slug = wav.stem.lower().replace(" ", "_").replace("-", "_")
        try:
            rows, report = ir_to_rows(wav, n_rows=6, f_lo=100.0, f_hi=10000.0)
        except Exception as exc:
            failed.append((slug, repr(exc)))
            continue
        bad = [i for i, r in enumerate(rows) if r["zero"]["r"] > 0.999
               or r["pole"]["r"] >= 1.0]
        proms = source_prominence(wav, [r["pole"]["hz"] for r in rows], SR_RUNTIME)
        tf = ir_to_tf(wav)
        import numpy as _np
        sg = _np.geomspace(60.0, 14000.0, 96)
        sdb = _np.interp(sg, tf["freqs_hz"], tf["mag_db"])
        sdb = sdb - _np.median(sdb)
        rec = {
            "format": "endpoint-v1",
            "name": slug,
            "source_wav": str(wav).replace("\\", "/"),
            "sample_rate": SR_RUNTIME,
            "residual_db": report["rows_rms_floored_db"],
            "unrepresentable_rows": bad,
            "source_spectrum": {"hz": sg.tolist(), "db": sdb.tolist()},
            "rows": [
                {
                    "pole_hz": r["pole"]["hz"],
                    "pole_r": r["pole"]["r"],
                    "pole_bw_hz": bandwidth_hz(r["pole"]["r"], SR_RUNTIME),
                    "zero_hz": r["zero"]["hz"],
                    "zero_r": r["zero"]["r"],
                    "scale_db": r["scale_db"],
                    "prominence_db": pr,
                }
                for r, pr in zip(rows, proms)
            ],
        }
        (OUT / f"{slug}.endpoint.json").write_text(json.dumps(rec, indent=1))
        made.append((slug, report["rows_rms_floored_db"], len(bad)))

    print(f"{'endpoint':38s} {'residual dB':>11s} {'unrepresentable':>15s}")
    for slug, res, bad in made:
        print(f"{slug:38s} {res:11.2f} {bad:15d}")
    for slug, err in failed:
        print(f"{slug:38s}  FAILED  {err}")
    print(f"\n{len(made)} endpoints written to {OUT}, {len(failed)} failed")


if __name__ == "__main__":
    main()
