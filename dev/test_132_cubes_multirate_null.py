"""
Test whether any of the 132 unlisted Morpheus cubes (at 39,062.5 Hz datum),
when compiled into continuous parametric form (Hz, BW_Hz) and evaluated across
44.1 kHz, 48.0 kHz, 96.0 kHz, and 192.0 kHz, match or null against any of the
P2k multi-rate variant quadruplets in EmulatorX.dll.
"""

import glob, os, json, struct, math, sys, time
import numpy as np

sys.path.insert(0, '.')
from dev.clean_corpus_analysis import decode_stage, biquad_response_db, words_to_biquad

SR_39K = 39062.5
TARGET_RATES = [44100.0, 48000.0, 96000.0, 192000.0]

def extract_morpheus_continuous_cube(json_path):
    with open(json_path, 'r', encoding='utf-8') as fp:
        doc = json.load(fp)

    name = doc.get('source_file', os.path.basename(json_path))
    corners = []

    for c_entry in doc.get('corner_data', []):
        sections = []
        for s_entry in c_entry.get('sections', []):
            pole_info = s_entry.get('pole', {})
            zero_info = s_entry.get('zero', {})
            scale = s_entry.get('scale', 1.0)

            p_kind = pole_info.get('kind', 'degenerate')
            p_hz = pole_info.get('hz', pole_info.get('frequency_hz', 0.0))
            p_r = pole_info.get('radius', 0.0)
            p_bw = -np.log(max(1e-6, p_r)) * SR_39K / np.pi if (p_kind == 'conjugate' and p_r > 0) else 0.0

            z_kind = zero_info.get('kind', 'degenerate')
            z_hz = zero_info.get('hz', zero_info.get('frequency_hz', 0.0))
            z_r = zero_info.get('radius', 0.0)
            z_bw = -np.log(max(1e-6, z_r)) * SR_39K / np.pi if (z_kind == 'conjugate' and z_r > 0) else 0.0

            sections.append({
                'section': s_entry.get('section', 0),
                'words': s_entry.get('words', []),
                'scale': scale,
                'pole': {'kind': p_kind, 'hz': float(p_hz), 'r': float(p_r), 'bw': float(p_bw)},
                'zero': {'kind': z_kind, 'hz': float(z_hz), 'r': float(z_r), 'bw': float(z_bw)}
            })
        corners.append(sections)
    return name, corners

def load_p2k_variant_quadruplets():
    p2k_dirs = sorted(glob.glob('ref/p2k_variants/P2k_*'))
    p2k_db = {}

    for d in p2k_dirs:
        pname = os.path.basename(d)
        bins = sorted(glob.glob(os.path.join(d, '*.bin')))
        if len(bins) != 4:
            continue

        var_data = []
        for vi, bpath in enumerate(bins):
            sr = TARGET_RATES[vi]
            raw = open(bpath, 'rb').read()
            words = struct.unpack('<120H', raw)

            corners = []
            for ci in range(4):
                c_stages = []
                for si in range(6):
                    w = words[(ci*6 + si)*5 : (ci*6 + si + 1)*5]
                    zero, pole, scale = decode_stage(w, sr=sr)
                    bw = -np.log(max(1e-6, pole.get('r', 0))) * sr / np.pi if pole['type'] == 'Conjugate' else 0.0
                    c_stages.append({
                        'words': w,
                        'zero': zero,
                        'pole': pole,
                        'pole_bw': bw,
                        'scale': scale,
                        'biquad': words_to_biquad(w)
                    })
                corners.append(c_stages)
            var_data.append({'sr': sr, 'corners': corners})
        p2k_db[pname] = var_data
    return p2k_db

def evaluate_cube_response_at_rate(corner_sections, freqs, target_sr):
    total_db = np.zeros_like(freqs, dtype=float)
    for s in corner_sections:
        p = s['pole']
        z = s['zero']
        scale = s['scale']

        if p['kind'] == 'conjugate' and p['hz'] > 0:
            p_hz = p['hz']
            p_bw = p['bw']
            r = np.exp(-np.pi * p_bw / target_sr)
            r = np.clip(r, 0.0, 0.9999)
            theta = 2.0 * np.pi * p_hz / target_sr
            a1 = -2.0 * r * np.cos(theta)
            a2 = r * r
        else:
            a1, a2 = 0.0, 0.0

        if z['kind'] == 'conjugate' and z['hz'] > 0:
            z_hz = z['hz']
            z_bw = z['bw']
            rz = np.exp(-np.pi * z_bw / target_sr)
            rz = np.clip(rz, 0.0, 1.0)
            thetaz = 2.0 * np.pi * z_hz / target_sr
            b1 = -2.0 * rz * np.cos(thetaz)
            b2 = rz * rz
            b0 = 1.0
        else:
            b0, b1, b2 = 1.0, 0.0, 0.0

        biquad = [b0 * scale, b1 * scale, b2 * scale, a1, a2]
        total_db += biquad_response_db(biquad, freqs, target_sr)
    return total_db

