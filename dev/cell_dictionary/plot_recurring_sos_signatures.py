"""
For the top recurring per-SOS geometric design-rule signatures (from
sos_rule_census.py), render the actual response-magnitude curves: one
representative SOS per signature, all of ITS OWN stored corner states
overlaid on one panel (discrete states, no path between them -- same
'no trajectory' rule as the census itself). Style matches the existing
plots/recurring_complete_sections.png convention: ranked panels, log-x
frequency, dB y-axis, titled with rank / occurrence count / geometry.

Read-only against the repository. Writes only under
dev/cell_dictionary/output/.
"""
import os
from collections import Counter

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

import decode_lib as dl
from load_corpus import load_p2k_objects, load_morpheus_objects
from sos_rule_census import (
    p2k_axis_pairs, morpheus_axis_pairs, build_sos_signature, signature_key,
    sos_is_fully_identity,
)

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")
os.makedirs(OUT_DIR, exist_ok=True)

FREQS = dl.log_grid_hz(40.0, 18000.0, 400)


def dict_to_pair(d):
    if d["kind"] == "conjugate":
        return dl.Conjugate(d["hz"], d["r"])
    if d["kind"] == "real":
        return dl.RealPair(d["a"], d["b"])
    return dl.Degenerate()


def dict_to_geometry(stage_dict):
    scale = stage_dict.get("scale")
    if scale is None:
        scale = 1.0  # Morpheus: this stage's own resonance shape, gain shown separately
    return dl.StageGeometry(pole=dict_to_pair(stage_dict["pole"]),
                             zero=dict_to_pair(stage_dict["zero"]),
                             scale=scale)


def fmt_sig(sig_key):
    return " | ".join(f"{a}:{p}/{z}" for (a, p, z) in sig_key)


def corner_label_morpheus(ci):
    z, m, q = ci & 1, (ci >> 1) & 1, (ci >> 2) & 1
    return f"M{m*100}_Q{q*100}_Z{z*100}"


def rank_signatures(objects, axis_pairs, n_stages, has_scale):
    counts = Counter()
    examples = {}
    for obj in objects:
        for stage_index in range(n_stages):
            stage_states = [c["stages"][stage_index] for c in obj["corners"]]
            if sos_is_fully_identity(stage_states):
                continue
            sig = build_sos_signature(obj, stage_index, axis_pairs, has_scale)
            key = signature_key(sig, list(axis_pairs.keys()), has_scale)
            counts[key] += 1
            examples.setdefault(key, []).append((obj, stage_index))
    return counts.most_common(), examples


def plot_corpus(objects, axis_pairs, n_stages, has_scale, corpus_label, corner_label_fn,
                 out_path, n_panels=6, grid=(3, 2)):
    ranked, examples = rank_signatures(objects, axis_pairs, n_stages, has_scale)

    fig, axes = plt.subplots(*grid, figsize=(14, 12), sharex=True, sharey=True)
    axes = axes.flatten()

    n_corners = len(objects[0]["corners"])
    cmap = plt.get_cmap("tab10" if n_corners <= 10 else "tab20")

    for i in range(min(n_panels, len(ranked))):
        sig_key, count = ranked[i]
        ax = axes[i]
        obj, stage_index = examples[sig_key][0]

        curves = []
        for ci, corner in enumerate(obj["corners"]):
            g = dict_to_geometry(corner["stages"][stage_index])
            bq = dl.stage_biquad(g, obj["sample_rate_hz"])
            db = dl.stage_response_db(bq, FREQS, obj["sample_rate_hz"])
            curves.append(db)
            ax.plot(FREQS, db, color=cmap(ci % cmap.N), lw=1.6, alpha=0.85,
                     label=corner_label_fn(ci))

        first_corner_stage = obj["corners"][0]["stages"][stage_index]
        pole0 = first_corner_stage["pole"]
        zero0 = first_corner_stage["zero"]

        def root_str(d):
            if d["kind"] == "conjugate":
                return f"{d['hz']:.0f}Hz r={d['r']:.3f}"
            if d["kind"] == "real":
                return f"real(a={d['a']:.3f},b={d['b']:.3f})"
            return "degenerate"

        title = (f"Rank #{i+1}: {count} occurrences  |  {corpus_label}\n"
                 f"{fmt_sig(sig_key)}\n"
                 f"example: {obj['name']} S{stage_index+1}  |  "
                 f"P0:{root_str(pole0)}  Z0:{root_str(zero0)}")
        ax.set_title(title, fontsize=8.3)
        ax.set_xscale("log")
        ax.grid(True, which="both", alpha=0.3)
        ax.set_ylim(-40, 40)
        ax.axhline(0, color="gray", lw=0.5, ls=":")
        if i == 0:
            ax.legend(loc="upper right", fontsize=6.5, ncol=2)

    for j in range(min(n_panels, len(ranked)), len(axes)):
        axes[j].axis("off")

    fig.suptitle(f"Top {n_panels} Recurring Per-SOS Geometric Design-Rule Signatures — {corpus_label}\n"
                 f"(one representative SOS per signature; every stored corner state of that SOS overlaid, "
                 f"no interpolation between them)", fontsize=12, fontweight="bold")
    plt.tight_layout(rect=[0, 0, 1, 0.90])
    plt.savefig(out_path, dpi=180)
    plt.close()
    print(f"wrote {out_path}  ({len(ranked)} distinct signatures total)")
    return ranked


def main():
    p2k_objects = load_p2k_objects()
    morph_objects = load_morpheus_objects()

    plot_corpus(p2k_objects, p2k_axis_pairs(), n_stages=6, has_scale=True,
                corpus_label="P2K", corner_label_fn=lambda ci: dl.P2K_CORNER_LABELS[ci],
                out_path=os.path.join(OUT_DIR, "p2k_recurring_sos_signatures.png"),
                n_panels=6, grid=(3, 2))

    plot_corpus(morph_objects, morpheus_axis_pairs(), n_stages=7, has_scale=False,
                corpus_label="Morpheus", corner_label_fn=corner_label_morpheus,
                out_path=os.path.join(OUT_DIR, "morpheus_recurring_sos_signatures.png"),
                n_panels=6, grid=(3, 2))


if __name__ == "__main__":
    main()
