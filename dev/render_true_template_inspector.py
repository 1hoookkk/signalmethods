import json, math, os, glob, struct
import numpy as np
import matplotlib.pyplot as plt

FS = 39062.5
NYQ = FS / 2.0
GRID = np.geomspace(40.0, 16000.0, 512)
COMBINE_K = 4.0

def decode_u16(word):
    u = int(word) + 1
    if u >= 65536:
        return 1.0
    if u <= 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))

def lerp_u16(a, b, frac):
    diff = float(int(b) - int(a))
    delta = int(round(diff * frac))
    return (int(a) + delta) & 0xFFFF

def stage_words_to_biquad(words):
    d = [decode_u16(w) for w in words]
    k = [
        COMBINE_K * d[0] + d[1],
        d[1],
        COMBINE_K * d[2] + d[3],
        d[3],
        COMBINE_K * d[4]
    ]
    c0, c1, c2, c3, c4 = k
    return [c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3]

def parse_body240_words(filepath):
    with open(filepath, "rb") as f:
        data = f.read(240)
    words_120 = struct.unpack("<120H", data)
    # 4 corners x 6 stages x 5 words
    corners = []
    idx = 0
    for ci in range(4):
        stages = []
        for si in range(6):
            stage_words = list(words_120[idx : idx + 5])
            stages.append(stage_words)
            idx += 5
        corners.append(stages)
    return corners # [4][6][5]

def interpolate_p2k_biquads(corners_words, morph, q):
    result_biquads = []
    for si in range(6):
        stage_words = []
        for wi in range(5):
            edge0 = lerp_u16(corners_words[0][si][wi], corners_words[1][si][wi], morph)
            edge1 = lerp_u16(corners_words[2][si][wi], corners_words[3][si][wi], morph)
            w = lerp_u16(edge0, edge1, q)
            stage_words.append(w)
        biquad = stage_words_to_biquad(stage_words)
        result_biquads.append(biquad)
    return result_biquads

def eval_cascade_response(biquads, hz_grid):
    w = 2.0 * math.pi * hz_grid / FS
    cw = np.cos(w)
    sw = np.sin(w)
    c2 = cw * cw - sw * sw
    s2 = 2.0 * sw * cw
    
    power = np.ones_like(hz_grid)
    for b in biquads:
        b0, b1, b2, a1, a2 = b
        num_re = b0 + b1 * cw + b2 * c2
        num_im = b1 * sw + b2 * s2
        num_pwr = num_re**2 + num_im**2
        
        den_re = 1.0 + a1 * cw + a2 * c2
        den_im = a1 * sw + a2 * s2
        den_pwr = np.maximum(den_re**2 + den_im**2, 1e-18)
        
        power *= (num_pwr / den_pwr)
        
    return 10.0 * np.log10(np.maximum(power, 1e-12))

