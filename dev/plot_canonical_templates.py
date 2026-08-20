import json, math, os
import numpy as np
import matplotlib.pyplot as plt

FS = 39062.5
NYQ = FS / 2.0
GRID = np.geomspace(40.0, 16000.0, 1024)
W = 2.0 * math.pi * GRID / FS
CW = np.cos(W)
SW = np.sin(W)
C2W = CW * CW - SW * SW
S2W = 2.0 * SW * CW

# Evaluator
def eval_biquad_db(fp, rp, fz, rz, is_real=False):
    if is_real:
        # Real pair: (1 - a z^-1)(1 - b z^-1)
        a, b = fp, rp # passed in
        num_re = (1.0 - a * CW) * (1.0 - b * CW) - (a * SW) * (b * SW)
        num_im = -(a + b) * SW + a * b * S2W
        den_re, den_im = 1.0, 0.0
        num_pwr = num_re**2 + num_im**2
        den_pwr = 1.0
    else:
        # Numerator (Zero)
        if rz > 1e-4:
            th_z = 2.0 * math.pi * fz / FS
            b1 = -2.0 * rz * math.cos(th_z)
            b2 = rz * rz
            num_re = 1.0 + b1 * CW + b2 * C2W
            num_im = b1 * SW + b2 * S2W
            num_pwr = num_re**2 + num_im**2
        else:
            num_pwr = 1.0
            
        # Denominator (Pole)
        if rp > 1e-4:
            th_p = 2.0 * math.pi * fp / FS
            a1 = -2.0 * rp * math.cos(th_p)
            a2 = rp * rp
            den_re = 1.0 + a1 * CW + a2 * C2W
            den_im = a1 * SW + a2 * S2W
            den_pwr = np.maximum(den_re**2 + den_im**2, 1e-18)
        else:
            den_pwr = 1.0
            
    return 10.0 * np.log10(np.maximum(num_pwr / den_pwr, 1e-12))

