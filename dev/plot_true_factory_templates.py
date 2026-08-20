import json, math, os, glob
import numpy as np
import matplotlib.pyplot as plt

FS = 39062.5
GRID = np.geomspace(40.0, 16000.0, 1024)

# 1. Canonical Bit-Exact Rossum SOS Evaluator (Identical to trench-core & render.js)
def eval_canonical_stage_db(fp, rp, fz, rz, scale=1.0, is_real=False):
    w = 2.0 * math.pi * GRID / FS
    cw = np.cos(w)
    sw = np.sin(w)
    c2 = cw * cw - sw * sw
    s2 = 2.0 * sw * cw
    
    # Numerator (Zeroes)
    if is_real or (rz < 1e-4 and fz < 1.0):
        # Identity or real
        b0 = scale
        b1 = 0.0
        b2 = 0.0
    else:
        th_z = 2.0 * math.pi * fz / FS
        b0 = scale
        b1 = -2.0 * rz * math.cos(th_z) * scale
        b2 = rz * rz * scale
        
    num_re = b0 + b1 * cw + b2 * c2
    num_im = b1 * sw + b2 * s2
    num_pwr = num_re**2 + num_im**2
    
    # Denominator (Poles)
    if rp < 1e-4 and fp < 1.0:
        a1 = 0.0
        a2 = 0.0
    else:
        th_p = 2.0 * math.pi * fp / FS
        a1 = -2.0 * rp * math.cos(th_p)
        a2 = rp * rp
        
    den_re = 1.0 + a1 * cw + a2 * c2
    den_im = a1 * sw + a2 * s2
    den_pwr = np.maximum(den_re**2 + den_im**2, 1e-18)
    
    return 10.0 * np.log10(np.maximum(num_pwr / den_pwr, 1e-12))

# Load actual factory presets from corpus
with open("ref/morpheus/cubes_decoded.json", "r", encoding="utf-8") as f:
    morph_data = json.load(f)
cubes = {c["name"]: c for c in morph_data["cubes"]}

p2k_files = glob.glob("recipes/architectures/P2k_*.json")
p2k_presets = {}
for p in p2k_files:
    d = json.load(open(p, "r", encoding="utf-8"))
    p2k_presets[d["name"]] = d

# Let's inspect the 6 canonical factory presets
targets = [
    ("TalkingHedz (P2K Vocal Formant)", "p2k", "TalkingHedz", 0, "#e74c3c"),
    ("BrsSwell2.4 (Morpheus Brass Ladder)", "morph", "BrsSwell2.4", 2, "#e67e22"),
    ("Flange3.4 (Morpheus All-Pass Flanger)", "morph", "Flange3.4", 1, "#9b59b6"),
    ("HOTwell.4 (Morpheus Metallic Bell)", "morph", "HOTwell.4", 2, "#3498db"),
    ("Ace of Bass (P2K Acid Synth LP)", "p2k", "Ace of Bass", 0, "#2ecc71"),
    ("Head Pan 1 (Morpheus 3D Spatial HRTF)", "morph", "Head Pan 1", 0, "#1abc9c")
]

plt.style.use('dark_background')
fig, axes = plt.subplots(3, 2, figsize=(18, 14), dpi=150)
axes = axes.flatten()

for idx, (title, kind, name, corner_idx, color) in enumerate(targets):
    ax = axes[idx]
    
    total_db = np.zeros_like(GRID)
    stage_curves = []
    
    if kind == "morph":
        c = cubes[name]
        cor = c["corners"][corner_idx]
        gain = cor.get("gain", 1.0)
        total_db += 20.0 * math.log10(max(gain, 1e-6))
        for si, s in enumerate(cor["sections"]):
            p = s["pole"]
            z = s["zero"]
            w = s["raw"]
            dead = (w[1] == 2047 and w[3] == 2047)
            if dead or (p["r"] < 0.05 and z["r"] < 0.05):
                continue
            s_db = eval_canonical_stage_db(p["hz"], p["r"], z["hz"], z["r"])
            stage_curves.append((f"S{si+1}", s_db))
            total_db += s_db
    else:
        p = p2k_presets[name]
        cname = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"][corner_idx]
        for si, s in enumerate(p["sections"]):
            g = s["corners"][cname]
            p_data = g["pole"]
            z_data = g["zero"]
            scale = g.get("scale", 1.0)
            
            if "pair" in p_data or "pair" in z_data:
                continue
            s_db = eval_canonical_stage_db(p_data["hz"], p_data["r"], z_data["hz"], z_data["r"], scale=scale)
            stage_curves.append((f"S{si+1}", s_db))
            total_db += s_db
            
    # Plot component stages
    for s_name, s_db in stage_curves:
        ax.plot(GRID, s_db, linestyle="--", linewidth=0.8, alpha=0.4)
        
    # Plot total compound cascade
    ax.plot(GRID, total_db, color=color, linewidth=2.4, label="Total Cascade H(z)")
    
    ax.set_xscale("log")
    ax.set_xlim(40.0, 16000.0)
    ax.set_ylim(-60.0, 30.0)
    
    ax.set_xticks([40, 100, 200, 500, 1000, 2000, 5000, 10000, 16000])
    ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
    
    ax.axhline(0, color="#444444", linestyle=":", linewidth=1.0)
    ax.grid(True, which="major", color="#2a2a2a", linestyle="-", linewidth=0.7)
    ax.grid(True, which="minor", color="#1c1c1c", linestyle=":", linewidth=0.5)
    
    ax.set_title(f"{title}", fontsize=11, fontweight="bold", pad=8, color="#ffffff")
    ax.set_xlabel("Frequency (Hz)", fontsize=9, color="#aaaaaa")
    ax.set_ylabel("Magnitude (dB)", fontsize=9, color="#aaaaaa")
    ax.tick_params(colors="#888888", labelsize=8)

plt.tight_layout()
out_png = "plots/corpus/true_factory_canonical_templates.png"
os.makedirs("plots/corpus", exist_ok=True)
plt.savefig(out_png, dpi=150)
print(f"SUCCESS: Saved true factory templates plot to {out_png}")
