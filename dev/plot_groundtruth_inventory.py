import json
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from collections import Counter

# Load decoded ground truth
with open("ref/morpheus/cubes_decoded.json", "r") as f:
    data = json.load(f)

FS = data["law"]["hz_datum"]  # 39062.5 Hz
cubes = data["cubes"]

# Set up matplotlib style for dense scientific workstation look
plt.style.use("dark_background")
plt.rcParams["font.family"] = "monospace"
plt.rcParams["font.size"] = 8
plt.rcParams["axes.grid"] = True
plt.rcParams["grid.color"] = "#15251a"
plt.rcParams["grid.linewidth"] = 0.5

fig = plt.figure(figsize=(18, 12), dpi=150)
fig.patch.set_facecolor("#030805")

# -----------------------------------------------------------------------------
# 1. Direct Form Biquad Evaluation at Native Fs
# -----------------------------------------------------------------------------
GRID_N = 256
GRID_HZ = np.logspace(np.log10(40), np.log10(16000), GRID_N)
W = 2 * np.pi * GRID_HZ / FS
CW = np.cos(W)
SW = np.sin(W)
C2 = np.cos(2 * W)
S2 = np.sin(2 * W)

def eval_section(s):
    rp = s["pole"]["r"]
    rz = s["zero"]["r"]
    thetap = 2 * np.pi * s["pole"]["hz"] / FS
    thetaz = 2 * np.pi * s["zero"]["hz"] / FS
    
    if rp == 0 and rz == 0:
        return np.zeros(GRID_N)
    
    a1 = -2 * rp * np.cos(thetap)
    a2 = rp * rp
    b1 = -2 * rz * np.cos(thetaz)
    b2 = rz * rz
    b0 = 1.0
    
    # Direct Form frequency response
    num_re = b0 + b1 * CW + b2 * C2
    num_im = -(b1 * SW + b2 * S2)
    den_re = 1.0 + a1 * CW + a2 * C2
    den_im = -(a1 * SW + a2 * S2)
    
    power = (num_re**2 + num_im**2) / np.maximum(den_re**2 + den_im**2, 1e-18)
    return 10 * np.log10(np.maximum(power, 1e-12))

def eval_corner(corner):
    gain_db = 20 * np.log10(max(corner["gain"], 1e-6))
    sum_db = np.full(GRID_N, gain_db)
    stages_db = []
    for s in corner["sections"]:
        s_db = eval_section(s)
        stages_db.append(s_db)
        sum_db += s_db
    return sum_db, stages_db

# -----------------------------------------------------------------------------
# TOP ROW: 4 GROUND-TRUTH PATCH TRANSFER FUNCTIONS (Native Fs, No DC Pinning)
# -----------------------------------------------------------------------------
exemplar_cubes = [
    (1, "LPFlange.4", 1, "C1 (Odd Plane)", "#3fd9ff"),
    (225, "Phaser", 0, "C0 (Base Plane)", "#57e86b"),
    (0, "AEParaVowel", 0, "C0 (Formant Stack)", "#ffd33d"),
    (13, "Vocal Cube", 1, "C1 (Talking Vocal)", "#ff6a9a"),
]

for idx, (c_id, target_name, c_num, label, color) in enumerate(exemplar_cubes):
    ax = plt.subplot2grid((3, 4), (0, idx))
    ax.set_facecolor("#020704")
    
    # Find matching cube
    matched = next((c for c in cubes if c["index"] == c_id or target_name.lower() in c["name"].lower()), None)
    if matched:
        corner = matched["corners"][c_num]
        sum_db, stages_db = eval_corner(corner)
        
        # Plot individual stages as thin dashed lines
        for si, s_db in enumerate(stages_db):
            if np.any(np.abs(s_db) > 0.1):
                ax.plot(GRID_HZ, s_db, color="#3a6048", linewidth=0.8, linestyle="--", alpha=0.6)
                
        # Plot full cascade product
        ax.plot(GRID_HZ, sum_db, color=color, linewidth=2.0, label="Cascade Sum")
        ax.set_title(f"Cube #{matched['index']}: {matched['name']}\n{label} · Native Fs={FS}Hz", fontsize=8, color="#d0f0d8", pad=4)
    
    ax.set_xscale("log")
    ax.set_xlim(40, 16000)
    ax.set_ylim(-60, 40)
    ax.set_xlabel("Hz", fontsize=7, color="#5c946e")
    ax.set_ylabel("dB", fontsize=7, color="#5c946e")
    ax.axhline(0, color="#153820", linestyle="-", linewidth=0.8)
    ax.axhline(36, color="#ff4444", linestyle=":", linewidth=0.8)

