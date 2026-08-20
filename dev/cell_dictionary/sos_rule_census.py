"""
Per-SOS (single second-order-section) design-rule census.

Unit of analysis: one SOS = (object, stage_index) -- a single physical
stage slot inside one factory preset (P2K) or cube (Morpheus), looked at
ONLY through its own set of stored corner states. No interpolation, no
path/trajectory between corners: each corner is a discrete stored state,
compared pairwise along each cube axis.

For each SOS and each cube axis (M, Q, and Z where it exists), this script
classifies whether the SOS's pole, zero, and scale/gain are HELD (constant
within the project's own documented packed-grid precision) or MOVED across
that axis, or undergo a TOPOLOGY_CHANGE (root kind itself changes -- e.g.
conjugate <-> real <-> degenerate). The tuple of per-axis classifications
for one SOS is its "design-rule signature". The script then reports how
many distinct signatures actually occur, i.e. the smallest set of recurring
per-SOS rules that covers the corpus -- as a coverage curve, not an
auto-picked answer.

Thresholds are the project's own documented packed-grid precision
(CLAUDE.md, "The object" section):
  - resonant frequency: within ~10 cents (0.10 semitone) -> HELD
  - radius: within 5e-4 -> HELD
  - scale: within 0.01 dB -> HELD
Real-root-pair values have no established precedent threshold in this
project; this script uses the same 5e-4 absolute tolerance as radius,
since real roots are themselves bounded [-1,1]-scale eigenvalue-like
quantities -- flagged here explicitly as this script's own assumption,
not a repository-established rule.

Read-only against the repository. Writes only under
dev/cell_dictionary/output/.
"""
import csv
import json
import os
from collections import Counter, defaultdict

from load_corpus import load_p2k_objects, load_morpheus_objects

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")
os.makedirs(OUT_DIR, exist_ok=True)

FREQ_HELD_ST = 0.10     # ~10 cents, CLAUDE.md "resonant roots within about 10 cents"
RADIUS_HELD = 5e-4      # CLAUDE.md "radii within 5e-4"
SCALE_HELD_DB = 0.01    # CLAUDE.md "scale within 0.01 dB"
REAL_ROOT_HELD = 5e-4   # this script's own assumption (see module docstring)

import math


def cents_apart(hz_a, hz_b):
    if hz_a <= 0.0 or hz_b <= 0.0:
        return None
    return abs(12.0 * math.log2(hz_a / hz_b))


def classify_root_pair(a, b):
    """a, b: the pole (or zero) dict for the two corner endpoints.
    Returns one of: 'held', 'moved', 'topology_change'."""
    if a["kind"] != b["kind"]:
        return "topology_change"
    if a["kind"] == "degenerate":
        return "held"
    if a["kind"] == "conjugate":
        d_hz = cents_apart(a["hz"], b["hz"])
        d_r = abs(a["r"] - b["r"])
        moved = (d_hz is not None and d_hz > FREQ_HELD_ST) or (d_r > RADIUS_HELD)
        return "moved" if moved else "held"
    if a["kind"] == "real":
        moved = abs(a["a"] - b["a"]) > REAL_ROOT_HELD or abs(a["b"] - b["b"]) > REAL_ROOT_HELD
        return "moved" if moved else "held"
    raise ValueError(f"unknown root kind {a['kind']!r}")


def classify_scale(scale_a, scale_b):
    if scale_a is None or scale_b is None:
        return None
    if scale_a <= 0.0 or scale_b <= 0.0:
        return "moved" if abs(scale_a - scale_b) > 1e-9 else "held"
    d_db = abs(20.0 * math.log10(scale_a / scale_b))
    return "held" if d_db <= SCALE_HELD_DB else "moved"


# ---------------------------------------------------------------------------
# axis definitions (pairs of corner_index sharing every other axis)
# ---------------------------------------------------------------------------

def p2k_axis_pairs():
    # corner_index order == ["M0_Q0","M100_Q0","M0_Q100","M100_Q100"]
    return {
        "M": [(0, 1), (2, 3)],
        "Q": [(0, 2), (1, 3)],
    }


