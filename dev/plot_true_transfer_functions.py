import os
import glob
import math
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import sys
sys.path.insert(0, ".")
import dev.analyze_corpus_sections as acs

# 1. Load the binary corpus using the verified bit-exact loader
corpus, xml_corpus = acs.load_all_corpora()
print(f"Loaded {len(corpus)} binary entities from dev corpus loader.")

GRID_N = 300
GRID_HZ = np.logspace(np.log10(40), np.log10(16000), GRID_N)

# Set up clean dark-chassis plotting style
plt.style.use("dark_background")
plt.rcParams["font.family"] = "monospace"
plt.rcParams["font.size"] = 8
plt.rcParams["axes.grid"] = True
plt.rcParams["grid.color"] = "#15251a"
plt.rcParams["grid.linewidth"] = 0.5

# Select 6 key presets across Morpheus 7-stage (.body) and P2k 6-stage (.bin)
targets = [
    ("preset:P2k_001_MegaSweepz", "P2K #001 MegaSweepz (6 Stages, Fs=39062.5 Hz)", 0),
    ("preset:P2k_010_Ooh-To-Eee", "P2K #010 Ooh-To-Eee (6 Stages, Fs=39062.5 Hz)", 0),
    ("preset:P2k_033_Deep_Liquid", "P2K #033 Deep Liquid (6 Stages, Fs=39062.5 Hz)", 0),
    ("cube:TalkingHedz", "Morpheus TalkingHedz (7 Stages, Fs=44100 Hz)", 0),
    ("cube:LPFlange.4", "Morpheus LPFlange.4 (7 Stages, Fs=44100 Hz)", 1), # Active corner 1
    ("cube:VocalCube", "Morpheus VocalCube (7 Stages, Fs=44100 Hz)", 0),
]

# If exact targets not in corpus keys, search by substring
selected_items = []
for t_key, t_title, c_idx in targets:
    matched_key = next((k for k in corpus.keys() if t_key.lower() in k.lower()), None)
    if not matched_key:
        matched_key = next((k for k in corpus.keys() if t_key.split(":")[-1].lower() in k.lower()), None)
    if matched_key:
        selected_items.append((matched_key, t_title, c_idx))

if not selected_items:
    # Fallback to first 6 items in corpus
    selected_items = [(k, k, 0) for k in list(corpus.keys())[:6]]

print(f"Selected {len(selected_items)} patches for true transfer function plotting:")
for k, t, c in selected_items:
    print(f"  - {k} -> {t}")

fig, axes = plt.subplots(len(selected_items), 2, figsize=(16, 3.2 * len(selected_items)), dpi=150)
fig.patch.set_facecolor("#030805")

for row_idx, (k, title, c_idx) in enumerate(selected_items):
    item = corpus[k]
    sr = item["datum_sr"]
    corner_name, stage_words = item["corners"][c_idx]
    
    # Decode 5 words per stage into true direct-form biquad coefficients [b0, b1, b2, a1, a2]
    biquads = [acs.words_to_biquad(w) for w in stage_words]
    
    # Left Subplot: Cascade Sum + Individual Stages
    ax_left = axes[row_idx, 0] if len(selected_items) > 1 else axes[0]
    ax_left.set_facecolor("#020704")
    
    stage_dbs = []
    for si, b in enumerate(biquads):
        s_db = acs.biquad_response_db(b, GRID_HZ, sr)
        stage_dbs.append(s_db)
        if np.any(np.abs(s_db) > 0.05):
            ax_left.plot(GRID_HZ, s_db, linestyle="--", linewidth=0.8, alpha=0.55, label=f"S{si+1}")
            
    cascade_db = acs.cascade_response_db(biquads, GRID_HZ, sr)
    ax_left.plot(GRID_HZ, cascade_db, color="#3fd9ff", linewidth=2.0, label="Cascade Total")
    ax_left.set_xscale("log")
    ax_left.set_xlim(40, 16000)
    ax_left.set_ylim(-45, 30)
    ax_left.set_title(f"{title} · [{corner_name}] Total Cascade & Stages", fontsize=8.5, color="#d0f0d8", pad=4)
    ax_left.set_ylabel("dB", fontsize=7.5, color="#5c946e")
    ax_left.set_xlabel("Hz", fontsize=7.5, color="#5c946e")
    ax_left.axhline(0, color="#153820", linestyle="-", linewidth=0.8)
    ax_left.legend(loc="upper right", fontsize=6.5, ncol=4, facecolor="#051008", edgecolor="#11331a")
    
    # Right Subplot: 4-Corner Morph Overlay (Corner Evolution)
    ax_right = axes[row_idx, 1] if len(selected_items) > 1 else axes[1]
    ax_right.set_facecolor("#020704")
    
    colors_corners = ["#3fd9ff", "#57e86b", "#ffd33d", "#ff6a9a", "#a371f7", "#ff9b54", "#e87ba4", "#00d2ff"]
    for ci, (cname, c_swords) in enumerate(item["corners"]):
        c_biquads = [acs.words_to_biquad(w) for w in c_swords]
        c_casc = acs.cascade_response_db(c_biquads, GRID_HZ, sr)
        col = colors_corners[ci % len(colors_corners)]
        ax_right.plot(GRID_HZ, c_casc, color=col, linewidth=1.5, alpha=0.85, label=f"{cname}")
        
    ax_right.set_xscale("log")
    ax_right.set_xlim(40, 16000)
    ax_right.set_ylim(-45, 30)
    ax_right.set_title(f"{title} · All Corners Superimposed", fontsize=8.5, color="#d0f0d8", pad=4)
    ax_right.set_ylabel("dB", fontsize=7.5, color="#5c946e")
    ax_right.set_xlabel("Hz", fontsize=7.5, color="#5c946e")
    ax_right.axhline(0, color="#153820", linestyle="-", linewidth=0.8)
    ax_right.legend(loc="upper right", fontsize=6.5, ncol=4, facecolor="#051008", edgecolor="#11331a")

plt.tight_layout()
out_png = "plots/groundtruth_true_transfer_functions.png"
plt.savefig(out_png, dpi=150, facecolor=fig.get_facecolor(), edgecolor="none")
print(f"Saved {out_png} successfully!")
