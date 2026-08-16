import os
import sys
import glob
import struct
import math
import json
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
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    if e == 0:
        x = m / 4096.0
    else:
        x = (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))

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
        r = math.sqrt(max(0.0, q))
        cos_w = max(-1.0, min(1.0, -p / (2.0 * r))) if r > 0 else 0.0
        hz = math.acos(cos_w) / TAU * sr
        return {'type': 'Conjugate', 'hz': hz, 'r': r}
    else:
        s = math.sqrt(disc)
        return {'type': 'RealPair', 'root_a': (-p + s) / 2.0, 'root_b': (-p - s) / 2.0}

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

def load_all_corpora():
    corpus = {}
    
    # 1. P2k Presets (.bin)
    p2k_files = glob.glob('ref/presets/*.bin')
    for pf in sorted(p2k_files):
        name = os.path.splitext(os.path.basename(pf))[0]
        data = open(pf, 'rb').read()
        if len(data) == 240:
            words = struct.unpack('<' + 'H' * 120, data)
            corners = []
            corner_names = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
            for ci in range(4):
                c_stages = []
                for si in range(6):
                    idx = (ci * 6 + si) * 5
                    sw = tuple(words[idx:idx+5])
                    c_stages.append(sw)
                corners.append((corner_names[ci], c_stages))
            corpus[f'preset:{name}'] = {
                'type': 'preset_bin',
                'name': name,
                'path': pf,
                'datum_sr': SR_39K,
                'corners': corners,
                'num_stages': 6
            }
            
    # 2. Cubes (.body files)
    body_files = glob.glob('ref/cubes/*.body') + glob.glob('recipes/extrusions/*.body') + glob.glob('recipes/hero/*.body')
    for bf in sorted(body_files):
        name = os.path.splitext(os.path.basename(bf))[0]
        data = open(bf, 'rb').read()
        if len(data) == 560:
            words = struct.unpack('<' + 'H' * 280, data)
            corners = []
            corner_names = [f"C{ci}" for ci in range(8)]
            for ci in range(8):
                c_stages = []
                for si in range(7):
                    idx = (ci * 7 + si) * 5
                    sw = tuple(words[idx:idx+5])
                    c_stages.append(sw)
                corners.append((corner_names[ci], c_stages))
            corpus[f'cube:{name}'] = {
                'type': 'cube_body',
                'name': name,
                'path': bf,
                'datum_sr': SR_44K,
                'corners': corners,
                'num_stages': 7
            }
        elif len(data) == 240:
            words = struct.unpack('<' + 'H' * 120, data)
            corners = []
            corner_names = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
            for ci in range(4):
                c_stages = []
                for si in range(6):
                    idx = (ci * 6 + si) * 5
                    sw = tuple(words[idx:idx+5])
                    c_stages.append(sw)
                corners.append((corner_names[ci], c_stages))
            corpus[f'cube_legacy:{name}'] = {
                'type': 'cube_body_legacy',
                'name': name,
                'path': bf,
                'datum_sr': SR_39K,
                'corners': corners,
                'num_stages': 6
            }

    # 3. Emulator X XML templates
    emu_root = r'C:\Users\hooki\OneDrive\Documents\Creative Professional\Emulator X Family\Templates\Filter'
    xml_files = glob.glob(os.path.join(emu_root, '*.xml'))
    xml_corpus = {}
    for xf in sorted(xml_files):
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
                xml_corpus[f'xml:{bname}'] = {
                    'name': bname,
                    'path': xf,
                    'type_abs': type_abs,
                    'sections': sections
                }
        except Exception as e:
            print(f"Error parsing XML {xf}: {e}")

    return corpus, xml_corpus

def analyze_binary_corpus(corpus):
    section_occurrences = defaultdict(list)
    zero_occurrences = defaultdict(list)
    pole_occurrences = defaultdict(list)
    scale_occurrences = defaultdict(list)
    corner_occurrences = defaultdict(list)
    
    all_corners = {}
    
    for fkey, fval in corpus.items():
        for cname, stages in fval['corners']:
            ckey = (fkey, cname)
            all_corners[ckey] = stages
            corner_tuple = tuple(stages)
            corner_occurrences[corner_tuple].append(ckey)
            
            for si, sw in enumerate(stages):
                if sw == IDENTITY_WORDS:
                    continue
                loc = (fkey, cname, si)
                section_occurrences[sw].append(loc)
                zero_occurrences[(sw[0], sw[1])].append(loc)
                pole_occurrences[(sw[2], sw[3])].append(loc)
                scale_occurrences[sw[4]].append(loc)

    return {
        'sections': section_occurrences,
        'zeros': zero_occurrences,
        'poles': pole_occurrences,
        'scales': scale_occurrences,
        'corners': corner_occurrences,
        'all_corners': all_corners
    }