def morpheus_axis_pairs():
    # corner_index bit0=Z(Transform2), bit1=M(Morph), bit2=Q(Freq tracking)
    # per CLAUDE.md's Vulcan-firmware section corner_index law.
    pairs = defaultdict(list)
    for ci in range(8):
        z, m, q = ci & 1, (ci >> 1) & 1, (ci >> 2) & 1
        m_partner = ci ^ 0b010
        q_partner = ci ^ 0b100
        z_partner = ci ^ 0b001
        if ci < m_partner:
            pairs["M"].append((ci, m_partner))
        if ci < q_partner:
            pairs["Q"].append((ci, q_partner))
        if ci < z_partner:
            pairs["Z"].append((ci, z_partner))
    return dict(pairs)


# ---------------------------------------------------------------------------
# per-SOS signature construction
# ---------------------------------------------------------------------------

def sos_is_fully_identity(stage_states):
    """stage_states: list of per-corner {pole, zero, scale, [is_null]} for
    one SOS. Uses the project's own established null-stage definition
    (raw pole words == raw zero words -> exact pole/zero cancellation,
    i.e. net-identity biquad regardless of individual root radius -- see
    author/src/bin/import_morpheus.rs's null_stage check) where available
    (Morpheus), falling back to both-roots-degenerate for P2K, which has
    no stored raw-word field in this tool's P2K decode path and whose
    scale can differ from 1.0 on an otherwise pole=zero=degenerate stage
    (P2K scale is per-section and non-cancelling, unlike Morpheus corner
    gain)."""
    for st in stage_states:
        if "is_null" in st:
            if not st["is_null"]:
                return False
        else:
            if st["pole"]["kind"] != "degenerate" or st["zero"]["kind"] != "degenerate":
                return False
            sc = st.get("scale")
            if sc is not None and abs(sc - 1.0) > 1e-6:
                return False
    return True


def build_sos_signature(obj, stage_index, axis_pairs, has_scale):
    corners = obj["corners"]
    stage_states = [c["stages"][stage_index] for c in corners]
    fully_identity = sos_is_fully_identity(stage_states)

    per_axis = {}
    for axis, pair_list in axis_pairs.items():
        pole_votes, zero_votes, scale_votes = [], [], []
        for (i, j) in pair_list:
            a = corners[i]["stages"][stage_index]
            b = corners[j]["stages"][stage_index]
            pole_votes.append(classify_root_pair(a["pole"], b["pole"]))
            zero_votes.append(classify_root_pair(a["zero"], b["zero"]))
            if has_scale:
                scale_votes.append(classify_scale(a["scale"], b["scale"]))
        pole_majority = Counter(pole_votes).most_common(1)[0][0]
        zero_majority = Counter(zero_votes).most_common(1)[0][0]
        pole_consistent = len(set(pole_votes)) == 1
        zero_consistent = len(set(zero_votes)) == 1
        entry = {
            "pole": pole_majority, "pole_consistent": pole_consistent,
            "zero": zero_majority, "zero_consistent": zero_consistent,
            "n_pairs": len(pair_list),
        }
        if has_scale:
            scale_majority = Counter(scale_votes).most_common(1)[0][0]
            entry["scale"] = scale_majority
            entry["scale_consistent"] = len(set(scale_votes)) == 1
        per_axis[axis] = entry

    return {
        "fully_identity": fully_identity,
        "per_axis": per_axis,
    }


def signature_key(sig, axes, has_scale):
    """Collapse a full per-SOS signature into a hashable tuple: the
    recurring GEOMETRIC 'design rule' identity for this SOS (pole + zero
    held/moved/topology_change per axis; majority class only, consistency
    reported separately). Scale/gain is deliberately EXCLUDED from this
    key and reported as its own separate census -- per this project's own
    canon ("Level comes from geometry ... never from per-section gain
    shaping"), amplitude is not treated as part of the geometric design
    rule for either corpus."""
    parts = []
    for axis in axes:
        e = sig["per_axis"][axis]
        parts.append((axis, e["pole"], e["zero"]))
    return tuple(parts)


def scale_signature_key(sig, axes):
    """Separate, scale/gain-only signature (P2K only; Morpheus gain is
    corner-level and censused independently by gain_axis_census)."""
    return tuple((axis, sig["per_axis"][axis]["scale"]) for axis in axes)


