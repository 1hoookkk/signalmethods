"""Can a Morpheus `.4` body be carried by six sections?

Every `.4` body is decoded at its own datum (39,062.5 Hz).  Each corner's
7-section cascade response, computed by the native core from the packed words,
is the target.  A free six-section cascade is fitted to it.  Each root pair is
either conjugate (hz, radius) or a real-axis pair (two radii of one sign),
chosen per root from the body's own decoded geometry so the model spans what
the words can hold.  A seven-section fit seeded from the body's own geometry is
the control: it must reach ~0 or the model does not match the core's law.

Also answers: are the eight corners of a `.4` body padding along one axis?
"""
import argparse
import json
import math
import pathlib
import sys

import numpy as np
from scipy.optimize import least_squares

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "out/build/windows-msvc-release/native/research"))
import trench_native_research as core  # noqa: E402

SR = core.kMorpheusDatumHz
DECODED = ROOT / "ref/morpheus_decoded"
GRID = np.asarray([20.0 * (18000.0 / 20.0) ** (i / 255.0) for i in range(256)])
FLOOR_DB = 80.0
WARP_MAX = 120.0
LO_HZ, HI_HZ = math.log(10.0), math.log(0.5 * SR)

W = 2.0 * math.pi * GRID / SR
Z1 = np.exp(-1j * W)
Z2 = Z1 * Z1


def radius_of_warp(w):
    return 1.0 - 10.0 ** (-w / 20.0)


def warp_of_radius(r):
    return min(-20.0 * math.log10(max(1.0 - abs(r), 1e-9)), WARP_MAX)


def pair_coefficients(kind, a, b):
    if kind == "c":
        r = radius_of_warp(b)
        return -2.0 * r * math.cos(2.0 * math.pi * math.exp(a) / SR), r * r
    r1, r2 = kind * radius_of_warp(a), kind * radius_of_warp(b)
    return -(r1 + r2), r1 * r2


def root_layout(root):
    kind = root["kind"]
    if kind == "conjugate":
        return "c", [math.log(max(root["hz"], 10.0)), warp_of_radius(root["radius"])]
    if kind == "real":
        a, b = root["root_a"], root["root_b"]
        sign = 1.0 if (a if a != 0.0 else b) >= 0.0 else -1.0
        return sign, [warp_of_radius(a), warp_of_radius(b)]
    return "c", [math.log(1000.0), 0.0]


def section_layout(sec):
    pk, pv = root_layout(sec["pole"])
    zk, zv = root_layout(sec["zero"])
    return (pk, zk), pv + zv


def model_db(x, layout):
    total = np.zeros_like(GRID)
    for i, (pk, zk) in enumerate(layout):
        a1, a2 = pair_coefficients(pk, x[4 * i], x[4 * i + 1])
        b1, b2 = pair_coefficients(zk, x[4 * i + 2], x[4 * i + 3])
        num = 1.0 + b1 * Z1 + b2 * Z2
        den = 1.0 + a1 * Z1 + a2 * Z2
        total += 20.0 * np.log10(np.maximum(np.abs(num / den), 1e-30))
    return total


def bounds(layout):
    lo, hi = [], []
    for pk, zk in layout:
        for k in (pk, zk):
            if k == "c":
                lo += [LO_HZ, 0.0]
                hi += [HI_HZ, WARP_MAX]
            else:
                lo += [0.0, 0.0]
                hi += [WARP_MAX, WARP_MAX]
    return np.asarray(lo), np.asarray(hi)


def stats(target, got):
    floor = target.max() - FLOOR_DB
    d = np.maximum(target, floor) - np.maximum(got, floor)
    d = d - d.mean()
    return float(np.sqrt(np.mean(d * d))), float(np.abs(d).max())


def fit(target, seeds):
    floor = target.max() - FLOOR_DB
    t = np.maximum(target, floor)
    best = None
    for layout, seed in seeds:
        lo, hi = bounds(layout)

        def resid(x, layout=layout):
            d = t - np.maximum(model_db(x, layout), floor)
            return d - d.mean()

        x0 = np.clip(np.asarray(seed, dtype=float), lo + 1e-6, hi - 1e-6)
        try:
            r = least_squares(resid, x0, bounds=(lo, hi), method="trf", x_scale="jac", max_nfev=3000)
        except ValueError:
            continue
        if best is None or r.cost < best[0].cost:
            best = (r, layout)
    return best


def corner_target(corner):
    flat = [w for s in corner["sections"] for w in s["words"]]
    return np.asarray(core.cascade_db(flat, list(GRID), SR))


def to_native_word(v):
    w = core.encode_word(min(max(v, 0.0), 1.0))
    return (w >> 4 << 4) | 0xF