def analyze_near_exact_and_scaffolds(corpus, bin_analysis):
    all_corners = bin_analysis['all_corners']
    corner_keys = list(all_corners.keys())
    
    shared_5_pairs = []
    shared_4_pairs = []
    
    for i in range(len(corner_keys)):
        ckey1 = corner_keys[i]
        c1 = all_corners[ckey1]
        for j in range(i + 1, len(corner_keys)):
            ckey2 = corner_keys[j]
            c2 = all_corners[ckey2]
            
            min_stages = min(len(c1), len(c2))
            matches = sum(1 for s in range(min_stages) if c1[s] == c2[s] and c1[s] != IDENTITY_WORDS)
            active1 = sum(1 for s in range(min_stages) if c1[s] != IDENTITY_WORDS)
            active2 = sum(1 for s in range(min_stages) if c2[s] != IDENTITY_WORDS)
            
            if min_stages == 6 and active1 == 6 and active2 == 6:
                if matches == 5:
                    diff_slot = [s for s in range(6) if c1[s] != c2[s]][0]
                    shared_5_pairs.append((ckey1, ckey2, diff_slot))
                elif matches == 4:
                    diff_slots = [s for s in range(6) if c1[s] != c2[s]]
                    shared_4_pairs.append((ckey1, ckey2, diff_slots))
            elif min_stages == 7 and active1 == 7 and active2 == 7:
                if matches == 6:
                    diff_slot = [s for s in range(7) if c1[s] != c2[s]][0]
                    shared_5_pairs.append((ckey1, ckey2, diff_slot))

    zero_scaffolds = defaultdict(list)
    for ckey, stages in all_corners.items():
        active = [s for s in stages if s != IDENTITY_WORDS]
        if len(active) >= 4:
            z_scaffold = tuple((s[0], s[1]) for s in active)
            p_scaffold = tuple((s[2], s[3]) for s in active)
            zero_scaffolds[z_scaffold].append((ckey, p_scaffold))

    pole_scaffolds = defaultdict(list)
    for ckey, stages in all_corners.items():
        active = [s for s in stages if s != IDENTITY_WORDS]
        if len(active) >= 4:
            p_scaffold = tuple((s[2], s[3]) for s in active)
            z_scaffold = tuple((s[0], s[1]) for s in active)
            pole_scaffolds[p_scaffold].append((ckey, z_scaffold))

    pole_angle_scaffolds = defaultdict(list)
    for ckey, stages in all_corners.items():
        active = [s for s in stages if s != IDENTITY_WORDS]
        if len(active) >= 4:
            angle_scaffold = tuple(s[2] for s in active)
            radius_scaffold = tuple(s[3] for s in active)
            pole_angle_scaffolds[angle_scaffold].append((ckey, radius_scaffold, tuple(s[0] for s in active)))

    return {
        'shared_5_pairs': shared_5_pairs,
        'shared_4_pairs': shared_4_pairs,
        'zero_scaffolds': zero_scaffolds,
        'pole_scaffolds': pole_scaffolds,
        'pole_angle_scaffolds': pole_angle_scaffolds
    }

