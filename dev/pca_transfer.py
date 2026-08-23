import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "out/build/windows-msvc-release/native/research"))
import trench_native_research as core

GROUPS = {
    "bass": ("bass", "303", "tb_or", "bottom", "sub", "phatt"),
    "vocal": ("vox", "orator", "hedz", "bouche", "vow", "talk", "mouth", "aah", "ooh"),
    "sweep": ("sweep", "rizer", "ravage", "weava", "tracer", "hertz"),
}


def group_of(name):
    for group, keys in GROUPS.items():
        if any(k in name for k in keys):
            return group
    return "other"


def main():
    out = ROOT / "dev" / "pca"
    out.mkdir(exist_ok=True)
    hz = np.array(core.erb_grid_hz())
    curves, labels = [], []
    for path in sorted((ROOT / "ref" / "transfer").glob("P2k_0*_c?.txt")):
        db = np.loadtxt(path)
        curves.append(db - db.mean())
        labels.append(path.stem)
    x = np.array(curves)
    prototype = x.mean(axis=0)
    dev = x - prototype
    u, s, vt = np.linalg.svd(dev, full_matrices=False)
    var = s**2 / np.sum(s**2)
    scores = u * s
    lines = [f"{len(x)} corners, {x.shape[1]} points, per-curve mean removed (level is the corner gain)",
             "variance explained: " + " ".join(f"PC{i+1} {v*100:.1f}%" for i, v in enumerate(var[:8])),
             f"cumulative 3 PCs {var[:3].sum()*100:.1f}%  5 PCs {var[:5].sum()*100:.1f}%  8 PCs {var[:8].sum()*100:.1f}%",
             ""]
    for k in range(4):
        pc = vt[k]
        i_max, i_min = int(np.argmax(pc)), int(np.argmin(pc))
        lines.append(f"PC{k+1}: +{pc[i_max]:.2f} at {hz[i_max]:.0f} Hz, {pc[i_min]:.2f} at {hz[i_min]:.0f} Hz")
    lines.append("")
    lines.append(f"{'corner':34s} {'group':6s} {'PC1':>8s} {'PC2':>8s} {'PC3':>8s}")
    for label, sc in zip(labels, scores):
        lines.append(f"{label:34s} {group_of(label):6s} {sc[0]:8.1f} {sc[1]:8.1f} {sc[2]:8.1f}")
    lines.append("")
    for group in ("bass", "vocal", "sweep", "other"):
        idx = [i for i, l in enumerate(labels) if group_of(l) == group]
        if not idx:
            continue
        m = scores[idx, :3].mean(axis=0)
        sd = scores[idx, :3].std(axis=0)
        lines.append(f"{group:6s} n={len(idx):3d} mean PC1 {m[0]:6.1f} PC2 {m[1]:6.1f} PC3 {m[2]:6.1f}  sd {sd[0]:.1f} {sd[1]:.1f} {sd[2]:.1f}")
    (out / "pca_transfer.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines[:8]))
    print("\n".join(lines[-5:]))

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    ax = axes[0, 0]
    ax.semilogx(hz, prototype, "k", lw=2, label="prototype (mean)")
    for k in range(3):
        ax.semilogx(hz, prototype + s[k] / np.sqrt(len(x)) * vt[k], lw=1, label=f"+1 sd PC{k+1}")
    ax.set_title("prototype and principal deviations (dB, mean removed)")
    ax.legend(fontsize=8)
    ax.grid(True, which="both", alpha=0.3)
    ax = axes[0, 1]
    for k in range(4):
        ax.semilogx(hz, vt[k], lw=1, label=f"PC{k+1} {var[k]*100:.0f}%")
    ax.set_title("principal component shapes")
    ax.legend(fontsize=8)
    ax.grid(True, which="both", alpha=0.3)
    colors = {"bass": "tab:red", "vocal": "tab:blue", "sweep": "tab:green", "other": "tab:gray"}
    ax = axes[1, 0]
    for body in sorted({l[:-3] for l in labels}):
        idx = [i for i, l in enumerate(labels) if l.startswith(body)]
        pts = scores[idx, :2]
        ax.plot(pts[[0, 1, 3, 2, 0], 0], pts[[0, 1, 3, 2, 0], 1], "-", color=colors[group_of(body)], alpha=0.5, lw=0.8)
        ax.scatter(pts[:, 0], pts[:, 1], s=14, color=colors[group_of(body)])
        ax.annotate(body[4:7], pts[0], fontsize=6, alpha=0.7)
    ax.set_xlabel("PC1")
    ax.set_ylabel("PC2")
    ax.set_title("corner scores, each body's four corners joined (red bass, blue vocal, green sweep, gray other)")
    ax.grid(True, alpha=0.3)
    ax = axes[1, 1]
    ax.bar(range(1, 13), var[:12] * 100)
    ax.set_title("variance explained per PC (%)")
    ax.set_xlabel("PC")
    fig.tight_layout()
    fig.savefig(out / "pca_transfer.png", dpi=110)
    print("plate:", out / "pca_transfer.png")


if __name__ == "__main__":
    main()