# -----------------------------------------------------------------------------
# MIDDLE ROW: BITMASK DISTRIBUTION & PER-LANE FACTOR COMPOSITION
# -----------------------------------------------------------------------------
# Panel E: Corner Bitmasks
ax_mask = plt.subplot2grid((3, 4), (1, 0), colspan=2)
ax_mask.set_facecolor("#020704")
masks = Counter()
for c in cubes:
    active_mask = 0
    for ci, co in enumerate(c["corners"]):
        if any(s["pole"]["r"] > 0 or s["zero"]["r"] > 0 for s in co["sections"]):
            active_mask |= (1 << ci)
    masks[f"{active_mask:08b}"] += 1

labels = [f"Mask [{m}]\n({m.count('1')}/8 Corners Active)" for m, _ in masks.most_common()]
counts = [count for _, count in masks.most_common()]
colors_bar = ["#57e86b", "#3fd9ff", "#ffa028"]

bars = ax_mask.bar(labels, counts, color=colors_bar, width=0.45, edgecolor="#11331a")
ax_mask.set_title("1. Corpus-Wide Corner Bitmasks (289 Cubes / 2,312 Corners)", fontsize=9, color="#d0f0d8")
ax_mask.set_ylabel("Number of Cubes", fontsize=8, color="#5c946e")
for bar in bars:
    yval = bar.get_height()
    ax_mask.text(bar.get_x() + bar.get_width()/2.0, yval + 3, f"{int(yval)} cubes\n({yval/289*100:.1f}%)", ha='center', va='bottom', fontsize=7.5, color="#ffffff")
ax_mask.set_ylim(0, 240)

# Panel F: Per-Stage Pole/Zero Geometry Composition
ax_stages = plt.subplot2grid((3, 4), (1, 2), colspan=2)
ax_stages.set_facecolor("#020704")

lanes_active = np.zeros((7, 3)) # [Dipole, Pole-Only, Identity]
for c in cubes:
    for co in c["corners"]:
        for si, s in enumerate(co["sections"]):
            rp = s["pole"]["r"]
            rz = s["zero"]["r"]
            if rp > 0 and rz > 0:
                lanes_active[si, 0] += 1
            elif rp > 0 and rz == 0:
                lanes_active[si, 1] += 1
            else:
                lanes_active[si, 2] += 1

stages_x = [f"Stage S{i+1}" for i in range(7)]
p1 = ax_stages.bar(stages_x, lanes_active[:, 0], label="Dipole Pair (Pole+Zero)", color="#3fd9ff", width=0.5)
p2 = ax_stages.bar(stages_x, lanes_active[:, 1], bottom=lanes_active[:, 0], label="Pole-Only (Zero=0)", color="#ffd33d", width=0.5)
p3 = ax_stages.bar(stages_x, lanes_active[:, 2], bottom=lanes_active[:, 0] + lanes_active[:, 1], label="Identity / Idle", color="#1a3525", width=0.5)

ax_stages.set_title("2. Per-Stage Factor Geometry (100% S7 Zero Disabled Law)", fontsize=9, color="#d0f0d8")
ax_stages.set_ylabel("Section Count across 2,312 Corners", fontsize=8, color="#5c946e")
ax_stages.legend(loc="upper right", fontsize=7, facecolor="#051008", edgecolor="#11331a")
ax_stages.set_ylim(0, 2600)