def render_true_inspector(body240_path, title, out_png):
    corners_words = parse_body240_words(body240_path)
    
    plt.style.use('dark_background')
    fig, axes = plt.subplots(2, 2, figsize=(14, 10), dpi=140)
    
    # 1. Top Left: Riding Morph at Q=0
    ax_m = axes[0, 0]
    for i, m in enumerate(np.linspace(0.0, 1.0, 11)):
        bq = interpolate_p2k_biquads(corners_words, m, 0.0)
        db = eval_cascade_response(bq, GRID)
        color = plt.cm.coolwarm(i / 10.0)
        ax_m.plot(GRID, db, color=color, linewidth=1.8 if i in [0, 10] else 0.9,
                  label=f"M={int(m*100)}%" if i in [0, 5, 10] else None)
    ax_m.set_title("Riding MORPH at Q=0 (Morph Sweep 0% -> 100%)", fontsize=10, fontweight="bold")
    ax_m.set_xscale("log")
    ax_m.set_xlim(40, 16000)
    ax_m.set_ylim(-60, 30)
    ax_m.grid(True, alpha=0.2)
    ax_m.legend(loc="upper right", fontsize=8)
    
    # 2. Top Right: Riding Q at Morph=50%
    ax_q = axes[0, 1]
    for i, q in enumerate(np.linspace(0.0, 1.0, 11)):
        bq = interpolate_p2k_biquads(corners_words, 0.5, q)
        db = eval_cascade_response(bq, GRID)
        color = plt.cm.viridis(i / 10.0)
        ax_q.plot(GRID, db, color=color, linewidth=1.8 if i in [0, 10] else 0.9,
                  label=f"Q={int(q*100)}%" if i in [0, 5, 10] else None)
    ax_q.set_title("Riding Q at Morph=50% (Resonance Bloom 0% -> 100%)", fontsize=10, fontweight="bold")
    ax_q.set_xscale("log")
    ax_q.set_xlim(40, 16000)
    ax_q.set_ylim(-60, 30)
    ax_q.grid(True, alpha=0.2)
    ax_q.legend(loc="upper right", fontsize=8)
    
    # 3. Bottom Left: The 4 Static Authored Corners
    ax_c = axes[1, 0]
    corner_labels = ["C00 (M0_Q0)", "C10 (M100_Q0)", "C01 (M0_Q100)", "C11 (M100_Q100)"]
    corner_colors = ["#3498db", "#e74c3c", "#f39c12", "#2ecc71"]
    for ci in range(4):
        m = 1.0 if ci in [1, 3] else 0.0
        q = 1.0 if ci in [2, 3] else 0.0
        bq = interpolate_p2k_biquads(corners_words, m, q)
        db = eval_cascade_response(bq, GRID)
        ax_c.plot(GRID, db, color=corner_colors[ci], linewidth=2.2, label=corner_labels[ci])
    ax_c.set_title("The 4 Authored Corners (Whole Cascade)", fontsize=10, fontweight="bold")
    ax_c.set_xscale("log")
    ax_c.set_xlim(40, 16000)
    ax_c.set_ylim(-60, 30)
    ax_c.grid(True, alpha=0.2)
    ax_c.legend(loc="upper right", fontsize=8)
    
    # 4. Bottom Right: Cumulative Cascade In-Circuit Build (Corner 0)
    ax_cum = axes[1, 1]
    bq_c0 = interpolate_p2k_biquads(corners_words, 0.0, 0.0)
    cum_colors = ["#e74c3c", "#e67e22", "#f1c40f", "#2ecc71", "#3498db", "#9b59b6"]
    for si in range(6):
        partial_bq = bq_c0[: si + 1]
        db = eval_cascade_response(partial_bq, GRID)
        ax_cum.plot(GRID, db, color=cum_colors[si], linewidth=1.4 if si < 5 else 2.4,
                    label=f"S1..S{si+1}" + (" (Total Cascade)" if si == 5 else ""))
    ax_cum.set_title("In-Circuit Cumulative Signal Flow (S1 -> S1..S6 @ C00)", fontsize=10, fontweight="bold")
    ax_cum.set_xscale("log")
    ax_cum.set_xlim(40, 16000)
    ax_cum.set_ylim(-60, 30)
    ax_cum.grid(True, alpha=0.2)
    ax_cum.legend(loc="upper right", fontsize=8)
    
    fig.suptitle(f"{title}\nBit-Exact Z-Plane Dynamic Inspector", fontsize=12, fontweight="bold", y=0.98)
    plt.tight_layout()
    os.makedirs(os.path.dirname(out_png), exist_ok=True)
    plt.savefig(out_png, dpi=140)
    plt.close()
    print(f"Rendered: {out_png}")

# Render key archetypes
render_true_inspector("plots/inspector/P2k_013_talking_hedz.body240", "P2K #013: TalkingHedz (Vocal Formant Template)", "plots/corpus/inspect_talking_hedz_exact.png")
render_true_inspector("plots/inspector/P2k_000_ace_of_bass.body240", "P2K #000: Ace of Bass (Acid Synth LP Template)", "plots/corpus/inspect_ace_of_bass_exact.png")
render_true_inspector("plots/inspector/P2k_030_tooth_comb.body240", "P2K #030: ToothComb (Comb & Phaser Template)", "plots/corpus/inspect_tooth_comb_exact.png")
render_true_inspector("plots/inspector/P2k_032_klang_kling.body240", "P2K #032: KlangKling (Metallic Bell Template)", "plots/corpus/inspect_klang_kling_exact.png")
render_true_inspector("plots/inspector/P2k_004_meaty_gizmo.body240", "P2K #004: MeatyGizmo (Crossover Ladder Template)", "plots/corpus/inspect_meaty_gizmo_exact.png")
render_true_inspector("plots/inspector/P2k_019_radio_craze.body240", "P2K #019: RadioCraze (Spatial / Cabinet EQ Template)", "plots/corpus/inspect_radio_craze_exact.png")
