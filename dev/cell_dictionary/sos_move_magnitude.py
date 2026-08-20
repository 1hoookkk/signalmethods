"""
Magnitude of the 'moved' cases from sos_rule_census.py. Same SOS unit,
same axis definitions, same held/moved/topology_change classification
(reused, not recomputed independently, so results stay consistent with
the earlier census) -- this script only adds: HOW MUCH does it move, in
sample-rate-independent units (semitones for frequency, damping
d=-ln(r) for radius, dB for scale/gain), when it is classified 'moved'.

No interpolation, no path/trajectory: each corner pair is one discrete
comparison, exactly as before.

Read-only against the repository. Writes only under
dev/cell_dictionary/output/.
"""
import json
import os

import numpy as np

from load_corpus import load_p2k_objects, load_morpheus_objects
from sos_rule_census import (
    p2k_axis_pairs, morpheus_axis_pairs, classify_root_pair, classify_scale,
)
import decode_lib as dl

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")
os.makedirs(OUT_DIR, exist_ok=True)

PCTS = [10, 25, 50, 75, 90, 100]


def collect_magnitudes(objects, axis_pairs, n_stages, has_scale):
    """Returns dict[(axis, root_kind_pair, quantity)] -> list of magnitudes,
    where root_kind_pair in {'pole','zero'}, quantity in
    {'freq_semitones','damping_delta'} for conjugate-conjugate moves, or
    {'real_a','real_b'} for real-real moves. Also collects scale/gain dB
    deltas under quantity 'scale_db' / 'gain_db'."""
    mags = {}

    def add(key, value):
        mags.setdefault(key, []).append(value)

    for obj in objects:
        corners = obj["corners"]
        for stage_index in range(n_stages):
            for axis, pair_list in axis_pairs.items():
                for (i, j) in pair_list:
                    a = corners[i]["stages"][stage_index]
                    b = corners[j]["stages"][stage_index]
                    for root_name in ("pole", "zero"):
                        ra, rb = a[root_name], b[root_name]
                        cls = classify_root_pair(ra, rb)
                        if cls != "moved":
                            continue
                        if ra["kind"] == "conjugate" and rb["kind"] == "conjugate":
                            st = dl.semitones(rb["hz"], ra["hz"])
                            if st is not None:
                                add((axis, root_name, "freq_semitones"), abs(st))
                            da, db_ = dl.damping(ra["r"]), dl.damping(rb["r"])
                            if da is not None and db_ is not None:
                                add((axis, root_name, "damping_delta"), abs(db_ - da))
                        elif ra["kind"] == "real" and rb["kind"] == "real":
                            add((axis, root_name, "real_a_delta"), abs(ra["a"] - rb["a"]))
                            add((axis, root_name, "real_b_delta"), abs(ra["b"] - rb["b"]))
                    if has_scale:
                        sa, sb = a.get("scale"), b.get("scale")
                        if classify_scale(sa, sb) == "moved" and sa and sb and sa > 0 and sb > 0:
                            add((axis, "stage", "scale_db"), abs(20.0 * np.log10(sb / sa)))
                if not has_scale:
                    # Morpheus corner-level gain, once per axis per object
                    # (not per stage) -- computed on the first stage_index
                    # pass only, guarded below.
                    pass

    if not has_scale:
        for obj in objects:
            corners = obj["corners"]
            for axis, pair_list in axis_pairs.items():
                for (i, j) in pair_list:
                    ga, gb = corners[i]["gain"], corners[j]["gain"]
                    if ga > 0 and gb > 0:
                        cls = classify_scale(ga, gb)
                        if cls == "moved":
                            add((axis, "corner", "gain_db"), abs(20.0 * np.log10(gb / ga)))

    return mags


def summarize(mags, label):
    print(f"\n=== {label}: magnitude of MOVED cases (percentiles) ===")
    rows = []
    for key in sorted(mags.keys()):
        axis, root_name, quantity = key
        vals = np.array(mags[key])
        pct = np.percentile(vals, PCTS)
        unit = {
            "freq_semitones": "st", "damping_delta": "d(-ln r)",
            "real_a_delta": "(linear root)", "real_b_delta": "(linear root)",
            "scale_db": "dB", "gain_db": "dB",
        }[quantity]
        pct_str = "  ".join(f"p{p}={v:.4g}" for p, v in zip(PCTS, pct))
        print(f"  axis={axis:<2} {root_name:<6} {quantity:<15} n={len(vals):>5}  {pct_str}  [{unit}]")
        rows.append({
            "axis": axis, "root": root_name, "quantity": quantity, "unit": unit,
            "n": len(vals),
            **{f"p{p}": float(v) for p, v in zip(PCTS, pct)},
            "mean": float(vals.mean()),
        })
    return rows


def main():
    p2k_objects = load_p2k_objects()
    morph_objects = load_morpheus_objects()

    p2k_mags = collect_magnitudes(p2k_objects, p2k_axis_pairs(), n_stages=6, has_scale=True)
    morph_mags = collect_magnitudes(morph_objects, morpheus_axis_pairs(), n_stages=7, has_scale=False)

    p2k_rows = summarize(p2k_mags, "P2K")
    morph_rows = summarize(morph_mags, "Morpheus")

    with open(os.path.join(OUT_DIR, "p2k_move_magnitudes.json"), "w") as f:
        json.dump(p2k_rows, f, indent=1)
    with open(os.path.join(OUT_DIR, "morpheus_move_magnitudes.json"), "w") as f:
        json.dump(morph_rows, f, indent=1)

    # raw value dumps too, for plotting / further analysis
    def dump_raw(mags, name):
        raw = {f"{a}|{r}|{q}": v for (a, r, q), v in mags.items()}
        with open(os.path.join(OUT_DIR, name), "w") as f:
            json.dump(raw, f)

    dump_raw(p2k_mags, "p2k_move_magnitudes_raw.json")
    dump_raw(morph_mags, "morpheus_move_magnitudes_raw.json")

    print(f"\nwrote outputs to {OUT_DIR}")
    return p2k_mags, morph_mags


if __name__ == "__main__":
    main()