def gain_axis_census(objects, axis_pairs):
    """Object-level (Morpheus only) per-axis corner-cascade-gain
    held/moved census -- gain lives at the corner, not the stage, for
    Morpheus, so this is reported separately from the per-SOS signatures."""
    rows = []
    for obj in objects:
        for axis, pair_list in axis_pairs.items():
            votes = []
            for (i, j) in pair_list:
                ga = obj["corners"][i]["gain"]
                gb = obj["corners"][j]["gain"]
                votes.append(classify_scale(ga, gb))
            rows.append({
                "object": obj["name"], "axis": axis,
                "majority": Counter(votes).most_common(1)[0][0],
                "consistent": len(set(votes)) == 1,
                "n_pairs": len(pair_list),
            })
    return rows


def run_corpus(objects, axis_pairs, corpus_label, has_scale, n_stages):
    all_sigs = []
    scale_sigs = []
    detail_rows = []
    for obj in objects:
        for stage_index in range(n_stages):
            sig = build_sos_signature(obj, stage_index, axis_pairs, has_scale)
            key = signature_key(sig, list(axis_pairs.keys()), has_scale)
            all_sigs.append((obj["name"], stage_index, sig["fully_identity"], key))
            if has_scale:
                scale_sigs.append((sig["fully_identity"], scale_signature_key(sig, list(axis_pairs.keys()))))
            row = {
                "object": obj["name"], "stage_index": stage_index,
                "fully_identity": sig["fully_identity"],
            }
            for axis in axis_pairs:
                e = sig["per_axis"][axis]
                row[f"{axis}_pole"] = e["pole"]
                row[f"{axis}_pole_consistent"] = e["pole_consistent"]
                row[f"{axis}_zero"] = e["zero"]
                row[f"{axis}_zero_consistent"] = e["zero_consistent"]
                if has_scale:
                    row[f"{axis}_scale"] = e["scale"]
                    row[f"{axis}_scale_consistent"] = e["scale_consistent"]
            detail_rows.append(row)

    n_total = len(all_sigs)
    identity_sigs = [s for s in all_sigs if s[2]]
    active_sigs = [s for s in all_sigs if not s[2]]

    active_counter = Counter(s[3] for s in active_sigs)
    ranked = active_counter.most_common()

    # coverage curve: cumulative fraction of ACTIVE (non-identity) SOS
    # explained by the K most common signatures
    n_active = len(active_sigs)
    coverage = []
    running = 0

    def fmt_signature(sig):
        parts = []
        for axis_tuple in sig:
            a, p, z = axis_tuple
            parts.append(f"{a}:{p}/{z}")
        return " | ".join(parts)

    for k, (sig, count) in enumerate(ranked, start=1):
        running += count
        coverage.append({
            "k_signatures": k,
            "signature": fmt_signature(sig),
            "count": count,
            "cumulative": running,
            "cumulative_fraction_of_active": running / n_active if n_active else 0.0,
        })

    print(f"\n=== {corpus_label} ===")
    print(f"total SOS instances: {n_total} ({len(objects)} objects x {n_stages} stages)")
    print(f"fully-identity SOS (unused stage slots): {len(identity_sigs)} "
          f"({100.0 * len(identity_sigs) / n_total:.1f}%)")
    print(f"active SOS: {n_active}")
    print(f"distinct recurring GEOMETRIC (pole+zero, scale excluded) design-rule "
          f"signatures among active SOS: {len(ranked)}")
    print("coverage by K most common geometric signatures:")
    for row in coverage[:15]:
        print(f"  K={row['k_signatures']:>2}  count={row['count']:>4}  "
              f"cum={row['cumulative']:>4}/{n_active} "
              f"({100*row['cumulative_fraction_of_active']:.1f}%)   {row['signature']}")

    scale_coverage = []
    if has_scale:
        active_scale = [s for (fi, s) in scale_sigs if not fi]
        scale_counter = Counter(active_scale)
        scale_ranked = scale_counter.most_common()
        running_s = 0
        n_active_scale = len(active_scale)
        for k, (sig, count) in enumerate(scale_ranked, start=1):
            running_s += count
            scale_coverage.append({
                "k_signatures": k,
                "signature": " | ".join(f"{a}:{s}" for (a, s) in sig),
                "count": count,
                "cumulative": running_s,
                "cumulative_fraction_of_active": running_s / n_active_scale if n_active_scale else 0.0,
            })
        print(f"\ndistinct SCALE-ONLY (per-SOS, separate from geometry) signatures: {len(scale_ranked)}")
        print("coverage by K most common scale signatures:")
        for row in scale_coverage[:10]:
            print(f"  K={row['k_signatures']:>2}  count={row['count']:>4}  "
                  f"cum={row['cumulative']:>4}/{n_active_scale} "
                  f"({100*row['cumulative_fraction_of_active']:.1f}%)   {row['signature']}")

    return {
        "corpus": corpus_label,
        "n_total_sos": n_total,
        "n_identity_sos": len(identity_sigs),
        "n_active_sos": n_active,
        "n_distinct_signatures": len(ranked),
        "coverage_curve": coverage,
        "scale_coverage_curve": scale_coverage,
        "detail_rows": detail_rows,
    }


