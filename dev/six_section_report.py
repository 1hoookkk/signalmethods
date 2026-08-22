"""Aggregate six_section_fit.py shard outputs into per-body verdicts."""
import glob
import json
import sys

import numpy as np

by_name = {}
for pattern in sys.argv[1:]:
    for path in sorted(glob.glob(pattern)):
        for body in json.load(open(path)):
            if body["corners"]:
                by_name[body["file"]] = body
bodies = list(by_name.values())

print(f"{'body':<24}{'corners':>8}{'ctl max':>9}{'six max rms':>12}{'six max peak':>13}{'packed max':>11}"
      f"{'axis0':>7}{'axis1':>7}{'axis2':>7}")
rows = []
for b in bodies:
    cs = b["corners"]
    if not cs:
        continue
    ctl = max(c["control7_rms"] for c in cs)
    six = max(c["six_rms"] for c in cs)
    peak = max(c["six_max"] for c in cs)
    packed = max(c["six_packed_rms"] for c in cs if np.isfinite(c["six_packed_rms"]))
    excess = max(c["six_rms"] - c["control7_rms"] for c in cs)
    rows.append((b["name"], len(cs), ctl, six, peak, packed, excess, b["axis_rms"]))
rows.sort(key=lambda r: -r[3])
for name, n, ctl, six, peak, packed, excess, ax in rows:
    print(f"{name:<24}{n:>8}{ctl:>9.2f}{six:>12.2f}{peak:>13.2f}{packed:>11.2f}"
          f"{ax[0]:>7.1f}{ax[1]:>7.1f}{ax[2]:>7.1f}")

six = np.asarray([r[3] for r in rows])
ctl = np.asarray([r[2] for r in rows])
exc = np.asarray([r[6] for r in rows])
ax = np.asarray([r[7] for r in rows])
print(f"\nbodies {len(rows)}")
print(f"six-section worst-corner rms: median {np.median(six):.2f}  p90 {np.percentile(six, 90):.2f}  max {six.max():.2f}")
print(f"bodies with worst-corner six rms < 1 dB: {(six < 1.0).sum()}   1-3 dB: {((six >= 1) & (six < 3)).sum()}   >= 3 dB: {(six >= 3).sum()}")
print(f"control (7-section) failed to reach 0.1 dB on: {(ctl > 0.1).sum()} bodies")
print(f"six minus control (the part attributable to the missing section): median {np.median(exc):.2f}  max {exc.max():.2f}")
print(f"corner-axis rms dB, median over bodies: bit0 {np.median(ax[:, 0]):.1f}  bit1 {np.median(ax[:, 1]):.1f}  bit2 {np.median(ax[:, 2]):.1f}")
print(f"bodies whose quietest axis moves the response < 1 dB rms: {(ax.min(axis=1) < 1.0).sum()}; < 3 dB: {(ax.min(axis=1) < 3.0).sum()}")
print("quietest axis per body:", np.bincount(ax.argmin(axis=1), minlength=3))
