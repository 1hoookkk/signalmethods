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

fig, axes = plt.subplots(4, 1, figsize=(12, 10), dpi=150)
fig.patch.set_facecolor("#030805")

active_corners = [1, 3, 5, 7]
for idx, ci in enumerate(active_corners):
    co = c["corners"][ci]
    gain = co["gain"]
    
    total_h = np.full(GRID_N, gain, dtype=complex)
    ax = axes[idx]
    ax.set_facecolor("#020704")
    
    for si, s in enumerate(co["sections"]):
        rp = s["pole"]["r"]
        rz = s["zero"]["r"]
        hp = s["pole"]["hz"]
        hz = s["zero"]["hz"]
        
        if rp == 0 and rz == 0:
            continue
            
        wp = 2 * np.pi * hp / FS
        wz = 2 * np.pi * hz / FS
        
        # Raw monic transfer function: (1 - 2*rz*cos(wz)*z^-1 + rz^2*z^-2) / (1 - 2*rp*cos(wp)*z^-1 + rp^2*z^-2)
        num = 1.0 - 2.0 * rz * np.cos(wz) * Z1 + (rz**2) * Z2
        den = 1.0 - 2.0 * rp * np.cos(wp) * Z1 + (rp**2) * Z2
        h_stage = num / np.where(np.abs(den) < 1e-12, 1e-12, den)
        
        s_db = 20 * np.log10(np.maximum(np.abs(h_stage), 1e-6))
        ax.plot(GRID_HZ, s_db, linestyle="--", linewidth=0.6, alpha=0.4, label=f"S{si+1} (z={hz:.0f}Hz, p={hp:.0f}Hz)")
        total_h *= h_stage
        
    total_db = 20 * np.log10(np.maximum(np.abs(total_h), 1e-6))
    ax.plot(GRID_HZ, total_db, color="#3fd9ff", linewidth=1.8, label=f"Total Corner C{ci}")
    ax.set_xscale("log")
    ax.set_xlim(20, 16000)
    ax.set_ylim(-60, 20)
    ax.set_title(f"LPFlange.4 · Corner C{ci} (Gain={gain:.2f})", fontsize=9, color="#d0f0d8")
    ax.set_ylabel("dB", fontsize=8, color="#5c946e")
    ax.set_xlabel("Hz", fontsize=8, color="#5c946e")
    ax.axhline(0, color="#153820", linestyle="-", linewidth=0.8)
    ax.legend(loc="upper right", fontsize=6, ncol=4, facecolor="#051008", edgecolor="#11331a")

plt.tight_layout()
plt.savefig("plots/lpflange_pure_monic.png", dpi=150, facecolor=fig.get_facecolor(), edgecolor="none")
print("Saved plots/lpflange_pure_monic.png")
