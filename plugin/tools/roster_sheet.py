import ctypes
import math
import re
import sys
from pathlib import Path

import numpy as np

ROOT = Path(r"C:\Users\hooki\trench-workstation")
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, RT_DOUBLES  # noqa: E402

GRID = np.geomspace(20.0, 20_000.0, 300)
RATE = 44_100.0

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int

def response(body, m, q):
    z1 = np.exp(-1j * 2 * np.pi * GRID / RATE)
    z2 = z1 * z1
    c = (ctypes.c_double * RT_DOUBLES)()
    mr = ctypes.c_double(); un = ctypes.c_uint32(); nf = ctypes.c_uint32()
    buf = ctypes.create_string_buffer(body, 240)
    if lib.trench_packed_probe_at(buf, 240, m, q, RATE, c, ctypes.byref(mr),
                                  ctypes.byref(un), ctypes.byref(nf)) != 0 \
       or un.value or nf.value:
        return np.full_like(GRID, -60.0)
    cc = np.ctypeslib.as_array(c).reshape(6, 5)
    h = np.ones_like(z1)
    for b0, b1, b2, a1, a2 in cc:
        h = h * (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)
    return 20 * np.log10(np.maximum(np.abs(h), 1e-12))

entries = []
for inc in ("PresetRoster.inc", "PresetRosterSignature.inc"):
    for line in (ROOT / "plugin" / "presets" / inc).read_text().splitlines():
        m = re.match(r'\s*TRENCH_PRESET\s*\("([^"]+)",\s*"([^"]+)",\s*"([^"]+)"', line)
        if m:
            entries.append((m.group(1), m.group(2), m.group(3)))

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
cmap = plt.get_cmap("YlOrRd_r")

n = len(entries)
ncols = 5
nrows = math.ceil(n / ncols)
fig, axes = plt.subplots(nrows, ncols, figsize=(19, 2.6 * nrows),
                         facecolor="#141414")
for ax in axes.flat[n:]:
    ax.axis("off")
missing = []
for k, (ax, (name, stem, cat)) in enumerate(zip(axes.flat, entries)):
    ax.set_facecolor("#141414")
    p = ROOT / "plugin" / "presets" / "bodies" / f"{stem}.body240"
    if not p.exists():
        missing.append(name)
        ax.text(0.5, 0.5, "missing body", color="0.6", ha="center",
                transform=ax.transAxes, fontsize=8)
    else:
        body = p.read_bytes()
        for j, m in enumerate(np.linspace(0, 1, 9)):
            ax.semilogx(GRID, response(body, float(m), 0.0),
                        color=cmap(j / 8), lw=0.9, alpha=0.9)
        ax.semilogx(GRID, response(body, 0.5, 1.0), color="#2bd8c3",
                    lw=1.0, ls="--", alpha=0.8)
    ax.set_xlim(20, 20_000); ax.set_ylim(-60, 30)
    ax.set_xticks([]); ax.set_yticks([])
    for sp in ax.spines.values():
        sp.set_color("0.3")
    ax.set_title(f"{k+1:02d}  {name}", fontsize=8.5, color="white")
    ax.text(0.985, 0.04, cat, transform=ax.transAxes, ha="right",
            fontsize=6, color="0.55")
fig.suptitle("THE SHIPPED MENU - every roster preset. Curves = MORPH ride at Q0 "
             "(dark down, pale up); teal dash = M50 Q100. Verdict sheet: "
             "presets_ship_v1/PLUGIN_PRESET_VERDICTS.md", fontsize=12, color="white")
fig.tight_layout(rect=[0, 0, 1, 0.985])
out = Path(__file__).with_name("roster_sheet.png")
fig.savefig(out, dpi=115, facecolor=fig.get_facecolor())
print(f"{n} presets, {len(missing)} missing bodies: {missing}")
print(out)

lines = ["# PLUGIN PRESET VERDICTS - yes ships, no dies (one strike)", "",
         "Mark y or n. Anything without a verdict does not ship.", "",
         "| # | preset | category | verdict |", "|---|---|---|---|"]
for k, (name, stem, cat) in enumerate(entries, 1):
    lines.append(f"| {k:02d} | {name} | {cat} |  |")
(ROOT / "presets_ship_v1" / "PLUGIN_PRESET_VERDICTS.md").write_text(
    "\n".join(lines), encoding="utf-8")
print("verdict sheet written")
