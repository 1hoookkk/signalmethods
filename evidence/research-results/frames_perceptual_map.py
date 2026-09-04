import csv
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "frames_responses.csv")
OUT_PNG = os.path.join(HERE, "frames_perceptual_map.png")
OUT_TXT = os.path.join(HERE, "frames_perceptual_map.txt")
OUT_CSV = os.path.join(HERE, "frames_perceptual_map.csv")

rows = list(csv.reader(open(SRC, encoding="utf-8")))
hz = np.array([float(v) for v in rows[0][2:]])
groups = [r[0] for r in rows[1:]]
names = [r[1] for r in rows[1:]]
X = np.array([[float(v) for v in r[2:]] for r in rows[1:]])
X = np.clip(X, -40.0, 40.0)
mean = X.mean(axis=0)
U, S, Vt = np.linalg.svd(X - mean, full_matrices=False)
var = S ** 2 / np.sum(S ** 2)
scores = (X - mean) @ Vt[:2].T
if Vt[0] @ np.where(hz > 2000, 1.0, -1.0) < 0:
    Vt[0] *= -1; scores[:, 0] *= -1
if np.corrcoef(scores[:, 1], (X.max(axis=1) - np.median(X, axis=1)))[0, 1] < 0:
    Vt[1] *= -1; scores[:, 1] *= -1

centroid = np.exp((10 ** (X / 20) * np.log(hz)).sum(axis=1) / (10 ** (X / 20)).sum(axis=1))
peak = X.max(axis=1) - np.median(X, axis=1)
lines = ["%d frames x %d points; variance by component: %s" % (X.shape[0], X.shape[1], ", ".join("%.1f%%" % (100 * v) for v in var[:6])),
         "axis 1 vs log centroid r = %.2f; axis 2 vs peak-over-median r = %.2f" % (np.corrcoef(scores[:, 0], np.log(centroid))[0, 1], np.corrcoef(scores[:, 1], peak)[0, 1])]
for g in sorted(set(groups)):
    idx = [i for i, x in enumerate(groups) if x == g]
    c = scores[idx].mean(axis=0); s = scores[idx].std(axis=0)
    lines.append("%-12s n=%3d  axis1 %6.1f +- %5.1f  axis2 %6.1f +- %5.1f" % (g, len(idx), c[0], s[0], c[1], s[1]))
open(OUT_TXT, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("\n".join(lines))
with open(OUT_CSV, "w", encoding="utf-8", newline="") as f:
    w = csv.writer(f); w.writerow(["group", "name", "axis1", "axis2", "centroid_hz", "peak_db"])
    for i in range(len(names)):
        w.writerow([groups[i], names[i], "%.3f" % scores[i, 0], "%.3f" % scores[i, 1], "%.0f" % centroid[i], "%.1f" % peak[i]])

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

fig, axes = plt.subplots(1, 2, figsize=(15, 7), gridspec_kw={"width_ratios": [2, 1]})
ax = axes[0]
palette = {"BANK": "#222222", "TYPES": "#888888", "HEAD": "#1f77b4", "XL-1 AUD": "#d62728", "VOWEL DVTD": "#2ca02c",
           "VOWEL H95": "#9467bd", "BODY": "#8c564b", "INSTRUMENT": "#e377c2", "VOWEL": "#17becf"}
for g in sorted(set(groups)):
    idx = [i for i, x in enumerate(groups) if x == g]
    ax.scatter(scores[idx, 0], scores[idx, 1], s=18, c=palette.get(g, "#000000"), label="%s %d" % (g, len(idx)), alpha=0.8)
for i in range(len(names)):
    if groups[i] == "BANK" and (names[i].endswith("M0 Q0") or names[i].endswith("M1 Q0")):
        ax.annotate(names[i].split(" · ")[0][:10] + names[i][-6:], (scores[i, 0], scores[i, 1]), fontsize=5, alpha=0.6)
ax.axhline(0, color="#cccccc", lw=0.5); ax.axvline(0, color="#cccccc", lw=0.5)
ax.set_xlabel("axis 1 (%.0f%%): dark to bright" % (100 * var[0])); ax.set_ylabel("axis 2 (%.0f%%): flat to peaked" % (100 * var[1]))
ax.set_title("every frame on the first two axes of its own response"); ax.legend(fontsize=7, loc="best")
ax2 = axes[1]
for k in range(3):
    ax2.semilogx(hz, Vt[k] * S[k] / np.sqrt(X.shape[0]), label="axis %d (%.0f%%)" % (k + 1, 100 * var[k]))
ax2.semilogx(hz, mean, color="#999999", ls="--", label="mean frame")
ax2.axhline(0, color="#cccccc", lw=0.5); ax2.set_xlabel("Hz"); ax2.set_ylabel("dB per one standard score"); ax2.legend(fontsize=8)
ax2.set_title("what each axis does to the curve")
fig.tight_layout(); fig.savefig(OUT_PNG, dpi=130)
print(OUT_PNG)