# Highlight S7 100% pole-only law
ax_stages.text(6, 2350, "S7 ZERO IS 100% DISABLED\n(1,816/1,816 Active Corners)", ha="center", fontsize=7, color="#ff4444", fontweight="bold")

# -----------------------------------------------------------------------------
# BOTTOM ROW: HARMONIC DIPOLE INTERVAL RATIOS & POLE SPACING
# -----------------------------------------------------------------------------
ax_ratios = plt.subplot2grid((3, 4), (2, 0), colspan=2)
ax_ratios.set_facecolor("#020704")

ratios = []
for c in cubes:
    for co in c["corners"]:
        for s in co["sections"]:
            hp = s["pole"]["hz"]
            hz = s["zero"]["hz"]
            rp = s["pole"]["r"]
            rz = s["zero"]["r"]
            if rp > 0 and rz > 0 and hp > 20:
                ratios.append(hz / hp)

ax_ratios.hist(ratios, bins=120, range=(0, 2.5), color="#33ff85", edgecolor="#030805", alpha=0.85)
ax_ratios.set_title("3. Intra-Section Zero/Pole Frequency Ratios (fz / fp)", fontsize=9, color="#d0f0d8")
ax_ratios.set_xlabel("Ratio (fz / fp)", fontsize=8, color="#5c946e")
ax_ratios.set_ylabel("Occurrences", fontsize=8, color="#5c946e")

# Annotate prominent musical intervals
ax_ratios.axvline(0.5, color="#ffd33d", linestyle="--", linewidth=1, label="1/2 Octave (0.50)")
ax_ratios.axvline(0.667, color="#3fd9ff", linestyle="--", linewidth=1, label="2/3 Fifth (0.67)")
ax_ratios.axvline(0.80, color="#ff6a9a", linestyle="--", linewidth=1, label="4/5 Major 3rd (0.80)")
ax_ratios.axvline(1.0, color="#ff4444", linestyle="--", linewidth=1, label="1.0 Unison / Parked Dipole")
ax_ratios.legend(loc="upper right", fontsize=7, facecolor="#051008", edgecolor="#11331a")

# Panel H: Inter-Pole Semitone Spacing
ax_spacing = plt.subplot2grid((3, 4), (2, 2), colspan=2)
ax_spacing.set_facecolor("#020704")

spacings = []
for c in cubes:
    for co in c["corners"]:
        active_poles = [s["pole"]["hz"] for s in co["sections"] if s["pole"]["r"] > 0.1 and s["pole"]["hz"] > 40]
        active_poles.sort()
        for i in range(len(active_poles) - 1):
            if active_poles[i] > 0:
                st = 12 * np.log2(active_poles[i+1] / active_poles[i])
                if st < 36:
                    spacings.append(st)

ax_spacing.hist(spacings, bins=72, range=(0, 36), color="#57e86b", edgecolor="#030805", alpha=0.85)
ax_spacing.set_title("4. Consecutive Pole Spacing (Semitones between Adjacent Poles)", fontsize=9, color="#d0f0d8")
ax_spacing.set_xlabel("Interval (Semitones)", fontsize=8, color="#5c946e")
ax_spacing.set_ylabel("Occurrences", fontsize=8, color="#5c946e")

ax_spacing.axvline(0, color="#ff4444", linestyle="--", linewidth=1, label="0 st (Stacked Pole / Doublet)")
ax_spacing.axvline(6, color="#3fd9ff", linestyle="--", linewidth=1, label="6 st (sqrt(2) Half-Octave)")
ax_spacing.axvline(12, color="#ffd33d", linestyle="--", linewidth=1, label="12 st (Exact 1 Octave)")
ax_spacing.legend(loc="upper right", fontsize=7, facecolor="#051008", edgecolor="#11331a")

plt.tight_layout()
plt.savefig("plots/groundtruth_corpus_inventory.png", dpi=150, facecolor=fig.get_facecolor(), edgecolor="none")
print("Saved plots/groundtruth_corpus_inventory.png successfully.")
