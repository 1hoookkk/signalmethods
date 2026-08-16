import json, os, glob, numpy as np, struct, math

TAU = 2.0 * math.pi
SR_DATUM = 39062.5
NUM_FREQS = 128

# Frequencies for RMS transfer function matching: 30 Hz to 18 kHz (log-spaced)
eval_freqs = np.logspace(np.log10(30.0), np.log10(18000.0), NUM_FREQS)
eval_w = TAU * eval_freqs / SR_DATUM

def biquad_mag_db(fp, rp, fz, rz, scale, w_vec, sr=SR_DATUM):
    wp = (TAU * fp) / sr if fp > 0 else 0.0
    wz = (TAU * fz) / sr if fz > 0 else 0.0
    
    # Numerator B(e^jw) = 1 - 2 rz cos(wz) z^-1 + rz^2 z^-2
    b0 = scale
    b1 = -2.0 * rz * math.cos(wz) * scale
    b2 = (rz ** 2) * scale
    
    # Denominator A(e^jw) = 1 - 2 rp cos(wp) z^-1 + rp^2 z^-2
    a0 = 1.0
    a1 = -2.0 * rp * math.cos(wp)
    a2 = rp ** 2
    
    z1 = np.exp(-1j * w_vec)
    z2 = np.exp(-2j * w_vec)
    
    num = b0 + b1 * z1 + b2 * z2
    den = a0 + a1 * z1 + a2 * z2
    
    H = num / (den + 1e-12)
    mag = np.abs(H)
    mag_db = 20.0 * np.log10(np.maximum(1e-6, mag))
    return mag_db

def cascade_mag_db(stages, w_vec, sr=SR_DATUM):
    total_db = np.zeros_like(w_vec)
    for stg in stages:
        fp = stg.get('fp', 0.0)
        rp = stg.get('rp', 0.0)
        fz = stg.get('fz', 0.0)
        rz = stg.get('rz', 0.0)
        sc = stg.get('scale', 1.0)
        total_db += biquad_mag_db(fp, rp, fz, rz, sc, w_vec, sr)
    return total_db

def decode_armadillo_pole(k1, k2, sr=SR_DATUM):
    # Log-frequency mapping across 10 octaves
    fp = (sr / 2.0) * (2.0 ** (10.0 * (k1 - 255.0) / 255.0))
    # Radius mapping: k2 maps to 1 - r^2
    w3 = k2 / 255.0
    r_sq = max(0.0, 1.0 - w3)
    rp = math.sqrt(r_sq)
    return float(fp), float(rp)

def decode_armadillo_zero(k1, k2, sr=SR_DATUM):
    if k1 == 255 and k2 == 255:
        return 0.0, 0.0
    fz = (sr / 2.0) * (2.0 ** (10.0 * (k1 - 255.0) / 255.0))
    w3 = k2 / 255.0
    r_sq = max(0.0, 1.0 - w3)
    rz = math.sqrt(r_sq)
    return float(fz), float(rz)

def load_p2k_dataset():
    p2k_files = sorted(glob.glob('recipes/architectures/P2k_*.json'))
    p2k_data = {}
    
    corner_keys = ['M0_Q0', 'M100_Q0', 'M0_Q100', 'M100_Q100']
    
    for f in p2k_files:
        name = os.path.basename(f).replace('.json', '')
        raw = json.load(open(f))
        
        # Build 4-corner cascade curves
        corner_curves = {}
        for ckey in corner_keys:
            stages = []
            for sec in raw.get('sections', []):
                c = sec.get('corners', {}).get(ckey, {})
                p = c.get('pole', {})
                z = c.get('zero', {})
                stages.append({
                    'fp': p.get('hz', 0.0),
                    'rp': p.get('r', 0.0),
                    'fz': z.get('hz', 0.0),
                    'rz': z.get('r', 0.0),
                    'scale': c.get('scale', 1.0)
                })
            corner_curves[ckey] = cascade_mag_db(stages, eval_w, SR_DATUM)
        p2k_data[name] = corner_curves
    return p2k_data

