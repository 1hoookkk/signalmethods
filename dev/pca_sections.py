import pathlib
import struct
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "out/build/windows-msvc-release/native/research"))
import trench_native_research as core

SR = 44100.0


def main():
    corners = sys.argv[1] if len(sys.argv) > 1 else "0123"
    out = ROOT / "dev" / "pca" / f"sections_{corners}"
    out.mkdir(parents=True, exist_ok=True)
    hz = np.array(core.erb_grid_hz())
    grid = list(hz)
    per_section = [[] for _ in range(6)]
    for path in sorted((ROOT / "ref" / "presets").glob("P2k_0*.bin")):
        words = struct.unpack("<120H", path.read_bytes())
        for corner in range(4):
            if str(corner) not in corners:
                continue
            for section in range(6):
                w = list(words[corner * 30 + section * 5:corner * 30 + section * 5 + 5])
                db = np.array(core.section_db(w, grid, SR))
                per_section[section].append(db - db.mean())
    lines = [f"corners {corners}: {len(per_section[0])} curves per section, per-curve mean removed", ""]
    results = []
    for section, curves in enumerate(per_section):
        x = np.array(curves)
        proto = x.mean(axis=0)
        u, s, vt = np.linalg.svd(x - proto, full_matrices=False)
        var = s**2 / np.sum(s**2)
        results.append((proto, s, vt, var, x))
        i_max, i_min = int(np.argmax(vt[0])), int(np.argmin(vt[0]))
        lines.append(
            f"S{section+1}: PC1 {var[0]*100:5.1f}%  PC2 {var[1]*100:5.1f}%  PC3 {var[2]*100:5.1f}%  "
            f"3 PCs {var[:3].sum()*100:5.1f}%   PC1 +{vt[0][i_max]:.2f} @ {hz[i_max]:.0f} Hz, {vt[0][i_min]:.2f} @ {hz[i_min]:.0f} Hz   "
            f"prototype span {proto.max()-proto.min():.1f} dB")
    (out / "pca_sections.txt").write_text("\n".join(lines) + "\n")
    print("\n".join(lines))

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 6, figsize=(22, 8))
    for section, (proto, s, vt, var, x) in enumerate(results):
        ax = axes[0, section]
        for row in x:
            ax.semilogx(hz, row, color="0.8", lw=0.4)
        ax.semilogx(hz, proto, "k", lw=2)
        ax.set_title(f"S{section+1} every curve + prototype")
        ax.set_ylim(-40, 40)
        ax.grid(True, which="both", alpha=0.3)
        ax = axes[1, section]
        sd = s / np.sqrt(len(x))
        ax.semilogx(hz, proto, "k", lw=1.5, label="prototype")
        for k in range(3):
            ax.semilogx(hz, proto + sd[k] * vt[k], lw=1, label=f"+1 sd PC{k+1} ({var[k]*100:.0f}%)")
        ax.set_title(f"S{section+1} principal deviations")
        ax.set_ylim(-40, 40)
        ax.legend(fontsize=7)
        ax.grid(True, which="both", alpha=0.3)
    fig.suptitle(f"per-section PCA, corners {corners}, {len(x)} curves each, 44.1 kHz, level removed")
    fig.tight_layout()
    fig.savefig(out / "pca_sections.png", dpi=100)
    print("plate:", out / "pca_sections.png")


if __name__ == "__main__":
    main()
