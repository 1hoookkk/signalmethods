import os, sys, glob, struct, math, json
from collections import defaultdict, Counter
import xml.etree.ElementTree as ET
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

TAU = 2.0 * math.pi
IDENTITY_WORDS = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
COMBINE_K = 4.0
SR_39K = 39062.5
SR_44K = 44100.0

def decode_u16(word):
    u = int(word) + 1
    if u == 65536: return 1.0
    if u == 1: return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    return (m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0) * (2.0 ** (e - 15))

def words_to_kernel(words):
    d = [decode_u16(w) for w in words]
    return [
        COMBINE_K * d[0] + d[1],
        d[1],
        COMBINE_K * d[2] + d[3],
        d[3],
        COMBINE_K * d[4]
    ]

def kernel_to_biquad(k):
    c0, c1, c2, c3, c4 = k
    return [c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3]

def words_to_biquad(words):
    return kernel_to_biquad(words_to_kernel(words))

def pair_geometry_at(d_mag, d_rsq, sr=39062.5):
    q = 1.0 - d_rsq
    c = 4.0 * d_mag + d_rsq
    p = c - 2.0
    if abs(p) < 1e-12 and abs(q) < 1e-12:
        return {'type': 'Degenerate', 'hz': 0.0, 'r': 0.0}
    disc = p * p - 4.0 * q
    if disc < 0.0:
        r = np.sqrt(max(0.0, q))
        cos_w = np.clip(-p / (2.0 * r) if r > 0 else 0.0, -1.0, 1.0)
        hz = np.arccos(cos_w) / TAU * sr
        return {'type': 'Conjugate', 'hz': float(hz), 'r': float(r)}
    else:
        s = np.sqrt(disc)
        return {'type': 'RealPair', 'root_a': float((-p + s) / 2.0), 'root_b': float((-p - s) / 2.0)}

def decode_stage(words, sr=39062.5):
    d0, d1, d2, d3, d4 = [decode_u16(w) for w in words]
    zero = pair_geometry_at(d0, d1, sr)
    pole = pair_geometry_at(d2, d3, sr)
    scale = COMBINE_K * d4
    return zero, pole, scale

def biquad_response_db(b, freqs, sr):
    w = 2.0 * np.pi * freqs / sr
    z1 = np.exp(-1j * w)
    z2 = np.exp(-2j * w)
    num = b[0] + b[1] * z1 + b[2] * z2
    den = 1.0 + b[3] * z1 + b[4] * z2
    h = num / np.where(np.abs(den) < 1e-12, 1e-12, den)
    mag = np.abs(h)
    return 20.0 * np.log10(np.maximum(mag, 1e-12))

def cascade_response_db(biquads, freqs, sr):
    total_db = np.zeros_like(freqs, dtype=float)
    for b in biquads:
        total_db += biquad_response_db(b, freqs, sr)
    return total_db

