import os
import sys
import glob
import math
import json
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, ".")
import dev.analyze_corpus_sections as acs

# Load Morpheus ground truth decoded dataset
with open("ref/morpheus/cubes_decoded.json", "r") as f:
    morph_data = json.load(f)

FS = morph_data["law"]["hz_datum"]  # 39062.5 Hz
cubes = morph_data["cubes"]

GRID_N = 300
GRID_HZ = np.logspace(np.log10(40), np.log10(16000), GRID_N)

# Set up dark scientific theme
plt.style.use("dark_background")
plt.rcParams["font.family"] = "monospace"
plt.rcParams["font.size"] = 8
plt.rcParams["axes.grid"] = True
plt.rcParams["grid.color"] = "#15251a"
plt.rcParams["grid.linewidth"] = 0.5

# Convert poles/zeros into true direct form coefficients [b0, b1, b2, a1, a2]
# Using true gain law with stage normalization
def roots_to_biquad(s, sr=39062.5):
    rp = s["pole"]["r"]
    rz = s["zero"]["r"]
    hp = s["pole"]["hz"]
    hz = s["zero"]["hz"]
    
    if rp == 0.0 and rz == 0.0:
        return [1.0, 0.0, 0.0, 0.0, 0.0]
    
    wp = 2.0 * math.pi * hp / sr
    wz = 2.0 * math.pi * hz / sr
    
    a1 = -2.0 * rp * math.cos(wp)
    a2 = rp * rp
    b1 = -2.0 * rz * math.cos(wz)
    b2 = rz * rz
    
    # In ARMAdillo/Morpheus, individual sections are normalized so that dipole passbands sit near unity
    # while global corner gain applies to the complete product.
    # For a pole/zero pair, the peak/shelf gain normalization:
    if rz > 0:
        # Dipole section: gain at DC/Nyquist or geometric mean
        num_peak = max(1e-4, 1.0 - rz)
        den_peak = max(1e-4, 1.0 - rp)
        # Attenuation factor to keep cascade headroom stable
        scale = min(1.0, den_peak / num_peak) if den_peak < num_peak else 1.0
    else:
        # All-pole section
        scale = max(1e-4, 1.0 - rp)
        
    return [scale, scale * b1, scale * b2, a1, a2]

target_names = ["LPFlange.4", "Phaser", "Be-Ye.4", "AEParaVowel", "BrassyBlast", "HiQ 4PoleLP"]
selected_cubes = []
for tn in target_names:
    matched = next((c for c in cubes if tn.lower() in c["name"].lower()), None)
    if matched:
        selected_cubes.append(matched)

fig, axes = plt.subplots(len(selected_cubes), 2, figsize=(16, 3.2 * len(selected_cubes)), dpi=150)
fig.patch.set_facecolor("#030805")

for row_idx, c in enumerate(selected_cubes):
    name = c["name"].strip()
    c_idx = c["index"]
    is_dot_4 = ".4" in name
    
    # Active corner selection: for .4 cubes, odd corners (1,3,5,7) are active
    active_corners = [ci for ci, co in enumerate(c["corners"]) if any(s["pole"]["r"] > 0 for s in co["sections"])]
    rep_corner_idx = active_corners[0] if active_corners else 0
    rep_corner = c["corners"][rep_corner_idx]
    
    # Convert roots to biquads
    biquads = [roots_to_biquad(s, FS) for s in rep_corner["sections"]]
    global_gain = rep_corner["gain"]
    global_gain_db = 20.0 * math.log10(max(global_gain, 1e-4))
    
    # Left Subplot: Cascade Sum + Stages
    ax_left = axes[row_idx, 0]
    ax_left.set_facecolor("#020704")
    
    for si, b in enumerate(biquads):
        s_db = acs.biquad_response_db(b, GRID_HZ, FS)
        if np.any(np.abs(s_db) > 0.05):
            ax_left.plot(GRID_HZ, s_db, linestyle="--", linewidth=0.8, alpha=0.55, label=f"S{si+1}")
            
    cascade_db = acs.cascade_response_db(biquads, GRID_HZ, FS) + global_gain_db
    ax_left.plot(GRID_HZ, cascade_db, color="#3fd9ff", linewidth=2.0, label="Cascade Total")
    ax_left.set_xscale("log")
    ax_left.set_xlim(40, 16000)
    ax_left.set_ylim(-50, 30)
    ax_left.set_title(f"Cube #{c_idx}: {name} · [Corner C{rep_corner_idx}] (Gain={global_gain:.2f})", fontsize=8.5, color="#d0f0d8", pad=4)
    ax_left.set_ylabel("dB", fontsize=7.5, color="#5c946e")
    ax_left.set_xlabel("Hz", fontsize=7.5, color="#5c946e")
    ax_left.axhline(0, color="#153820", linestyle="-", linewidth=0.8)
    ax_left.legend(loc="upper right", fontsize=6.5, ncol=4, facecolor="#051008", edgecolor="#11331a")
    
    # Right Subplot: Superimposed Active Corners
    ax_right = axes[row_idx, 1]
    ax_right.set_facecolor("#020704")
    colors = ["#3fd9ff", "#57e86b", "#ffd33d", "#ff6a9a", "#a371f7", "#ff9b54", "#e87ba4", "#00d2ff"]
    
    for ci in active_corners:
        co = c["corners"][ci]
        c_biquads = [roots_to_biquad(s, FS) for s in co["sections"]]
        c_gain_db = 20.0 * math.log10(max(co["gain"], 1e-4))
        c_casc = acs.cascade_response_db(c_biquads, GRID_HZ, FS) + c_gain_db
        ax_right.plot(GRID_HZ, c_casc, color=colors[ci % len(colors)], linewidth=1.5, alpha=0.85, label=f"C{ci}")
        
    ax_right.set_xscale("log")
    ax_right.set_xlim(40, 16000)
    ax_right.set_ylim(-50, 30)
    ax_right.set_title(f"Cube #{c_idx}: {name} · Active Corners Superimposed ({len(active_corners)}/8 Active)", fontsize=8.5, color="#d0f0d8", pad=4)
    ax_right.set_ylabel("dB", fontsize=7.5, color="#5c946e")
    ax_right.set_xlabel("Hz", fontsize=7.5, color="#5c946e")
    ax_right.axhline(0, color="#153820", linestyle="-", linewidth=0.8)
    ax_right.legend(loc="upper right", fontsize=6.5, ncol=4, facecolor="#051008", edgecolor="#11331a")

plt.tight_layout()
out_png = "plots/groundtruth_canonical_morpheus_cubes.png"
plt.savefig(out_png, dpi=150, facecolor=fig.get_facecolor(), edgecolor="none")
print(f"Saved {out_png} successfully!")