def run_null_experiment():
    t0 = time.time()
    print("=" * 85)
    print("MULTI-RATE LINEAGE NULL TEST: 132 UNLISTED MORPHEUS CUBES vs 50 P2K QUADRUPLETS")
    print("=" * 85)

    cube_files = sorted(glob.glob('ref/morpheus_decoded/unlisted/*.json'))
    print(f"Loaded {len(cube_files)} unlisted Morpheus cube definitions.")

    p2k_db = load_p2k_variant_quadruplets()
    print(f"Loaded {len(p2k_db)} P2K multi-rate quadruplets (44.1k, 48k, 96k, 192k).\n")

    freq_grid = np.geomspace(40.0, 18000.0, 256)

    # 1. Precompute P2K normalized responses: shape (n_p2k, 4_corners, 4_rates, 256)
    p2k_list = sorted(p2k_db.keys())
    n_p2k = len(p2k_list)
    p2k_resps = np.zeros((n_p2k, 4, 4, len(freq_grid)), dtype=np.float32)
    p2k_is_flat = np.zeros((n_p2k, 4), dtype=bool)

    for pi, pname in enumerate(p2k_list):
        for vi in range(4):
            sr = TARGET_RATES[vi]
            for pc_i in range(4):
                biquads = [st['biquad'] for st in p2k_db[pname][vi]['corners'][pc_i]]
                r = np.zeros_like(freq_grid, dtype=float)
                for b in biquads:
                    r += biquad_response_db(b, freq_grid, sr)
                span = np.max(r) - np.min(r)
                if span < 1.0: # flat/bypass corner
                    p2k_is_flat[pi, pc_i] = True
                r_norm = r - np.mean(r)
                p2k_resps[pi, pc_i, vi, :] = r_norm

    # 2. Precompute Morpheus compiled responses: shape (n_cubes, 8_corners, 4_rates, 256)
    n_cubes = len(cube_files)
    cube_resps = np.zeros((n_cubes, 8, 4, len(freq_grid)), dtype=np.float32)
    cube_is_flat = np.zeros((n_cubes, 8), dtype=bool)
    cube_names = []

    for ci, cf in enumerate(cube_files):
        cname, c_corners = extract_morpheus_continuous_cube(cf)
        cube_names.append(cname)
        for mc_i, mc_sec in enumerate(c_corners):
            for vi in range(4):
                sr = TARGET_RATES[vi]
                r = evaluate_cube_response_at_rate(mc_sec, freq_grid, sr)
                span = np.max(r) - np.min(r)
                if span < 1.0:
                    cube_is_flat[ci, mc_i] = True
                r_norm = r - np.mean(r)
                cube_resps[ci, mc_i, vi, :] = r_norm

    print(f"Precomputed response tensors in {time.time() - t0:.2f}s. Computing cross-distance matrix...")

    all_active_matches = []

    for ci in range(n_cubes):
        c_name = cube_names[ci]
        c_tensor = cube_resps[ci] # (8, 4, 256)
        for pi in range(n_p2k):
            p_name = p2k_list[pi]
            p_tensor = p2k_resps[pi] # (4, 4, 256)
            for mc_i in range(8):
                if cube_is_flat[ci, mc_i]:
                    continue
                for pc_i in range(4):
                    if p2k_is_flat[pi, pc_i]:
                        continue
                    diff = c_tensor[mc_i] - p_tensor[pc_i] # (4, 256)
                    rms_per_rate = np.sqrt(np.mean(diff ** 2, axis=1)) # (4,)
                    avg_rms = float(np.mean(rms_per_rate))
                    all_active_matches.append({
                        'cube': c_name,
                        'p2k': p_name,
                        'cube_corner': mc_i,
                        'p2k_corner': pc_i,
                        'avg_rms_db': avg_rms,
                        'var_rms': [float(x) for x in rms_per_rate]
                    })

    all_active_matches.sort(key=lambda x: x['avg_rms_db'])

    print("\n" + "=" * 105)
    print("--- TOP 25 CLOSEST NON-TRIVIAL (ACTIVE SPECTRAL SHAPE) MATCHES ACROSS ALL 4 RATES ---")
    print(f"{'Morpheus Cube':<28} {'P2K Preset':<28} {'C_M':<4} {'C_P':<4} {'Avg RMS':<9} {'44.1k':<7} {'48k':<7} {'96k':<7} {'192k':<7}")
    print("-" * 105)
    for m in all_active_matches[:25]:
        v = m['var_rms']
        print(f"{m['cube'][:27]:<28} {m['p2k'][:27]:<28} {m['cube_corner']:<4} {m['p2k_corner']:<4} {m['avg_rms_db']:<9.2f} {v[0]:<7.2f} {v[1]:<7.2f} {v[2]:<7.2f} {v[3]:<7.2f}")

    print("\n--- ACTIVE CORPUS DISTANCE DISTRIBUTION (132 Unlisted Cubes vs 50 P2K Variants) ---")
    distances = [m['avg_rms_db'] for m in all_active_matches]
    print(f"Total Active Corner-Pair Comparisons : {len(distances)}")
    print(f"Minimum Distance across all 4 rates   : {np.min(distances):.2f} dB")
    print(f"Median Distance across all 4 rates    : {np.median(distances):.2f} dB")
    print(f"Active Matches < 1.0 dB (Exact Null)  : {sum(1 for d in distances if d < 1.0)}")
    print(f"Active Matches < 3.0 dB (Close Family): {sum(1 for d in distances if d < 3.0)}")
    print(f"Active Matches < 5.0 dB (Rough Resemb): {sum(1 for d in distances if d < 5.0)}")
    print("=" * 105)

if __name__ == '__main__':
    run_null_experiment()