def run_clean_analysis():
    os.makedirs('plots', exist_ok=True)
    os.makedirs('plotdata', exist_ok=True)
    
    # 1. Load P2k Factory Presets (33 presets)
    p2k_files = sorted(glob.glob('ref/presets/P2k_*.bin'))
    p2k_presets = {}
    for pf in p2k_files:
        name = os.path.splitext(os.path.basename(pf))[0]
        data = open(pf, 'rb').read()
        words = struct.unpack('<' + 'H' * 120, data)
        corners = {}
        for ci, cname in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
            stages = [tuple(words[(ci*6 + si)*5 : (ci*6 + si + 1)*5]) for si in range(6)]
            corners[cname] = stages
        p2k_presets[name] = corners

    # 2. Load Cubes (Hero, Extrusions, first.body)
    cube_files = sorted(glob.glob('recipes/hero/*.body') + glob.glob('recipes/extrusions/*.body') + glob.glob('ref/cubes/first.body'))
    cubes = {}
    for bf in cube_files:
        name = os.path.splitext(os.path.basename(bf))[0]
        data = open(bf, 'rb').read()
        if len(data) == 560:
            words = struct.unpack('<' + 'H' * 280, data)
            corners = {}
            for ci in range(8):
                stages = [tuple(words[(ci*7 + si)*5 : (ci*7 + si + 1)*5]) for si in range(7)]
                corners[f"C{ci}"] = stages
            cubes[name] = {'sr': SR_44K, 'stages': 7, 'corners': corners}
        elif len(data) == 240:
            words = struct.unpack('<' + 'H' * 120, data)
            corners = {}
            for ci, cname in enumerate(["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]):
                stages = [tuple(words[(ci*6 + si)*5 : (ci*6 + si + 1)*5]) for si in range(6)]
                corners[cname] = stages
            cubes[name] = {'sr': SR_39K, 'stages': 6, 'corners': corners}

    # 3. Load Emulator X XML Templates
    emu_root = r'C:\Users\hooki\OneDrive\Documents\Creative Professional\Emulator X Family\Templates\Filter'
    xml_files = sorted(glob.glob(os.path.join(emu_root, '*.xml')))
    xml_presets = {}
    for xf in xml_files:
        bname = os.path.splitext(os.path.basename(xf))[0]
        try:
            tree = ET.parse(xf)
            root = tree.getroot()
            filt = root.find('.//filter')
            if filt is not None:
                sections = []
                for ds in filt.findall('designer-section'):
                    idx = int(ds.attrib.get('index', 0))
                    lg = int(ds.findtext('low-gain', '0').strip())
                    lf = int(ds.findtext('low-freq', '0').strip())
                    hg = int(ds.findtext('high-gain', '0').strip())
                    hf = int(ds.findtext('high-freq', '0').strip())
                    st = int(ds.findtext('type', '0').strip())
                    sections.append((idx, st, lf, hf, lg, hg))
                sections.sort(key=lambda s: s[0])
                type_abs = filt.findtext('type-absolute', '0').strip()
                xml_presets[bname] = {'type_abs': type_abs, 'sections': sections}
        except Exception as e:
            pass

    freqs = np.geomspace(40, 18000, 512)

    # -------------------------------------------------------------
    # P2K FACTORY PRESET ANALYSIS
    # -------------------------------------------------------------
    p2k_sec_occ = defaultdict(list)
    p2k_zero_occ = defaultdict(list)
    p2k_pole_occ = defaultdict(list)
    p2k_corner_occ = defaultdict(list)
    all_p2k_corners = {}

    for pname, corners in p2k_presets.items():
        for cname, stages in corners.items():
            ckey = (pname, cname)
            all_p2k_corners[ckey] = stages
            p2k_corner_occ[tuple(stages)].append(ckey)
            for si, sw in enumerate(stages):
                if sw != IDENTITY_WORDS:
                    loc = (pname, cname, si + 1)
                    p2k_sec_occ[sw].append(loc)
                    p2k_zero_occ[(sw[0], sw[1])].append(loc)
                    p2k_pole_occ[(sw[2], sw[3])].append(loc)

    # Cross-preset complete section sharing
    p2k_shared_sec = {sw: locs for sw, locs in p2k_sec_occ.items() if len(set(l[0] for l in locs)) > 1}
    # Within-preset complete section sharing across corners/slots
    p2k_multi_sec = {sw: locs for sw, locs in p2k_sec_occ.items() if len(locs) > 1}

    # Near-exact corners (5 shared + 1 replaced section in P2k)
    p2k_ckeys = list(all_p2k_corners.keys())
    p2k_shared_5_pairs = []
    p2k_shared_4_pairs = []
    for i in range(len(p2k_ckeys)):
        k1 = p2k_ckeys[i]
        c1 = all_p2k_corners[k1]
        for j in range(i + 1, len(p2k_ckeys)):
            k2 = p2k_ckeys[j]
            c2 = all_p2k_corners[k2]
            matches = sum(1 for s in range(6) if c1[s] == c2[s] and c1[s] != IDENTITY_WORDS)
            if matches == 5:
                diff_slot = [s for s in range(6) if c1[s] != c2[s]][0]
                p2k_shared_5_pairs.append((k1, k2, diff_slot))
            elif matches == 4:
                diff_slots = [s for s in range(6) if c1[s] != c2[s]]
                p2k_shared_4_pairs.append((k1, k2, diff_slots))

    # Zero scaffolds in P2k
    p2k_z_scaffolds = defaultdict(list)
    for ckey, stages in all_p2k_corners.items():
        active = [s for s in stages if s != IDENTITY_WORDS]
        if len(active) == 6:
            z_scaff = tuple((s[0], s[1]) for s in active)
            p_scaff = tuple((s[2], s[3]) for s in active)
            p2k_z_scaffolds[z_scaff].append((ckey, p_scaff))

    # Pole angle scaffolds with different radii in P2k
    p2k_p_angle_scaffolds = defaultdict(list)
    for ckey, stages in all_p2k_corners.items():
        active = [s for s in stages if s != IDENTITY_WORDS]
        if len(active) == 6:
            p_angles = tuple(s[2] for s in active)
            p_radii = tuple(s[3] for s in active)
            p2k_p_angle_scaffolds[p_angles].append((ckey, p_radii, tuple(s[0] for s in active)))

    # -------------------------------------------------------------
    # EMULATOR X XML PRESET ANALYSIS
    # -------------------------------------------------------------
    xml_ds_occ = defaultdict(list)
    xml_filter_secs = {}
    for pname, pval in xml_presets.items():
        s_list = []
        for (idx, st, lf, hf, lg, hg) in pval['sections']:
            tup = (st, lf, hf, lg, hg)
            s_list.append(tup)
            if not (st == 0 and lf == 0 and hf == 0 and lg == 0 and hg == 0):
                xml_ds_occ[tup].append((pname, idx))
        xml_filter_secs[pname] = s_list

    xml_multi_ds = {k: v for k, v in xml_ds_occ.items() if len(v) > 1}
    
    # XML 5-section matches
    xml_pnames = list(xml_filter_secs.keys())
    xml_5_pairs = []
    for i in range(len(xml_pnames)):
        n1 = xml_pnames[i]
        s1 = xml_filter_secs[n1]
        for j in range(i + 1, len(xml_pnames)):
            n2 = xml_pnames[j]
            s2 = xml_filter_secs[n2]
            if len(s1) == 6 and len(s2) == 6:
                matches = sum(1 for s in range(6) if s1[s] == s2[s])
                if matches == 5:
                    diff_idx = [s for s in range(6) if s1[s] != s2[s]][0]
                    xml_5_pairs.append((n1, n2, diff_idx))

    # -------------------------------------------------------------
    # PLOTTING
    # -------------------------------------------------------------
    # Plot 1: Authentic P2k Recurring Sections & Zero States
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # 1A: S6 Traveling Null vs S3/S4/S5 Vowel Zeros
    ax = axes[0, 0]
    z_null = [0xFF7D, 0x01F0] # 17.96 kHz r=1.0
    z_s3 = [0xA9FC, 0xC3FC]   # 2014 Hz r=0.960
    z_s4 = [0xBBFC, 0xBBFC]   # 2971 Hz r=0.972
    z_s5 = [0xE1FC, 0xEBFC]   # 7922 Hz r=0.750
    
    for zw, name, col, ls in [
        (z_null, "S6 Nyquist Null (17.96 kHz, r=1.0) [48 corners / 26 presets]", '#d62728', '-'),
        (z_s3, "S3 Vowel Zero (2014 Hz, r=0.960) [10 corners / 6 presets]", '#1f77b4', '--'),
        (z_s4, "S4 Vowel Zero (2971 Hz, r=0.972) [9 corners / 6 presets]", '#2ca02c', '-.'),
        (z_s5, "S5 Shelf Zero (7922 Hz, r=0.750) [10 corners / 6 presets]", '#ff7f0e', ':')
    ]:
        bq = words_to_biquad((zw[0], zw[1], 0xDFFF, 0xFFFF, 0xDFFF))
        r = biquad_response_db(bq, freqs, SR_39K)
        ax.plot(freqs, r, label=name, color=col, ls=ls, lw=2)
    ax.set_xscale('log')
    ax.set_title("Key Recurring Zero Atoms in P2k Factory Presets (39.0625 kHz)", fontsize=10, fontweight='bold')
    ax.set_ylabel("Magnitude (dB)")
    ax.set_ylim(-45, 15)
    ax.grid(True, which='both', alpha=0.3)
    ax.legend(fontsize=7.5, loc='lower left')
    ax.axhline(0, color='gray', lw=0.5)

    # 1B: Shared Complete Sections (Millennium / Meaty Gizmo S6 & Vowel Pose Sharing)
    ax = axes[0, 1]
    # Millennium / MeatyGizmo S6
    s_mill = (0xF37D, 0x01F0, 0x67FC, 0xB8FC, 0xD25A)
    # TalkingHedz S1 M0_Q0
    s_hedz1 = (0x6CFC, 0xCFFC, 0xECFC, 0xB8FC, 0xD1FA)
    # TalkingHedz S6 M0_Q0
    s_hedz6 = (0xDEFC, 0x01F0, 0x41FB, 0xA1FC, 0xD1FA)
    
    r_mill = biquad_response_db(words_to_biquad(s_mill), freqs, SR_39K)
    r_hedz1 = biquad_response_db(words_to_biquad(s_hedz1), freqs, SR_39K)
    r_hedz6 = biquad_response_db(words_to_biquad(s_hedz6), freqs, SR_39K)
    
    ax.plot(freqs, r_mill, label="Millennium / MeatyGizmo S6 (11.1k null + 456Hz pole)", color='#9467bd', lw=2)
    ax.plot(freqs, r_hedz1, label="TalkingHedz / UbuOrator S1 (347Hz zero + 9.3k pole)", color='#1f77b4', lw=2)
    ax.plot(freqs, r_hedz6, label="TalkingHedz / UbuOrator S6 (6.4k null + 199Hz pole)", color='#e377c2', lw=2)
    ax.set_xscale('log')
    ax.set_title("Exact Stored Complete Sections Shared Across Presets", fontsize=10, fontweight='bold')
    ax.set_ylabel("Magnitude (dB)")
    ax.set_ylim(-40, 25)
    ax.grid(True, which='both', alpha=0.3)
    ax.legend(fontsize=7.5, loc='lower left')
    ax.axhline(0, color='gray', lw=0.5)

    # 1C: Zero Scaffolds: Fixed Zeros with Animated Formant Poles (TalkingHedz M0 vs M100)
    ax = axes[1, 0]
    th_m0 = all_p2k_corners[('P2k_013_talking_hedz', 'M0_Q0')]
    th_m100 = all_p2k_corners[('P2k_013_talking_hedz', 'M100_Q0')]
    ubu_m0 = all_p2k_corners[('P2k_021_ubu_orator', 'M0_Q0')]
    
    r_thm0 = cascade_response_db([words_to_biquad(w) for w in th_m0], freqs, SR_39K)
    r_thm100 = cascade_response_db([words_to_biquad(w) for w in th_m100], freqs, SR_39K)
    r_ubum0 = cascade_response_db([words_to_biquad(w) for w in ubu_m0], freqs, SR_39K)
    
    ax.plot(freqs, r_thm0, label="TalkingHedz M0_Q0 (/u/ pose)", color='#1f77b4', lw=2)
    ax.plot(freqs, r_thm100, label="TalkingHedz M100_Q0 (/i/ pose)", color='#ff7f0e', lw=2)
    ax.plot(freqs, r_ubum0, label="UbuOrator M0_Q0 (Shared /u/ pose)", color='#2ca02c', ls='--', lw=2)
    ax.set_xscale('log')
    ax.set_title("Shared Vowel Poses Across Filter Families (TalkingHedz & UbuOrator)", fontsize=10, fontweight='bold')
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.set_ylim(-35, 45)
    ax.grid(True, which='both', alpha=0.3)
    ax.legend(fontsize=8, loc='upper right')
    ax.axhline(0, color='gray', lw=0.5)

    # 1D: Pole Scaffold with Q-Lift (Klub Klassik M0_Q0 vs M0_Q100)
    ax = axes[1, 1]
    kk_q0 = all_p2k_corners[('P2k_005_klub_klassik', 'M0_Q0')]
    kk_q100 = all_p2k_corners[('P2k_005_klub_klassik', 'M0_Q100')]
    r_kk_q0 = cascade_response_db([words_to_biquad(w) for w in kk_q0], freqs, SR_39K)
    r_kk_q100 = cascade_response_db([words_to_biquad(w) for w in kk_q100], freqs, SR_39K)
    
    ax.plot(freqs, r_kk_q0, label="Klub Klassik M0_Q0 (Baseline Pole Scaffold)", color='#1f77b4', lw=2)
    ax.plot(freqs, r_kk_q100, label="Klub Klassik M0_Q100 (Same Frequencies + Radius Lift)", color='#d62728', lw=2)
    ax.set_xscale('log')
    ax.set_title("Same Pole Frequency Scaffold + Varying Radii (Q-Modulation)", fontsize=10, fontweight='bold')
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.set_ylim(-30, 45)
    ax.grid(True, which='both', alpha=0.3)
    ax.legend(fontsize=8, loc='upper right')
    ax.axhline(0, color='gray', lw=0.5)

    plt.tight_layout()
    plt.savefig('plots/p2k_factory_section_recurrence.png', dpi=200)
    plt.close()

    # Save detailed JSON report
    clean_report = {
        'p2k_summary': {
            'total_presets': len(p2k_presets),
            'total_corners': len(all_p2k_corners),
            'total_active_sections': len(p2k_sec_occ),
            'sections_shared_across_distinct_presets': len(p2k_shared_sec),
            'zero_states_shared_across_distinct_presets': len({zw: locs for zw, locs in p2k_zero_occ.items() if len(set(l[0] for l in locs)) > 1}),
            'pole_states_shared_across_distinct_presets': len({pw: locs for pw, locs in p2k_pole_occ.items() if len(set(l[0] for l in locs)) > 1}),
            'zero_scaffolds_shared': len({k: v for k, v in p2k_z_scaffolds.items() if len(v) > 1}),
            'pole_angle_scaffolds_shared': len({k: v for k, v in p2k_p_angle_scaffolds.items() if len(v) > 1}),
            'near_exact_5_shared_pairs': len(p2k_shared_5_pairs)
        },
        'xml_summary': {
            'total_presets': len(xml_presets),
            'designer_sections_shared_across_presets': len(xml_multi_ds),
            'five_section_matches': len(xml_5_pairs)
        }
    }
    with open('plotdata/clean_corpus_summary.json', 'w') as f:
        json.dump(clean_report, f, indent=2)
        
    print("Clean analysis complete. Output written to plots/p2k_factory_section_recurrence.png and plotdata/clean_corpus_summary.json")

if __name__ == '__main__':
    run_clean_analysis()