def pack(x, layout, offset_db):
    n = len(layout)
    gain = to_native_word(10.0 ** (offset_db / 20.0 / n) / 4.0)
    rows = []
    for i, (pk, zk) in enumerate(layout):
        p, q = pair_coefficients(pk, x[4 * i], x[4 * i + 1])
        pm, pr = to_native_word((p + q + 1.0) / 4.0), to_native_word(1.0 - q)
        p, q = pair_coefficients(zk, x[4 * i + 2], x[4 * i + 3])
        zm, zr = to_native_word((p + q + 1.0) / 4.0), to_native_word(1.0 - q)
        rows.append([zm, zr, pm, pr, gain])
    return rows


def padding_check(doc):
    curves = [corner_target(c) for c in doc["corner_data"]]
    out = []
    for axis in range(3):
        diffs = []
        for a in range(8):
            b = a ^ (1 << axis)
            if b > a:
                d = curves[a] - curves[b]
                diffs.append(float(np.sqrt(np.mean((d - d.mean()) ** 2))))
        out.append(float(np.mean(diffs)))
    return out


def run_body(row, corners):
    doc = json.load(open(DECODED / row["path"]))
    name = row.get("manual_name") or doc["source_file"]
    result = {"name": name, "file": doc["source_file"], "axis_rms": padding_check(doc), "corners": []}
    for corner in doc["corner_data"]:
        if corner["corner"] not in corners:
            continue
        target = corner_target(corner)
        if not np.all(np.isfinite(target)):
            continue
        secs = [section_layout(s) for s in corner["sections"]]
        spans = [s["span_db"] for s in corner["sections"]]
        seed7 = ([k for k, _ in secs], [v for _, vals in secs for v in vals])
        fitted = fit(target, [seed7])
        if fitted is None:
            print(f"  {name:<22} c{corner['corner']}  control fit raised on every seed; skipped", flush=True)
            continue
        control, c_layout = fitted
        c_rms, c_max = stats(target, model_db(control.x, c_layout))
        weakest = sorted(range(7), key=lambda j: spans[j])[:4]
        seeds6 = [([k for j, (k, _) in enumerate(secs) if j != drop],
                   [v for j, (_, vals) in enumerate(secs) if j != drop for v in vals]) for drop in weakest]
        six, s_layout = fit(target, seeds6)
        s_rms, s_max = stats(target, model_db(six.x, s_layout))
        offset = float(np.mean(target - model_db(six.x, s_layout)))
        rows = pack(six.x, s_layout, offset)
        packed = np.asarray(core.cascade_db([w for r in rows for w in r], list(GRID), SR))
        p_rms, p_max = stats(target, packed) if np.all(np.isfinite(packed)) else (float("nan"),) * 2
        result["corners"].append({
            "corner": corner["corner"],
            "control7_rms": c_rms, "control7_max": c_max,
            "six_rms": s_rms, "six_max": s_max,
            "six_packed_rms": p_rms, "six_packed_max": p_max,
            "spans": spans,
            "six_words": rows,
        })
        print(f"  {name:<22} c{corner['corner']}  7-ctl {c_rms:5.2f}/{c_max:5.2f}   "
              f"6-fit {s_rms:5.2f}/{s_max:5.2f}   6-packed {p_rms:5.2f}/{p_max:5.2f}  "
              f"spans {' '.join(f'{s:4.0f}' for s in spans)}", flush=True)
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--body", default=None, help="substring of manual name or file")
    ap.add_argument("--bodies", default=None, help="file with one source_file per line")
    ap.add_argument("--corners", default="0,1,2,3,4,5,6,7")
    ap.add_argument("--out", default=None)
    ap.add_argument("--shard", default="0/1")
    args = ap.parse_args()
    corners = {int(v) for v in args.corners.split(",")}
    index = json.load(open(DECODED / "index.json"))
    rows = [r for r in index["filters"] if r["geometry"] == "square"]
    if args.body:
        rows = [r for r in rows if args.body.lower() in (r.get("manual_name") or "").lower()
                or args.body.lower() in r["source_file"].lower()]
    if args.bodies:
        wanted = set(pathlib.Path(args.bodies).read_text().split())
        rows = [r for r in rows if r["source_file"] in wanted]
    k, n = (int(v) for v in args.shard.split("/"))
    rows = rows[k::n]
    results = [run_body(r, corners) for r in rows]
    if args.out:
        pathlib.Path(args.out).write_text(json.dumps(results, indent=1))
    allc = [c for r in results for c in r["corners"]]
    if allc:
        for key in ("control7_rms", "six_rms", "six_max", "six_packed_rms"):
            v = np.asarray([c[key] for c in allc])
            v = v[np.isfinite(v)]
            print(f"{key:<16} median {np.median(v):5.2f}  p90 {np.percentile(v, 90):5.2f}  max {v.max():5.2f}  n={len(v)}")


if __name__ == "__main__":
    main()
