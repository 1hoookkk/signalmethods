import csv
import math
import pathlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
NAMES = ("Δ log2 pole Hz (oct)", "Δ pole width (oct)", "Δ zero offset (oct)", "Δ zero width (oct)")


def vec(r):
    if r["pole_kind"] != "conjugate":
        return None
    if r["zero_kind"] == "conjugate":
        zo, zw = float(r["zero_offset_oct"]), float(r["zero_bw_oct"])
    elif r["zero_kind"] == "real":
        zo, zw = (-6.0 if float(r["zero_real_a"]) > 0 else 6.0), 2.0
    else:
        zo, zw = 6.0, 4.0
    return np.array([math.log2(float(r["pole_hz"])), float(r["pole_bw_oct"]), zo, zw])


def moves(idx, bodies, pairs):
    out = []
    for a, b in pairs:
        for body in bodies:
            d = []
            for s in range(6):
                va, vb = vec(idx[(body, a, s)]), vec(idx[(body, b, s)])
                if va is None or vb is None:
                    d = None
                    break
                d.append(vb - va)
            if d is not None:
                out.append(np.array(d))
    return np.array(out)


def main():
    rows = [r for r in csv.DictReader(open(ROOT / "ref" / "sections.tsv"), delimiter="\t")
            if r["source"] == "p2k" and r["variant"] == "0" and int(r["body"][4:7]) <= 32]
    idx = {(r["body"], int(r["corner"]), int(r["section"])): r for r in rows}
    bodies = sorted({r["body"] for r in rows})
    axes_moves = {"Morph (M0→M100)": moves(idx, bodies, [(0, 1), (2, 3)]),
                  "Q (Q0→Q100)": moves(idx, bodies, [(0, 2), (1, 3)])}
    out = ROOT / "dev" / "pca"
    lines = []
    for label, m in axes_moves.items():
        flat = m.reshape(len(m), 24)
        z = (flat - flat.mean(0)) / np.where(flat.std(0) > 0, flat.std(0), 1)
        s = np.linalg.svd(z - z.mean(0), compute_uv=False)
        var = s**2 / np.sum(s**2)
        lines.append(f"{label}: {len(m)} moves; PC1 {var[0]*100:.0f}% PC2 {var[1]*100:.0f}% PC3 {var[2]*100:.0f}%; 8 PCs {var[:8].sum()*100:.0f}%")
        for s_ in range(6):
            med = np.median(m[:, s_, :], axis=0)
            q1, q3 = np.percentile(m[:, s_, :], 25, axis=0), np.percentile(m[:, s_, :], 75, axis=0)
            lines.append(f"  S{s_+1} " + "  ".join(f"{n.split(' ')[1]} {med[k]:+.2f} [{q1[k]:+.2f},{q3[k]:+.2f}]" for k, n in enumerate(NAMES)))
    (out / "corner_moves.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 4, figsize=(20, 9), sharey="col")
    for row, (label, m) in enumerate(axes_moves.items()):
        for k in range(4):
            ax = axes[row, k]
            data = [m[:, s_, k] for s_ in range(6)]
            ax.axhline(0, color="0.6", lw=0.8)
            ax.boxplot(data, labels=[f"S{s_+1}" for s_ in range(6)], showfliers=True, flierprops={"marker": ".", "markersize": 3})
            for s_ in range(6):
                ax.scatter(np.full(len(data[s_]), s_ + 1) + np.random.uniform(-0.18, 0.18, len(data[s_])), data[s_], s=6, alpha=0.35)
            ax.set_title(f"{label}: {NAMES[k]}", fontsize=10)
            ax.grid(True, axis="y", alpha=0.3)
    fig.suptitle("per-lane change between partner corners, 33 bodies — each dot one move; the spread is the finding")
    fig.tight_layout()
    fig.savefig(out / "corner_moves.png", dpi=100)
    print("plate:", out / "corner_moves.png")


if __name__ == "__main__":
    main()