# Load the 6 templates
scaffolds = [
    {
        "id": "T1",
        "title": "Template 1: Vocal Formant Scaffold",
        "subtitle": "5-Formant vocal tract with slaved guard valleys & S1 headroom shield",
        "color": "#e74c3c",
        "stages": [
            {"name": "S1 (Headroom Shield)", "fp": 9320.9, "rp": 0.9753, "fz": 346.7, "rz": 0.9354},
            {"name": "S2 (Formant F1)",       "fp": 890.7,  "rp": 0.9793, "fz": 1113.2, "rz": 0.9458},
            {"name": "S3 (Formant F2)",       "fp": 1569.6, "rp": 0.9773, "fz": 2014.4, "rz": 0.9602},
            {"name": "S4 (Formant F3)",       "fp": 2348.0, "rp": 0.9969, "fz": 2971.3, "rz": 0.9723},
            {"name": "S5 (Nasal / Air)",      "fp": 4606.9, "rp": 0.9479, "fz": 7921.9, "rz": 0.7501},
            {"name": "S6 (Throat / Pinna)",   "fp": 199.4,  "rp": 0.9912, "fz": 6396.3, "rz": 1.0000},
            {"name": "S7 (High Air Cap)",     "fp": 16000.0,"rp": 0.8500, "fz": 0.0,    "rz": 0.0000},
        ],
        "gain_db": -5.0
    },
    {
        "id": "T2",
        "title": "Template 2: Interleaved Crossover Ladder",
        "subtitle": "Acoustic body where Zero(S_k) = Pole(S_k+1) at ratio ~1.20",
        "color": "#e67e22",
        "stages": [
            {"name": "S1 (148Hz -> 194Hz)",   "fp": 148.4, "rp": 0.9882, "fz": 194.2, "rz": 0.9993},
            {"name": "S2 (194Hz -> 238Hz)",   "fp": 194.2, "rp": 0.9998, "fz": 238.3, "rz": 0.9989},
            {"name": "S3 (238Hz -> 284Hz)",   "fp": 238.3, "rp": 0.9997, "fz": 283.6, "rz": 0.9984},
            {"name": "S4 (284Hz -> 343Hz)",   "fp": 283.6, "rp": 0.9997, "fz": 343.2, "rz": 0.9976},
            {"name": "S5 (343Hz -> 412Hz)",   "fp": 343.2, "rp": 0.9996, "fz": 412.3, "rz": 0.9962},
            {"name": "S6 (412Hz -> 496Hz)",   "fp": 412.3, "rp": 0.9995, "fz": 495.8, "rz": 0.9990},
            {"name": "S7 (496Hz Anchor)",     "fp": 495.8, "rp": 0.9995, "fz": 0.0,   "rz": 0.0000},
        ],
        "gain_db": -2.0
    },
    {
        "id": "T3",
        "title": "Template 3: Unit-Circle Comb & Phaser Array",
        "subtitle": "All-pass circular loop + unit-circle zeros (Rz=1.0) at octaves",
        "color": "#9b59b6",
        "stages": [
            {"name": "S1 (24Hz Pole / 2.5k Zero)", "fp": 24.3,   "rp": 0.9999, "fz": 2535.6, "rz": 0.9954},
            {"name": "S2 (2.4k Pole / 1.2k Zero)", "fp": 2459.3, "rp": 0.9273, "fz": 1267.8, "rz": 0.9823},
            {"name": "S3 (1.2k Pole / 629Hz Zero)", "fp": 1258.2, "rp": 0.9997, "fz": 629.1,  "rz": 0.9997},
            {"name": "S4 (624Hz Pole / 215Hz Zero)","fp": 624.4,  "rp": 0.9823, "fz": 215.7,  "rz": 0.9641},
            {"name": "S5 (314Hz Pole / 157Hz Zero)","fp": 314.6,  "rp": 0.9999, "fz": 157.3,  "rz": 0.9999},
            {"name": "S6 (155Hz Pole / 24Hz Zero)", "fp": 154.9,  "rp": 0.9954, "fz": 24.3,   "rz": 1.0000},
            {"name": "S7 (DC Tail Anchor)",        "fp": 20.0,   "rp": 0.9978, "fz": 0.0,    "rz": 0.0000},
        ],
        "gain_db": -3.0
    },
    {
        "id": "T4",
        "title": "Template 4: Inharmonic Metallic Bell & Resonator",
        "subtitle": "All-pole chord [0, 12, 19, 24, 28, 31] st (Rp >= 0.998, Rz = 0)",
        "color": "#3498db",
        "stages": [
            {"name": "S1 (f0 = 1868.6 Hz)",       "fp": 1868.6, "rp": 0.9996, "fz": 0.0, "rz": 0.0},
            {"name": "S2 (+12 st = 3794.4 Hz)",   "fp": 3794.4, "rp": 0.9991, "fz": 0.0, "rz": 0.0},
            {"name": "S3 (+19 st = 5681.5 Hz)",   "fp": 5681.5, "rp": 0.9987, "fz": 0.0, "rz": 0.0},
            {"name": "S4 (+24 st = 7578.0 Hz)",   "fp": 7578.0, "rp": 0.9982, "fz": 0.0, "rz": 0.0},
            {"name": "S5 (+28 st = 9472.0 Hz)",   "fp": 9472.0, "rp": 0.9975, "fz": 0.0, "rz": 0.0},
            {"name": "S6 (+31 st = 11366 Hz)",    "fp": 11366.0,"rp": 0.9968, "fz": 0.0, "rz": 0.0},
            {"name": "S7 (Idle Sentinel)",         "fp": 9381.8, "rp": 0.1250, "fz": 0.0, "rz": 0.0},
        ],
        "gain_db": -12.0
    },
    {
        "id": "T5",
        "title": "Template 5: Acid Synth / 24-48 dB LP Stack",
        "subtitle": "Sub-bass anchor + clustered cutoff poles + transparent sentinels",
        "color": "#2ecc71",
        "stages": [
            {"name": "S1 (Sub-bass Anchor 59Hz)", "fp": 59.3,   "rp": 0.9992, "fz": 0.0, "rz": 0.0},
            {"name": "S2 (Cutoff Pole 800Hz)",    "fp": 800.0,  "rp": 0.9900, "fz": 0.0, "rz": 0.0},
            {"name": "S3 (Cutoff Pole 800Hz)",    "fp": 800.0,  "rp": 0.9900, "fz": 0.0, "rz": 0.0},
            {"name": "S4 (Idle Sentinel)",        "fp": 9381.8, "rp": 0.1250, "fz": 0.0, "rz": 0.0},
            {"name": "S5 (Idle Sentinel)",        "fp": 9381.8, "rp": 0.1250, "fz": 0.0, "rz": 0.0},
            {"name": "S6 (Idle Sentinel)",        "fp": 9381.8, "rp": 0.1250, "fz": 0.0, "rz": 0.0},
            {"name": "S7 (Idle Sentinel)",        "fp": 9381.8, "rp": 0.1250, "fz": 0.0, "rz": 0.0},
        ],
        "gain_db": -6.0
    },
    {
        "id": "T6",
        "title": "Template 6: Spatial HRTF & Tone Shaper",
        "subtitle": "Torso shelf + pinna elevation notch (7.8 kHz) + air sheen",
        "color": "#1abc9c",
        "stages": [
            {"name": "S1 (Torso Shelf 1.4k)",      "fp": 1468.1, "rp": 0.8194, "fz": 0.0,    "rz": 0.0000},
            {"name": "S2 (Mid Body Peak)",         "fp": 2200.0, "rp": 0.9000, "fz": 0.0,    "rz": 0.0000},
            {"name": "S3 (Pinna Notch 7.8kHz)",    "fp": 9381.8, "rp": 0.1250, "fz": 7855.9, "rz": 0.9882},
            {"name": "S4 (Ear Canal 4.9kHz)",      "fp": 4956.7, "rp": 0.9312, "fz": 0.0,    "rz": 0.0000},
            {"name": "S5 (High Air Notch 11.9k)",  "fp": 9305.5, "rp": 0.8662, "fz": 11973.4,"rz": 0.9733},
            {"name": "S6 (Air Resonance 18k)",     "fp": 18229.5,"rp": 0.9141, "fz": 0.0,    "rz": 0.0000},
            {"name": "S7 (Idle Sentinel)",         "fp": 9381.8, "rp": 0.1250, "fz": 0.0,    "rz": 0.0000},
        ],
        "gain_db": 0.0
    }
]

