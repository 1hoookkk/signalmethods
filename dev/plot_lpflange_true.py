import json
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

with open("ref/morpheus/cubes_decoded.json") as f:
    d = json.load(f)

c = d["cubes"][1]
FS = 39062.5
GRID_N = 1000
GRID_HZ = np.logspace(np.log10(20), np.log10(16000), GRID_N)
W = 2 * np.pi * GRID_HZ / FS
Z1 = np.exp(-1j * W)
Z2 = np.exp(-2j * W)

fig = plt.figure(figsize=(16, 10), dpi=150)
fig.patch.set_facecolor("#030805")

# 4 Active corners of LPFlange.4 (Corners 1, 3, 5, 7)
corners_meta = [
    (1, "Corner C1 (Low Comb / Deep Sub Notches)", "#3fd9ff"),
    (5, "Corner C5 (Mid-Range Comb / Swept 2 Octaves Up)", "#57e86b"),
    (3, "Corner C3 (High Comb / Ultrasonic Shelf)", "#ffd33d"),
    (7, "Corner C7 (High-Q Comb Morph)", "#ff6a9a"),
]

for idx, (ci, title, col) in enumerate(corners_meta):
    co = c["corners"][ci]
    gain = co["gain"]
    
    # Left Column: Cascade Sum + Interleaved Stages
    ax1 = plt.subplot2grid((4, 2), (idx, 0))
    ax1.set_facecolor("#020704")
    
    total_h = np.full(GRID_N, gain, dtype=complex)
    for si, s in enumerate(co["sections"]):
        rp, rz = s["pole"]["r"], s["zero"]["r"]
        hp, hz = s["pole"]["hz"], s["zero"]["hz"]
        if rp == 0 and rz == 0:
            continue
        wp = 2 * np.pi * hp / FS
        wz = 2 * np.pi * hz / FS
        num = 1.0 - 2.0 * rz * np.cos(wz) * Z1 + (rz**2) * Z2
        den = 1.0 - 2.0 * rp * np.cos(wp) * Z1 + (rp**2) * Z2
        h_stage = num / np.where(np.abs(den) < 1e-12, 1e-12, den)
        
        s_db = 20 * np.log10(np.maximum(np.abs(h_stage), 1e-6))
        ax1.plot(GRID_HZ, s_db, linestyle="--", linewidth=0.6, alpha=0.35)
        total_h *= h_stage
        
    total_db = 20 * np.log10(np.maximum(np.abs(total_h), 1e-6))
    ax1.plot(GRID_HZ, total_db, color=col, linewidth=1.8, label="Cascade Total")
    ax1.set_xscale("log")
    ax1.set_xlim(20, 16000)
    ax1.set_ylim(-60, 25)
    ax1.set_title(f"LPFlange.4 · {title}", fontsize=8.5, color="#d0f0d8", pad=3)
    ax1.set_ylabel("dB", fontsize=7.5, color="#5c946e")
    ax1.set_xlabel("Hz", fontsize=7.5, color="#5c946e")
    ax1.axhline(0, color="#153820", linestyle="-", linewidth=0.8)

# Right Panel: Large Overlay of All 4 Active Corners Showing Morph Trajectory
ax2 = plt.subplot2grid((4, 2), (0, 1), rowspan=4)
ax2.set_facecolor("#020704")

for ci, title, col in corners_meta:
    co = c["corners"][ci]
    gain = co["gain"]
    total_h = np.full(GRID_N, gain, dtype=complex)
    for s in co["sections"]:
        rp, rz = s["pole"]["r"], s["zero"]["r"]
        hp, hz = s["pole"]["hz"], s["zero"]["hz"]
        if rp == 0 and rz == 0:
            continue
        wp = 2 * np.pi * hp / FS
        wz = 2 * np.pi * hz / FS
        num = 1.0 - 2.0 * rz * np.cos(wz) * Z1 + (rz**2) * Z2
        den = 1.0 - 2.0 * rp * np.cos(wp) * Z1 + (rp**2) * Z2
        h_stage = num / np.where(np.abs(den) < 1e-12, 1e-12, den)
        total_h *= h_stage
    total_db = 20 * np.log10(np.maximum(np.abs(total_h), 1e-6))
    ax2.plot(GRID_HZ, total_db, color=col, linewidth=1.8, label=f"C{ci}: {title.split('(')[1].replace(')', '')}")

ax2.set_xscale("log")
ax2.set_xlim(20, 16000)
ax2.set_ylim(-60, 25)
ax2.set_title("LPFlange.4 · Complete 4-Corner Morph Space (Comb Teeth Sweep)", fontsize=10, color="#d0f0d8", pad=6)
ax2.set_ylabel("dB", fontsize=8.5, color="#5c946e")
ax2.set_xlabel("Hz", fontsize=8.5, color="#5c946e")
ax2.axhline(0, color="#153820", linestyle="-", linewidth=0.8)
ax2.legend(loc="lower left", fontsize=7.5, facecolor="#051008", edgecolor="#11331a")

plt.tight_layout()
out_png = "plots/lpflange_true_anatomy.png"
plt.savefig(out_png, dpi=150, facecolor=fig.get_facecolor(), edgecolor="none")
print(f"Saved {out_png}")
