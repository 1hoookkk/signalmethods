"""Histogram plots of the 'moved'-case magnitudes from sos_move_magnitude.py.
Read-only against the repository; writes only under dev/cell_dictionary/output/."""
import json
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "output")


def load_raw(name):
    with open(os.path.join(OUT_DIR, name)) as f:
        return json.load(f)


def pool(raw, quantity, root):
    vals = []
    for key, v in raw.items():
        axis, r, q = key.split("|")
        if r == root and q == quantity:
            vals.extend(v)
    return np.array(vals)


def main():
    p2k_raw = load_raw("p2k_move_magnitudes_raw.json")
    morph_raw = load_raw("morpheus_move_magnitudes_raw.json")

    fig, axes = plt.subplots(2, 2, figsize=(11, 8))
    fig.suptitle("Magnitude of MOVED SOS cases (pooled across axes), P2K vs Morpheus", fontsize=12)

    def hist_panel(ax, corpus_raw, quantity, xlabel, bins, xlim=None, logx=False):
        pole_v = pool(corpus_raw, quantity, "pole")
        zero_v = pool(corpus_raw, quantity, "zero")
        if logx:
            pole_v = pole_v[pole_v > 0]
            zero_v = zero_v[zero_v > 0]
            bins_use = np.logspace(np.log10(max(min(pole_v.min(), zero_v.min()), 1e-4)),
                                    np.log10(max(pole_v.max(), zero_v.max())), bins)
            ax.set_xscale("log")
        else:
            bins_use = bins
        ax.hist(pole_v, bins=bins_use, alpha=0.55, label=f"pole (n={len(pole_v)})", color="#3b6fa0")
        ax.hist(zero_v, bins=bins_use, alpha=0.55, label=f"zero (n={len(zero_v)})", color="#c0663c")
        med_p, med_z = np.median(pole_v), np.median(zero_v)
        ax.axvline(med_p, color="#3b6fa0", linestyle="--", linewidth=1)
        ax.axvline(med_z, color="#c0663c", linestyle="--", linewidth=1)
        ax.set_xlabel(xlabel)
        ax.set_ylabel("count")
        ax.legend(fontsize=8)
        if xlim:
            ax.set_xlim(*xlim)

    hist_panel(axes[0, 0], p2k_raw, "freq_semitones", "P2K: |Δ freq| when moved (semitones)", 40)
    hist_panel(axes[0, 1], p2k_raw, "damping_delta", "P2K: |Δ damping| when moved  (d = -ln r)", 40)
    hist_panel(axes[1, 0], morph_raw, "freq_semitones", "Morpheus: |Δ freq| when moved (semitones)", 40)
    hist_panel(axes[1, 1], morph_raw, "damping_delta", "Morpheus: |Δ damping| when moved  (d = -ln r)", 40)

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    out_path = os.path.join(OUT_DIR, "move_magnitude_histograms.png")
    plt.savefig(out_path, dpi=140)
    print(f"wrote {out_path}")

    # second figure: scale/gain dB
    fig2, axes2 = plt.subplots(1, 2, figsize=(11, 4.2))
    fig2.suptitle("Magnitude of MOVED scale/gain, when moved", fontsize=12)

    def hist_scalar(ax, raw, quantity, root, xlabel, color):
        v = pool(raw, quantity, root)
        ax.hist(v, bins=40, color=color, alpha=0.8)
        ax.axvline(np.median(v), color="black", linestyle="--", linewidth=1)
        ax.set_xlabel(xlabel + f"  (n={len(v)}, median={np.median(v):.2f} dB)")
        ax.set_ylabel("count")

    hist_scalar(axes2[0], p2k_raw, "scale_db", "stage", "P2K: |Δ per-section scale| (dB)", "#3b6fa0")
    hist_scalar(axes2[1], morph_raw, "gain_db", "corner", "Morpheus: |Δ corner cascade gain| (dB)", "#c0663c")
    plt.tight_layout(rect=[0, 0, 1, 0.93])
    out_path2 = os.path.join(OUT_DIR, "move_magnitude_scale_gain.png")
    plt.savefig(out_path2, dpi=140)
    print(f"wrote {out_path2}")


if __name__ == "__main__":
    main()