# Set up matplotlib dark theme
plt.style.use('dark_background')
fig, axes = plt.subplots(3, 2, figsize=(18, 14), dpi=150)
axes = axes.flatten()

for idx, sc in enumerate(scaffolds):
    ax = axes[idx]
    
    # Compute total cascade and per-stage responses
    total_db = np.full_like(GRID, sc["gain_db"])
    stage_dbs = []
    
    for s in sc["stages"]:
        s_db = eval_biquad_db(s["fp"], s["rp"], s["fz"], s["rz"])
        stage_dbs.append((s["name"], s_db))
        total_db += s_db
        
    # Plot individual stage curves (thin dashed)
    for s_name, s_db in stage_dbs:
        ax.plot(GRID, s_db, linestyle="--", linewidth=0.8, alpha=0.45)
        
    # Plot total cascade (thick solid hero line)
    ax.plot(GRID, total_db, color=sc["color"], linewidth=2.4, label="Total Compound Cascade")
    
    # Styling
    ax.set_xscale("log")
    ax.set_xlim(40.0, 16000.0)
    ax.set_ylim(-50.0, 35.0)
    
    ax.set_xticks([40, 100, 200, 500, 1000, 2000, 5000, 10000, 16000])
    ax.get_xaxis().set_major_formatter(plt.ScalarFormatter())
    
    ax.axhline(0, color="#444444", linestyle=":", linewidth=1.0)
    ax.grid(True, which="major", color="#2a2a2a", linestyle="-", linewidth=0.7)
    ax.grid(True, which="minor", color="#1c1c1c", linestyle=":", linewidth=0.5)
    
    ax.set_title(f"{sc['title']}\n{sc['subtitle']}", fontsize=11, fontweight="bold", pad=8, color="#ffffff")
    ax.set_xlabel("Frequency (Hz)", fontsize=9, color="#aaaaaa")
    ax.set_ylabel("Magnitude (dB)", fontsize=9, color="#aaaaaa")
    ax.tick_params(colors="#888888", labelsize=8)

plt.tight_layout()
out_png = "plots/corpus/six_canonical_templates.png"
os.makedirs("plots/corpus", exist_ok=True)
plt.savefig(out_png, dpi=150)
print(f"SUCCESS: Saved exact templates plot to {out_png}")