def analyze_xml_corpus(xml_corpus):
    ds_occurrences = defaultdict(list)
    filter_sections = {}
    
    for fkey, fval in xml_corpus.items():
        s_list = []
        for (idx, st, lf, hf, lg, hg) in fval['sections']:
            tup = (st, lf, hf, lg, hg)
            s_list.append(tup)
            if not (st == 0 and lf == 0 and hf == 0 and lg == 0 and hg == 0):
                ds_occurrences[tup].append((fkey, idx))
        filter_sections[fkey] = s_list

    xml_full_matches = defaultdict(list)
    for fkey, s_list in filter_sections.items():
        xml_full_matches[tuple(s_list)].append(fkey)
        
    xml_keys = list(filter_sections.keys())
    xml_5_matches = []
    for i in range(len(xml_keys)):
        k1 = xml_keys[i]
        s1 = filter_sections[k1]
        for j in range(i+1, len(xml_keys)):
            k2 = xml_keys[j]
            s2 = filter_sections[k2]
            if len(s1) == 6 and len(s2) == 6:
                matches = sum(1 for s in range(6) if s1[s] == s2[s])
                if matches == 5:
                    diff_idx = [s for s in range(6) if s1[s] != s2[s]][0]
                    xml_5_matches.append((k1, k2, diff_idx))

    return {
        'ds_occurrences': ds_occurrences,
        'xml_full_matches': xml_full_matches,
        'xml_5_matches': xml_5_matches,
        'filter_sections': filter_sections
    }