def load_cubes_dataset():
    # Demodulate cubes binary from audio
    import wave
    wav_path = r'C:\Users\hooki\Downloads\extracted_firmware\cubes_v1.01vc_170120.wav'
    with wave.open(wav_path, 'rb') as w:
        raw = w.readframes(w.getnframes())
    samples = np.frombuffer(raw, dtype=np.uint8).astype(int) - 128
    crossings = np.where(np.diff(np.signbit(samples)))[0]
    intervals = np.diff(crossings)
    symbols = ['S' if iv <= 6 else 'L' for iv in intervals]
    bits = []
    i = 0
    while i < len(symbols):
        if symbols[i] == 'L':
            bits.append(0); i += 1
        elif i + 1 < len(symbols) and symbols[i] == 'S' and symbols[i+1] == 'S':
            bits.append(1); i += 2
        else:
            i += 1
    bits_arr = np.array(bits)
    offset = 3
    b_sub = bits_arr[offset : offset + (len(bits_arr) - offset) // 8 * 8]
    weights = 2 ** np.arange(8)
    data = np.dot(b_sub.reshape(-1, 8), weights).astype(np.uint8).tobytes()

    first_cube_offset = 1090
    record_len = 332
    total_cubes = 289

    cubes = []
    for c_idx in range(total_cubes):
        pos = first_cube_offset + c_idx * record_len
        rec = data[pos : pos + record_len]
        raw_name = rec[:12]
        clean_name = ''.join(chr(b) if 32 <= b <= 126 else ' ' for b in raw_name).strip()
        payload = rec[12:332]
        
        # 8 corners x 7 stages x 4 bytes = 224 bytes (or 320 bytes structured)
        # Let's decode the 8 corners (each has 7 stages with 4 bytes)
        corners = []
        for corner_idx in range(8):
            stg_list = []
            for stg_idx in range(7):
                b_off = corner_idx * 28 + stg_idx * 4
                if b_off + 4 <= len(payload):
                    pk1, pk2, zk1, zk2 = payload[b_off : b_off + 4]
                    fp, rp = decode_armadillo_pole(pk1, pk2, SR_DATUM)
                    fz, rz = decode_armadillo_zero(zk1, zk2, SR_DATUM)
                    stg_list.append({'fp': fp, 'rp': rp, 'fz': fz, 'rz': rz, 'scale': 1.0})
                else:
                    stg_list.append({'fp': 0.0, 'rp': 0.0, 'fz': 0.0, 'rz': 0.0, 'scale': 1.0})
            corners.append(stg_list)
        cubes.append({'id': c_idx, 'name': clean_name, 'corners': corners})
    return cubes

def run_audit():
    print("Loading P2K architectures...")
    p2k_presets = load_p2k_dataset()
    print(f"Loaded {len(p2k_presets)} P2K presets.")
    
    print("Loading 289 Morpheus cubes...")
    cubes = load_cubes_dataset()
    print(f"Loaded {len(cubes)} cubes.")

    # 6 Cube Faces (4 corners per face out of 8 corners: [0..7])
    # Corner indexing binary: c = z*4 + y*2 + x
    # (0:000, 1:001, 2:010, 3:011, 4:100, 5:101, 6:110, 7:111)
    faces = {
        'Z0 (Freq Low)':      [0, 1, 2, 3], # (000, 001, 010, 011)
        'Z1 (Freq High)':     [4, 5, 6, 7], # (100, 101, 110, 111)
        'Y0 (X-Z Morph/Q)':   [0, 1, 4, 5], # (000, 001, 100, 101) - Standard P2k orientation
        'Y1 (X-Z Trans/Q)':   [2, 3, 6, 7], # (010, 011, 110, 111)
        'X0 (Y-Z Trans/Morph)':[0, 2, 4, 6], # (000, 010, 100, 110)
        'X1 (Y-Z Trans/Morph)':[1, 3, 5, 7]  # (001, 011, 101, 111)
    }

    # Corner permutations for 2D plane: 4 rotations/flips
    permutations = [
        [0, 1, 2, 3], # standard
        [0, 2, 1, 3], # transpose
        [1, 0, 3, 2], # X-flip
        [2, 3, 0, 1]  # Y-flip
    ]

    p2k_corner_keys = ['M0_Q0', 'M100_Q0', 'M0_Q100', 'M100_Q100']

    results = []

    print("\nRunning exhaustively across 33 P2k Presets x 289 Cubes x 6 Faces x 7 Omitted Stages x 4 Orientations...")

    for p2k_name, p2k_corners in p2k_presets.items():
        best_score = float('inf')
        second_best_score = float('inf')
        best_match = None
        second_match = None
        
        all_cube_scores = []

        for cube in cubes:
            min_cube_score = float('inf')
            min_cube_config = None

            for face_name, face_c_indices in faces.items():
                for perm in permutations:
                    c_map = [face_c_indices[p] for p in perm]
                    
                    # Test dropping stage omit_s in 0..6
                    for omit_s in range(7):
                        # Build the 4 corner curves for this cube face with stage omit_s removed
                        mse_sum = 0.0
                        for corner_i, p2k_k in enumerate(p2k_corner_keys):
                            c_idx = c_map[corner_i]
                            stgs_7 = cube['corners'][c_idx]
                            stgs_6 = [stgs_7[s] for s in range(7) if s != omit_s]
                            cube_curve = cascade_mag_db(stgs_6, eval_w, SR_DATUM)
                            p2k_curve = p2k_corners[p2k_k]
                            
                            # Mean squared error in dB across the frequency band
                            diff = cube_curve - p2k_curve
                            mse_sum += np.mean(diff ** 2)
                        
                        avg_mse = mse_sum / 4.0
                        if avg_mse < min_cube_score:
                            min_cube_score = avg_mse
                            min_cube_config = {
                                'face': face_name,
                                'omit_stage': omit_s + 1,
                                'perm': perm
                            }

            all_cube_scores.append((min_cube_score, cube['id'], cube['name'], min_cube_config))

        all_cube_scores.sort(key=lambda x: x[0])
        best = all_cube_scores[0]
        second = all_cube_scores[1]
        
        # Compute randomized-null distribution: sample 100 random permutations
        null_scores = [all_cube_scores[k][0] for k in np.random.choice(len(all_cube_scores), size=min(100, len(all_cube_scores)), replace=False)]
        null_mean = np.mean(null_scores)
        null_std = np.std(null_scores) + 1e-6
        z_score = (null_mean - best[0]) / null_std

        margin = second[0] - best[0]

        results.append({
            'p2k_preset': p2k_name,
            'best_cube_id': best[1],
            'best_cube_name': best[2],
            'best_mse_db2': best[0],
            'best_rmse_db': math.sqrt(best[0]),
            'second_cube_id': second[1],
            'second_cube_name': second[2],
            'second_mse_db2': second[0],
            'margin_mse': margin,
            'z_score': z_score,
            'face': best[3]['face'],
            'omitted_stage': best[3]['omit_stage']
        })

    # Output ranked table
    results.sort(key=lambda x: x['best_mse_db2'])
    
    print("\n" + "="*115)
    print(f"{'P2K Preset':<24s} | {'Best Cube Match':<20s} | {'RMSE':<8s} | {'2nd Best Cube':<20s} | {'Margin':<8s} | {'Z-Score':<7s} | {'Face':<16s} | {'Drop'}")
    print("="*115)
    for r in results:
        print(f"{r['p2k_preset']:<24s} | #{r['best_cube_id']:03d} {r['best_cube_name'][:14]:<14s} | {r['best_rmse_db']:6.2f}dB | #{r['second_cube_id']:03d} {r['second_cube_name'][:14]:<14s} | {r['margin_mse']:7.2f}  | {r['z_score']:6.2f}σ | {r['face']:<16s} | S{r['omitted_stage']}")
    print("="*115)

    with open('ref/p2k_morpheus_acoustic_audit.json', 'w', encoding='utf-8') as fp:
        json.dump(results, fp, indent=2)
    print("Saved complete numerical audit to ref/p2k_morpheus_acoustic_audit.json")

if __name__ == '__main__':
    run_audit()
