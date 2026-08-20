"""
Plot one P2K object: per-corner panels, each showing every active
section's own response curve overlaid with the full cascade (bold black),
so close/'beating' pole pairs are visible directly. Decoded at the
VERIFIED P2K datum (39,062.5 Hz).

Usage: python plot_object.py <name-fragment>

Read-only against the repository. Writes only under
dev/cell_dictionary/output/.
"""
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import decode_lib as dl
from load_corpus import load_p2k_objects

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")
os.makedirs(OUT_DIR, exist_ok=True)

FREQS = dl.log_grid_hz(40.0, 18000.0, 500)


def dict_to_pair(d):
    if d["kind"] == "conjugate":
        return dl.Conjugate(d["hz"], d["r"])
    if d["kind"] == "real":
        return dl.RealPair(d["a"], d["b"])
    return dl.Degenerate()


def dict_to_geometry(stage_dict):
    return dl.StageGeometry(pole=dict_to_pair(stage_dict["pole"]),
                             zero=dict_to_pair(stage_dict["zero"]),
                             scale=stage_dict["scale"])


def root_str(d):
    if d["kind"] == "conjugate":
        return f"{d['hz']:.0f}Hz r={d['r']:.4f}"
    if d["kind"] == "real":
        return f"real(a={d['a']:.4f},b={d['b']:.4f})"
    return "off"


def plot_p2k_object(obj, out_path):
    fig, axes = plt.subplots(2, 2, figsize=(15, 11), sharex=True, sharey=True)
    axes = axes.flatten()
    cmap = plt.get_cmap("tab10")

    for ci, corner in enumerate(obj["corners"]):
        ax = axes[ci]
        cascade = [0.0] * len(FREQS)
        for si, stage_dict in enumerate(corner["stages"]):
            g = dict_to_geometry(stage_dict)
            if dl.stage_is_identity(g):
                continue
            bq = dl.stage_biquad(g, obj["sample_rate_hz"])
            db = dl.stage_response_db(bq, FREQS, obj["sample_rate_hz"])
            cascade = [a + b for a, b in zip(cascade, db)]
            label = f"S{si+1}  P:{root_str(stage_dict['pole'])}  Z:{root_str(stage_dict['zero'])}"
            ax.plot(FREQS, db, color=cmap(si % 10), lw=1.3, alpha=0.75, label=label)

        ax.plot(FREQS, cascade, color="black", lw=2.2, label="cascade (sum)")
        ax.set_title(corner["label"], fontsize=11, fontweight="bold")
        ax.set_xscale("log")
        ax.grid(True, which="both", alpha=0.3)
        ax.set_ylim(-40, 40)
        ax.axhline(0, color="gray", lw=0.5, ls=":")
        ax.legend(loc="upper right", fontsize=6.3)

    fig.suptitle(f"{obj['name']}  (P2K, datum {obj['sample_rate_hz']:.1f} Hz)\n"
                 f"per-stage response overlaid with cascade (black), all 4 corners",
                 fontsize=13, fontweight="bold")
    plt.tight_layout(rect=[0, 0, 1, 0.93])
    plt.savefig(out_path, dpi=170)
    plt.close()
    print(f"wrote {out_path}")


def main():
    query = sys.argv[1] if len(sys.argv) > 1 else "ear_bender"
    objects = load_p2k_objects()
    matches = [o for o in objects if query.lower().replace(" ", "_").replace("-", "_") in o["name"].lower().replace("-", "_")]
    if not matches:
        print(f"no P2K object matching {query!r}; available: {[o['name'] for o in objects]}")
        return
    for obj in matches:
        out_path = os.path.join(OUT_DIR, f"object_{obj['name']}.png")
        plot_p2k_object(obj, out_path)


if __name__ == "__main__":
    main()