def run_analysis_and_plot():
    os.makedirs('plots', exist_ok=True)
    os.makedirs('plotdata', exist_ok=True)
    
    corpus, xml_corpus = load_all_corpora()
    print(f"Loaded {len(corpus)} binary objects and {len(xml_corpus)} XML presets.")
    
    bin_analysis = analyze_binary_corpus(corpus)
    scaff_analysis = analyze_near_exact_and_scaffolds(corpus, bin_analysis)
    xml_analysis = analyze_xml_corpus(xml_corpus)
    
    freqs = np.geomspace(40, 18000, 512)
    
    # -------------------------------------------------------------
    # 1. EXACT COMPLETE SECTIONS RECURRENCE
    # -------------------------------------------------------------
    sec_occ = bin_analysis['sections']
    # Filter out single occurrences
    multi_sections = {k: v for k, v in sec_occ.items() if len(v) > 1}
    sorted_multi_sections = sorted(multi_sections.items(), key=lambda x: len(x[1]), reverse=True)
    
    print("\n=======================================================")
    print(f"EXACT COMPLETE SECTION RECURRENCE (Distinct active sections: {len(sec_occ)})")
    print(f"Sections appearing in multiple corners/filters: {len(multi_sections)}")
    print("=======================================================")
    
    sec_report = []
    for idx, (words, locs) in enumerate(sorted_multi_sections[:30]):
        z_geo, p_geo, sc = decode_stage(words, SR_39K)
        # Distinct filters
        filters = sorted(list(set(loc[0] for loc in locs)))
        slots = Counter(loc[2] + 1 for loc in locs)
        corners = Counter(loc[1] for loc in locs)
        
        words_hex = [f"0x{w:04X}" for w in words]
        entry = {
            'rank': idx + 1,
            'words': words_hex,
            'total_occurrences': len(locs),
            'num_distinct_filters': len(filters),
            'filters': filters,
            'slots': dict(slots),
            'corners': dict(corners),
            'zero_geometry_39k': z_geo,
            'pole_geometry_39k': p_geo,
            'scale': sc
        }
        sec_report.append(entry)
        print(f"#{idx+1}: {len(locs)} occ ({len(filters)} filters) | Slots: {dict(slots)} | Words: {words_hex}")
        print(f"     Zero: {z_geo} | Pole: {p_geo} | Scale: {sc:.4f}")
        print(f"     Filters: {', '.join(filters[:6])}{'...' if len(filters)>6 else ''}")

    # Plot top recurring complete sections
    fig, axes = plt.subplots(3, 2, figsize=(14, 12), sharex=True, sharey=True)
    axes = axes.flatten()
    for i, (words, locs) in enumerate(sorted_multi_sections[:6]):
        ax = axes[i]
        b_39k = words_to_biquad(words)
        resp_39k = biquad_response_db(b_39k, freqs, SR_39K)
        resp_44k = biquad_response_db(b_39k, freqs, SR_44K)
        
        z_geo, p_geo, sc = decode_stage(words, SR_39K)
        
        ax.plot(freqs, resp_39k, label='39.0625 kHz datum', color='#1f77b4', lw=2)
        ax.plot(freqs, resp_44k, label='44.1 kHz datum', color='#ff7f0e', lw=1.5, ls='--')
        
        slots_str = ", ".join(f"S{s}:{cnt}" for s, cnt in sorted(Counter(l[2]+1 for l in locs).items()))
        filters = sorted(list(set(l[0].split(':')[-1] for l in locs)))
        filt_str = ", ".join(filters[:3]) + (f" +{len(filters)-3}" if len(filters) > 3 else "")
        
        title = f"Rank #{i+1}: {len(locs)} occ across {len(filters)} filters ({slots_str})\n"
        if z_geo['type'] == 'Conjugate':
            title += f"Z: {z_geo['hz']:.0f}Hz r={z_geo['r']:.3f} | "
        else:
            title += f"Z: Real | "
        if p_geo['type'] == 'Conjugate':
            title += f"P: {p_geo['hz']:.0f}Hz r={p_geo['r']:.3f} | "
        else:
            title += f"P: Real | "
        title += f"Gain: {20*np.log10(sc):.1f}dB\n[{filt_str}]"
        
        ax.set_title(title, fontsize=9)
        ax.set_xscale('log')
        ax.grid(True, which='both', alpha=0.3)
        ax.set_ylim(-40, 40)
        ax.axhline(0, color='gray', lw=0.5, ls=':')
        if i == 0:
            ax.legend(loc='upper right', fontsize=8)
            
    fig.suptitle("Top 6 Exact Complete Section States Recur Across Corpus", fontsize=13, fontweight='bold')
    plt.tight_layout()
    plt.savefig('plots/recurring_complete_sections.png', dpi=200)
    plt.close()

    # -------------------------------------------------------------
    # 2. EXACT ZERO PAIR RECURRENCE
    # -------------------------------------------------------------
    z_occ = bin_analysis['zeros']
    multi_zeros = {k: v for k, v in z_occ.items() if len(v) > 1}
    sorted_multi_zeros = sorted(multi_zeros.items(), key=lambda x: len(x[1]), reverse=True)
    
    print("\n=======================================================")
    print(f"EXACT ZERO-PAIR RECURRENCE (Distinct active zero pairs: {len(z_occ)})")
    print(f"Zero pairs appearing in multiple locations: {len(multi_zeros)}")
    print("=======================================================")
    
    zero_report = []
    for idx, ((w0, w1), locs) in enumerate(sorted_multi_zeros[:20]):
        d0, d1 = decode_u16(w0), decode_u16(w1)
        z_geo = pair_geometry_at(d0, d1, SR_39K)
        filters = sorted(list(set(loc[0] for loc in locs)))
        slots = Counter(loc[2] + 1 for loc in locs)
        
        zero_report.append({
            'rank': idx + 1,
            'words': [f"0x{w0:04X}", f"0x{w1:04X}"],
            'occurrences': len(locs),
            'filters': len(filters),
            'slots': dict(slots),
            'geometry': z_geo
        })
        print(f"Zero #{idx+1}: {len(locs)} occ ({len(filters)} filters) | Slots: {dict(slots)} | Words: [0x{w0:04X}, 0x{w1:04X}] | Geo: {z_geo}")

    # -------------------------------------------------------------
    # 3. EXACT FULL CORNER EQUIVALENCES
    # -------------------------------------------------------------
    corner_occ = bin_analysis['corners']
    multi_corners = {k: v for k, v in corner_occ.items() if len(v) > 1}
    
    print("\n=======================================================")
    print(f"EXACT FULL-CORNER RECURRENCE (Distinct full corners: {len(corner_occ)})")
    print(f"Corners shared identically across multiple endpoints: {len(multi_corners)}")
    print("=======================================================")
    
    corner_report = []
    for idx, (stages, ckeys) in enumerate(sorted(multi_corners.items(), key=lambda x: len(x[1]), reverse=True)):
        filt_names = [f"{k[0].split(':')[-1]}.{k[1]}" for k in ckeys]
        active_count = sum(1 for s in stages if s != IDENTITY_WORDS)
        corner_report.append({
            'rank': idx + 1,
            'occurrences': len(ckeys),
            'endpoints': filt_names,
            'active_stages': active_count
        })
        print(f"Shared Corner #{idx+1}: {len(ckeys)} endpoints | Active Stages: {active_count} | Endpoints: {filt_names}")

    # -------------------------------------------------------------
    # 4. "5 SECTIONS + 1 REPLACED SECTION" (Near-Exact Hamming Pairs)
    # -------------------------------------------------------------
    shared_5 = scaff_analysis['shared_5_pairs']
    print("\n=======================================================")
    print(f"NEAR-EXACT CORNER PAIRS: '5 Shared + 1 Replaced Section' (Found: {len(shared_5)} pairs)")
    print("=======================================================")
    
    # Filter for interesting cross-filter or cross-corner pairs
    shared_5_report = []
    for idx, (ck1, ck2, diff_slot) in enumerate(shared_5[:25]):
        f1, cname1 = ck1
        f2, cname2 = ck2
        s1 = bin_analysis['all_corners'][ck1][diff_slot]
        s2 = bin_analysis['all_corners'][ck2][diff_slot]
        
        z1, p1, sc1 = decode_stage(s1, SR_39K)
        z2, p2, sc2 = decode_stage(s2, SR_39K)
        
        shared_5_report.append({
            'pair_idx': idx + 1,
            'corner1': f"{f1.split(':')[-1]}.{cname1}",
            'corner2': f"{f2.split(':')[-1]}.{cname2}",
            'diff_slot': diff_slot + 1,
            's1_words': [f"0x{w:04X}" for w in s1],
            's2_words': [f"0x{w:04X}" for w in s2],
            's1_geo': {'zero': z1, 'pole': p1, 'scale': sc1},
            's2_geo': {'zero': z2, 'pole': p2, 'scale': sc2}
        })
        print(f"Pair #{idx+1}: {f1.split(':')[-1]}.{cname1} <-> {f2.split(':')[-1]}.{cname2} | Replaced Stage: S{diff_slot+1}")
        print(f"     S{diff_slot+1} A: Z={z1} P={p1} Gain={20*np.log10(sc1):.1f}dB")
        print(f"     S{diff_slot+1} B: Z={z2} P={p2} Gain={20*np.log10(sc2):.1f}dB")

    # Plot 4 illustrative 5-shared + 1-replaced corner pairs
    fig, axes = plt.subplots(2, 2, figsize=(14, 10), sharex=True, sharey=True)
    axes = axes.flatten()
    
    # Pick distinct interesting pairs
    sample_pairs = []
    seen_filters = set()
    for item in shared_5:
        f1 = item[0][0].split(':')[-1]
        f2 = item[1][0].split(':')[-1]
        if (f1, f2) not in seen_filters and f1 != f2:
            sample_pairs.append(item)
            seen_filters.add((f1, f2))
        if len(sample_pairs) == 4:
            break
    if len(sample_pairs) < 4:
        sample_pairs = shared_5[:4]
        
    for i, (ck1, ck2, diff_slot) in enumerate(sample_pairs):
        ax = axes[i]
        c1 = bin_analysis['all_corners'][ck1]
        c2 = bin_analysis['all_corners'][ck2]
        
        bqs1 = [words_to_biquad(w) for w in c1 if w != IDENTITY_WORDS]
        bqs2 = [words_to_biquad(w) for w in c2 if w != IDENTITY_WORDS]
        
        tot_resp1 = cascade_response_db(bqs1, freqs, SR_39K)
        tot_resp2 = cascade_response_db(bqs2, freqs, SR_39K)
        
        # Also plot the replaced section response
        bq_diff1 = words_to_biquad(c1[diff_slot])
        bq_diff2 = words_to_biquad(c2[diff_slot])
        resp_diff1 = biquad_response_db(bq_diff1, freqs, SR_39K)
        resp_diff2 = biquad_response_db(bq_diff2, freqs, SR_39K)
        
        n1 = f"{ck1[0].split(':')[-1]}.{ck1[1]}"
        n2 = f"{ck2[0].split(':')[-1]}.{ck2[1]}"
        
        ax.plot(freqs, tot_resp1, label=f"Full {n1}", color='#1f77b4', lw=2)
        ax.plot(freqs, tot_resp2, label=f"Full {n2}", color='#ff7f0e', lw=2)
        ax.plot(freqs, resp_diff1, label=f"Replaced S{diff_slot+1} ({n1})", color='#1f77b4', ls=':', lw=1.2)
        ax.plot(freqs, resp_diff2, label=f"Replaced S{diff_slot+1} ({n2})", color='#ff7f0e', ls=':', lw=1.2)
        
        ax.set_title(f"5 Shared Sections + Replaced S{diff_slot+1}\n{n1} vs {n2}", fontsize=9)
        ax.set_xscale('log')
        ax.grid(True, which='both', alpha=0.3)
        ax.set_ylim(-50, 40)
        ax.legend(loc='lower left', fontsize=7.5)
        ax.axhline(0, color='gray', lw=0.5, ls=':')
        
    fig.suptitle("Anatomy of 5-Shared + 1-Replaced Section Corner Pairs", fontsize=13, fontweight='bold')
    plt.tight_layout()
    plt.savefig('plots/shared_5_replaced_1_corners.png', dpi=200)
    plt.close()

    # -------------------------------------------------------------
    # 5. SAME ZERO SCAFFOLD + DIFFERENT POLES
    # -------------------------------------------------------------
    z_scaffolds = scaff_analysis['zero_scaffolds']
    multi_z_scaffolds = {k: v for k, v in z_scaffolds.items() if len(v) > 1}
    
    print("\n=======================================================")
    print(f"SAME ZERO SCAFFOLD + DIFFERENT POLES (Found: {len(multi_z_scaffolds)} shared zero scaffolds)")
    print("=======================================================")
    
    z_scaff_report = []
    for idx, (z_scaff, entries) in enumerate(sorted(multi_z_scaffolds.items(), key=lambda x: len(x[1]), reverse=True)[:15]):
        # check distinct pole scaffolds
        distinct_poles = set(e[1] for e in entries)
        endpoints = [f"{e[0][0].split(':')[-1]}.{e[0][1]}" for e in entries]
        
        # decode zeros
        zeros_geo = [pair_geometry_at(decode_u16(zw[0]), decode_u16(zw[1]), SR_39K) for zw in z_scaff]
        
        z_scaff_report.append({
            'rank': idx + 1,
            'endpoints_count': len(entries),
            'distinct_pole_scaffolds': len(distinct_poles),
            'endpoints': endpoints,
            'zeros_geometry': zeros_geo
        })
        print(f"Zero Scaffold #{idx+1}: {len(entries)} corners ({len(distinct_poles)} distinct pole sets) | Endpoints: {endpoints[:5]}...")
        print(f"     Zeros: {zeros_geo}")

    # Plot shared zero scaffold
    fig, axes = plt.subplots(2, 2, figsize=(14, 10), sharex=True, sharey=True)
    axes = axes.flatten()
    
    top_z_scaff = sorted(multi_z_scaffolds.items(), key=lambda x: len(x[1]), reverse=True)[:4]
    for i, (z_scaff, entries) in enumerate(top_z_scaff):
        ax = axes[i]
        
        # compute zero scaffold response (with unit pole / scale 1)
        z_bqs = []
        for zw in z_scaff:
            # construct biquad with zero pair zw and identity pole/scale
            w = (zw[0], zw[1], 0xDFFF, 0xFFFF, 0xDFFF)
            z_bqs.append(words_to_biquad(w))
        z_resp = cascade_response_db(z_bqs, freqs, SR_39K)
        ax.plot(freqs, z_resp, label="Fixed Zero Scaffold", color='black', lw=2.5, ls='--')
        
        # Plot up to 4 distinct full corner responses on this scaffold
        seen_p = set()
        colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728']
        color_idx = 0
        for ckey, p_scaff in entries:
            if p_scaff not in seen_p and color_idx < len(colors):
                seen_p.add(p_scaff)
                stages = bin_analysis['all_corners'][ckey]
                bqs = [words_to_biquad(w) for w in stages if w != IDENTITY_WORDS]
                tot_resp = cascade_response_db(bqs, freqs, SR_39K)
                ep_name = f"{ckey[0].split(':')[-1]}.{ckey[1]}"
                ax.plot(freqs, tot_resp, label=f"Corner {ep_name}", color=colors[color_idx], lw=1.5, alpha=0.85)
                color_idx += 1
                
        ax.set_title(f"Zero Scaffold #{i+1}: {len(entries)} Corners Sharing Identical Zeros", fontsize=9)
        ax.set_xscale('log')
        ax.grid(True, which='both', alpha=0.3)
        ax.set_ylim(-60, 40)
        ax.legend(loc='lower left', fontsize=7.5)
        ax.axhline(0, color='gray', lw=0.5, ls=':')
        
    fig.suptitle("Same Zero Scaffold Hosting Diverse Pole Configurations", fontsize=13, fontweight='bold')
    plt.tight_layout()
    plt.savefig('plots/recurring_zero_scaffolds.png', dpi=200)
    plt.close()

    # -------------------------------------------------------------
    # 6. SAME POLE SCAFFOLD + DIFFERENT RADII (Q-Moves)
    # -------------------------------------------------------------
    p_angle_scaffolds = scaff_analysis['pole_angle_scaffolds']
    multi_p_angles = {k: v for k, v in p_angle_scaffolds.items() if len(v) > 1}
    
    print("\n=======================================================")
    print(f"SAME POLE ANGLE SCAFFOLD + DIFFERENT RADII (Found: {len(multi_p_angles)} scaffolds)")
    print("=======================================================")
    
    p_angle_report = []
    for idx, (p_angles, entries) in enumerate(sorted(multi_p_angles.items(), key=lambda x: len(x[1]), reverse=True)[:15]):
        endpoints = [f"{e[0][0].split(':')[-1]}.{e[0][1]}" for e in entries]
        distinct_radii = set(e[1] for e in entries)
        p_angle_report.append({
            'rank': idx + 1,
            'endpoints_count': len(entries),
            'distinct_radii_sets': len(distinct_radii),
            'endpoints': endpoints,
            'pole_angle_words': [f"0x{w:04X}" for w in p_angles]
        })
        print(f"Pole Angle Scaffold #{idx+1}: {len(entries)} corners ({len(distinct_radii)} radii sets) | Endpoints: {endpoints[:5]}...")

    # Plot 4 pole angle scaffolds with varying radii
    fig, axes = plt.subplots(2, 2, figsize=(14, 10), sharex=True, sharey=True)
    axes = axes.flatten()
    top_p_angles = sorted(multi_p_angles.items(), key=lambda x: len(x[1]), reverse=True)[:4]
    
    for i, (p_angles, entries) in enumerate(top_p_angles):
        ax = axes[i]
        colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
        for c_i, (ckey, r_scaff, z_scaff) in enumerate(entries[:4]):
            stages = bin_analysis['all_corners'][ckey]
            bqs = [words_to_biquad(w) for w in stages if w != IDENTITY_WORDS]
            tot_resp = cascade_response_db(bqs, freqs, SR_39K)
            ep_name = f"{ckey[0].split(':')[-1]}.{ckey[1]}"
            ax.plot(freqs, tot_resp, label=f"{ep_name}", color=colors[c_i % len(colors)], lw=1.6)
            
        ax.set_title(f"Pole Frequency Scaffold #{i+1} with Varying Radii\n({len(entries)} corners)", fontsize=9)
        ax.set_xscale('log')
        ax.grid(True, which='both', alpha=0.3)
        ax.set_ylim(-50, 40)
        ax.legend(loc='lower left', fontsize=7.5)
        ax.axhline(0, color='gray', lw=0.5, ls=':')
        
    fig.suptitle("Pole Frequency Alignment: Shared Resonant Scaffold under Q-Modulation", fontsize=13, fontweight='bold')
    plt.tight_layout()
    plt.savefig('plots/same_pole_scaffold_diff_radii.png', dpi=200)
    plt.close()

    # -------------------------------------------------------------
    # 7. EMULATOR X XML RECURRING SECTIONS & 5-MATCHES
    # -------------------------------------------------------------
    ds_occ = xml_analysis['ds_occurrences']
    multi_ds = {k: v for k, v in ds_occ.items() if len(v) > 1}
    sorted_multi_ds = sorted(multi_ds.items(), key=lambda x: len(x[1]), reverse=True)
    
    print("\n=======================================================")
    print(f"EMULATOR X XML RECURRENCE (Distinct active designer sections: {len(ds_occ)})")
    print(f"Designer sections occurring across multiple XML presets: {len(multi_ds)}")
    print(f"5-Section matches between XML presets: {len(xml_analysis['xml_5_matches'])}")
    print("=======================================================")
    
    xml_report = []
    for idx, (ds_tup, locs) in enumerate(sorted_multi_ds[:25]):
        st, lf, hf, lg, hg = ds_tup
        type_str = {0: 'Peak', 1: 'LowShelf', 2: 'Notch', 3: 'LowPass', 4: 'HighPass', 5: 'BandPass', 6: 'HighShelf'}.get(st, f"Type_{st}")
        filters = sorted(list(set(l[0].split(':')[-1] for l in locs)))
        slots = Counter(l[1] for l in locs)
        
        xml_report.append({
            'rank': idx + 1,
            'designer_tuple': {'type': type_str, 'type_id': st, 'low_freq': lf, 'high_freq': hf, 'low_gain': lg, 'high_gain': hg},
            'occurrences': len(locs),
            'filters_count': len(filters),
            'slots': dict(slots),
            'filters': filters
        })
        print(f"XML DS #{idx+1}: {len(locs)} occ ({len(filters)} presets) | Slots: {dict(slots)} | [{type_str}: LF={lf} HF={hf} LG={lg} HG={hg}] | Presets: {filters[:4]}...")

    # Plot top recurring XML sections
    fig, axes = plt.subplots(3, 2, figsize=(14, 12), sharex=True, sharey=True)
    axes = axes.flatten()
    
    for i, (ds_tup, locs) in enumerate(sorted_multi_ds[:6]):
        ax = axes[i]
        st, lf, hf, lg, hg = ds_tup
        type_str = {0: 'Peak', 1: 'LowShelf', 2: 'Notch', 3: 'LowPass', 4: 'HighPass', 5: 'BandPass', 6: 'HighShelf'}.get(st, f"Type_{st}")
        
        filters = sorted(list(set(l[0].split(':')[-1] for l in locs)))
        slots_str = ", ".join(f"S{s}:{cnt}" for s, cnt in sorted(Counter(l[1] for l in locs).items()))
        
        # Plot parameter trajectories across slots/presets
        ax.bar(['Low Freq', 'High Freq', 'Low Gain', 'High Gain'], [lf, hf, lg, hg], color=['#1f77b4', '#aec7e8', '#ff7f0e', '#ffbb78'])
        ax.set_ylim(0, 130)
        ax.set_title(f"XML Rank #{i+1}: {type_str} ({len(locs)} occ across {len(filters)} presets)\nSlots: {slots_str}\n[{', '.join(filters[:3])}...]", fontsize=9)
        ax.grid(True, axis='y', alpha=0.3)
        for p in ax.patches:
            ax.annotate(f"{int(p.get_height())}", (p.get_x() + p.get_width() / 2., p.get_height() + 2), ha='center', fontsize=8)
            
    fig.suptitle("Top 6 Reused Designer Section Parameter States in Emulator X XML Library", fontsize=13, fontweight='bold')
    plt.tight_layout()
    plt.savefig('plots/emulator_x_recurring_sections.png', dpi=200)
    plt.close()

    # -------------------------------------------------------------
    # 8. SAVE COMPLETE JSON REPORT
    # -------------------------------------------------------------
    complete_report = {
        'summary': {
            'binary_objects_loaded': len(corpus),
            'xml_presets_loaded': len(xml_corpus),
            'total_active_sections': len(sec_occ),
            'multi_occurrence_sections': len(multi_sections),
            'total_active_zero_pairs': len(z_occ),
            'multi_occurrence_zeros': len(multi_zeros),
            'total_distinct_corners': len(corner_occ),
            'identical_shared_corners': len(multi_corners),
            'near_exact_5_shared_corner_pairs': len(shared_5),
            'zero_scaffolds_shared': len(multi_z_scaffolds),
            'pole_angle_scaffolds_shared': len(multi_p_angles),
            'xml_multi_occurrence_sections': len(multi_ds),
            'xml_5_section_matches': len(xml_analysis['xml_5_matches'])
        },
        'top_recurring_complete_sections': sec_report,
        'top_recurring_zeros': zero_report,
        'shared_full_corners': corner_report,
        'shared_5_replaced_1_pairs': shared_5_report,
        'shared_zero_scaffolds': z_scaff_report,
        'shared_pole_angle_scaffolds': p_angle_report,
        'emulator_x_recurring_designer_sections': xml_report
    }
    
    with open('plotdata/corpus_section_recurrence_report.json', 'w') as f:
        json.dump(complete_report, f, indent=2)
    print("\nSaved full JSON report to plotdata/corpus_section_recurrence_report.json")
    print("All plots generated successfully in plots/")

if __name__ == '__main__':
    run_analysis_and_plot()
