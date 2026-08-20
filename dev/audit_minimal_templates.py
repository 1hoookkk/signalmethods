import json, glob
from collections import Counter, defaultdict

# Load Morpheus cubes
with open("ref/morpheus/cubes_decoded.json", "r", encoding="utf-8") as f:
    morph = json.load(f)

# Load P2K presets
p2k_files = sorted(glob.glob("recipes/architectures/P2k_*.json"))
p2k_presets = [json.load(open(f, "r", encoding="utf-8")) for f in p2k_files]

def classify_preset_scaffold(sections, num_stages):
    """
    Classifies a preset's base scaffold into canonical macro-archetypes.
    """
    # Analyze Corner 0
    active_poles = []
    active_zeros = []
    unit_zeros = []
    idle_count = 0
    s1_is_shelf = False
    s7_is_idle = False
    
    for si, s in enumerate(sections):
        # Support both morpheus format and p2k format
        if "pole" in s: # Morpheus section
            p = s["pole"]
            z = s["zero"]
            w = s.get("raw", [0,0,0,0])
        else: # P2K section (Corner 0)
            g = s["corners"]["M0_Q0"]
            p = g["pole"]
            z = g["zero"]
            w = [0,0,0,0]
            
        pr = p.get("r", 0.0)
        zr = z.get("r", 0.0)
        phz = p.get("hz", 0.0)
        zhz = z.get("hz", 0.0)
        
        if (pr < 0.45 and zr < 0.45) or (w[1] == 2047 and w[3] == 2047):
            idle_count += 1
            if si == num_stages - 1:
                s7_is_idle = True
        else:
            if pr >= 0.45:
                active_poles.append((si, phz, pr))
            if zr >= 0.45:
                active_zeros.append((si, zhz, zr))
            if zr >= 0.999:
                unit_zeros.append((si, zhz))
                
        if si == 0 and ("pair" in p or (pr > 0.8 and phz < 120.0)):
            s1_is_shelf = True

    # 1. Check Sub-Order Synth LP / HP (e.g. 1 to 4 active stages, rest idle)
    if idle_count >= 3:
        if len(active_poles) <= 2:
            return "T1: Simple Synth 2-Pole / 4-Pole (Lowpass/Highpass/Bandpass)"
        else:
            return "T2: Brickwall Clustered Multi-Pole Synth (24-48 dB Cutoff Stack)"

    # 2. Check All-Pass Phaser / Flanger Comb (Many unit-circle zeros interleaved across octaves)
    if len(unit_zeros) >= 4:
        return "T3: Harmonic & Octave Comb / Phaser Array (Unit-Circle Zero Grid)"

    # 3. Check Inharmonic Metallic Bell / Ringmod (High Q poles, high frequencies, sparse zeros)
    high_q_poles = sum(1 for s, hz, r in active_poles if r >= 0.995)
    if high_q_poles >= 4 and len(active_zeros) <= 2:
        return "T4: Inharmonic Metallic Bell / Resonator Bank (All-Pole Chords)"

    # 4. Check Interleaved Crossover Ladder (Zero of stage k ≈ Pole of stage k+1)
    ladder_matches = 0
    for s_p, hp, _ in active_poles:
        for s_z, hz, _ in active_zeros:
            if s_z == s_p - 1 and abs(hp - hz) < 50.0:
                ladder_matches += 1
    if ladder_matches >= 2:
        return "T5: Interleaved Crossover Ladder (Acoustic Body / Brass / Reeds)"

    # 5. Check Vocal Formant (3-5 distinct formant poles in 300 - 4500 Hz range with guard zeros)
    vocal_poles = sum(1 for s, hz, _ in active_poles if 250.0 <= hz <= 4500.0)
    if vocal_poles >= 3:
        return "T6: Vocal Formant Scaffold (3-5 Formant Peaks + Guarded Valleys)"

    # 6. Spatial / EQ Shaper
    return "T7: Spatial HRTF & Multi-Band Tone Shaper (Elevation Notches / EQ)"

# Classify all 289 Morpheus cubes
morph_counts = Counter()
morph_examples = defaultdict(list)
for c in morph["cubes"]:
    cor0_secs = c["corners"][0]["sections"]
    cat = classify_preset_scaffold(cor0_secs, 7)
    morph_counts[cat] += 1
    morph_examples[cat].append(c["name"])

# Classify all 33 P2K presets
p2k_counts = Counter()
p2k_examples = defaultdict(list)
for p in p2k_presets:
    cat = classify_preset_scaffold(p["sections"], 6)
    p2k_counts[cat] += 1
    p2k_examples[cat].append(p["name"])

print("=== CORPUS-WIDE SCAFFOLD COVERAGE AUDIT ===")
total_all = len(morph["cubes"]) + len(p2k_presets)
print(f"Total Presets Analyzed: {total_all} (289 Morpheus + 33 P2K)\n")

all_categories = sorted(list(set(list(morph_counts.keys()) + list(p2k_counts.keys()))))
for cat in all_categories:
    m_n = morph_counts[cat]
    p_n = p2k_counts[cat]
    tot = m_n + p_n
    pct = (tot / total_all) * 100.0
    print(f"[{cat}]")
    print(f"  Total: {tot:3d} presets ({pct:5.1f}%) | Morpheus: {m_n:3d} | P2K: {p_n:2d}")
    print(f"  Sample Presets: {(morph_examples[cat][:3] + p2k_examples[cat][:3])[:5]}")
    print()
