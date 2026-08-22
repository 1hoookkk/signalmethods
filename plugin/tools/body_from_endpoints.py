from __future__ import annotations
import json, math, subprocess, sys, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

Q_ARM = 0.6214
CLUSTER_PEAKS = 4
ROOT = Path(__file__).resolve().parent.parent
EP = ROOT / "recipes" / "endpoints"
BIN = ROOT / "target" / "release" / "body-from-geometry.exe"


def load(name):
    d = json.loads((EP / f"{name}.endpoint.json").read_text())
    real = sorted((r for r in d["rows"] if r.get("prominence_db", 0.0) > 0.0),
                  key=lambda r: r["pole_hz"])
    rest = [r for r in d["rows"] if r.get("prominence_db", 0.0) <= 0.0]
    best, best_span = real, 1e9
    if len(real) > CLUSTER_PEAKS:
        for i in range(len(real) - CLUSTER_PEAKS + 1):
            w = real[i:i + CLUSTER_PEAKS]
            span = math.log2(w[-1]["pole_hz"] / w[0]["pole_hz"])
            if span < best_span:
                best_span, best = span, w
    elif real:
        best_span = math.log2(real[-1]["pole_hz"] / real[0]["pole_hz"])
    chosen = best
    dropped = [r for r in real if r not in chosen]
    ranked = list(chosen) + dropped + rest
    feats = [{"hz": r["pole_hz"], "prominence_db": r["prominence_db"]} for r in chosen]
    d["cluster_span_oct"] = best_span
    d["rows"] = ranked
    d["n_features"] = len(feats)
    solved, res = d["rows"], float("nan")
    for src, dst in zip(d["rows"], solved):
        dst["prominence_db"] = src.get("prominence_db", 6.0)
    d["rows"] = solved
    d["feature_residual"] = res
    return d


def arm(r):
    return min(r + (1.0 - r) * Q_ARM, 0.999)


def corner(rows, order, armed):
    out = []
    for i in order:
        r = rows[i]
        pr = arm(r["pole_r"]) if armed else r["pole_r"]
        zr = min(r["zero_r"], 0.999)
        out.append({
            "pole_hz": r["pole_hz"], "pole_r": min(pr, 0.999),
            "zero_hz": r["zero_hz"], "zero_r": zr,
            "scale": min(max(10.0 ** (r["scale_db"] / 20.0), 1e-6), 4.0),
        })
    return out


def rank_order(rows):
    return [i for i, _ in sorted(enumerate(rows), key=lambda t: t[1]["pole_hz"])]


def peak_db(stages, sr=44100.0, n=1200):
    import numpy as np
    f = np.geomspace(20.0, 20000.0, n)
    z = np.exp(-2j * np.pi * f / sr)
    acc = np.ones_like(f, dtype=complex)
    for s in stages:
        pa = 2.0 * math.pi * s["pole_hz"] / sr
        za = 2.0 * math.pi * s["zero_hz"] / sr
        a1 = -2.0 * s["pole_r"] * math.cos(pa)
        a2 = s["pole_r"] ** 2
        n1 = -2.0 * s["zero_r"] * math.cos(za)
        n2 = s["zero_r"] ** 2
        b0 = s["scale"]
        acc *= (b0 * (1 + n1 * z + n2 * z * z)) / (1 + a1 * z + a2 * z * z)
    return float(20.0 * np.log10(np.maximum(np.abs(acc), 1e-12)).max())


def normalise(stages, target_db=0.0):
    trim = (target_db - peak_db(stages)) / (20.0 * len(stages))
    g = 10.0 ** trim
    for s in stages:
        s["scale"] = min(max(s["scale"] * g, 1e-6), 4.0)
    return stages


def build(a_name, b_name, out_path, permutation=None):
    a, b = load(a_name), load(b_name)
    oa, ob = rank_order(a["rows"]), rank_order(b["rows"])
    if permutation is not None:
        ob = [ob[k] for k in permutation]
    geom = []
    for rows, order, armed in ((a["rows"], oa, False), (b["rows"], ob, False),
                               (a["rows"], oa, True), (b["rows"], ob, True)):
        geom += normalise(corner(rows, order, armed))
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as f:
        json.dump({"stages": geom}, f)
        tmp = f.name
    out_path.parent.mkdir(parents=True, exist_ok=True)
    res = subprocess.run([str(BIN), tmp, str(out_path)], capture_output=True, text=True)
    Path(tmp).unlink(missing_ok=True)
    return res.returncode, (res.stdout + res.stderr).strip()


PAIRS = [
    ("soprano_ukulele_close_mono", "concert_ukulele_close_mono", "uke_small_to_large"),
    ("violin_body_dampened", "violin_body_resonant", "violin_damped_to_ringing"),
    ("kalimba_resonance_full", "steel_pan_medium_sweep_1", "tine_to_pan"),
    ("violin_body_resonant", "china_cymbal_contact_resonant", "wood_to_cymbal"),
    ("china_cymbal_contact_damped", "china_cymbal_contact_resonant", "cymbal_damped_to_ringing"),
    ("glockenspiel_sweep_1", "china_cymbal_sweep_sdc_1", "glock_to_cymbal"),
    ("concert_ukulele_contact_mic", "steel_pan_medium_sweep_1", "uke_to_pan"),
]


def main():
    out_dir = ROOT / "filters" / "measured"
    print(f"{'body':34s} {'result':>8s}   note")
    for a, b, name in PAIRS:
        rc, msg = build(a, b, out_dir / f"MEAS_{name}.body240")
        print(f"MEAS_{name:29s} {'ok' if rc == 0 else 'FAIL':>8s}   {msg[:70]}")


if __name__ == "__main__":
    main()