def write_csv(rows, path, fieldnames):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        for r in rows:
            w.writerow(r)


def main():
    p2k_objects = load_p2k_objects()
    morph_objects = load_morpheus_objects()

    p2k_axes = p2k_axis_pairs()
    morph_axes = morpheus_axis_pairs()

    p2k_result = run_corpus(p2k_objects, p2k_axes, "P2K (33 objects x 6 stages, M/Q axes)",
                             has_scale=True, n_stages=6)
    morph_result = run_corpus(morph_objects, morph_axes, "Morpheus (289 objects x 7 stages, M/Q/Z axes)",
                               has_scale=False, n_stages=7)

    morph_gain_rows = gain_axis_census(morph_objects, morph_axes)
    gain_counter = Counter((r["axis"], r["majority"]) for r in morph_gain_rows)
    print("\n=== Morpheus corner-cascade gain, per-axis (object-level, not per-SOS) ===")
    for (axis, cls), count in sorted(gain_counter.items()):
        print(f"  axis={axis}  {cls:>18}  {count}")

    p2k_fields = ["object", "stage_index", "fully_identity",
                  "M_pole", "M_pole_consistent", "M_zero", "M_zero_consistent", "M_scale", "M_scale_consistent",
                  "Q_pole", "Q_pole_consistent", "Q_zero", "Q_zero_consistent", "Q_scale", "Q_scale_consistent"]
    morph_fields = ["object", "stage_index", "fully_identity",
                     "M_pole", "M_pole_consistent", "M_zero", "M_zero_consistent",
                     "Q_pole", "Q_pole_consistent", "Q_zero", "Q_zero_consistent",
                     "Z_pole", "Z_pole_consistent", "Z_zero", "Z_zero_consistent"]

    write_csv(p2k_result["detail_rows"], os.path.join(OUT_DIR, "p2k_sos_rules.csv"), p2k_fields)
    write_csv(morph_result["detail_rows"], os.path.join(OUT_DIR, "morpheus_sos_rules.csv"), morph_fields)
    write_csv(morph_gain_rows, os.path.join(OUT_DIR, "morpheus_gain_axis_rules.csv"),
              ["object", "axis", "majority", "consistent", "n_pairs"])

    with open(os.path.join(OUT_DIR, "p2k_coverage_curve.json"), "w") as f:
        json.dump(p2k_result["coverage_curve"], f, indent=1)
    with open(os.path.join(OUT_DIR, "morpheus_coverage_curve.json"), "w") as f:
        json.dump(morph_result["coverage_curve"], f, indent=1)
    with open(os.path.join(OUT_DIR, "p2k_scale_coverage_curve.json"), "w") as f:
        json.dump(p2k_result["scale_coverage_curve"], f, indent=1)

    summary = {
        "thresholds": {
            "freq_held_semitones": FREQ_HELD_ST,
            "radius_held": RADIUS_HELD,
            "scale_held_db": SCALE_HELD_DB,
            "real_root_held": REAL_ROOT_HELD,
        },
        "p2k": {k: v for k, v in p2k_result.items() if k != "detail_rows"},
        "morpheus": {k: v for k, v in morph_result.items() if k != "detail_rows"},
    }
    with open(os.path.join(OUT_DIR, "sos_rule_summary.json"), "w") as f:
        json.dump(summary, f, indent=1)

    print(f"\nwrote outputs to {OUT_DIR}")


if __name__ == "__main__":
    main()
