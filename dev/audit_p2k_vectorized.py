import json, os, glob, numpy as np, struct, math, time

TAU = 2.0 * math.pi
SR_DATUM = 39062.5
NUM_FREQS = 128

eval_freqs = np.logspace(np.log10(30.0), np.log10(18000.0), NUM_FREQS)
eval_w = TAU * eval_freqs / SR_DATUM

def biquad_mag_db_vec(fp_arr, rp_arr, fz_arr, rz_arr, scale_arr, w_vec, sr=SR_DATUM):
    # Vectorized computation across N stages x NUM_FREQS
    # fp_arr, rp_arr, fz_arr, rz_arr, scale_arr shape: (N,)
    # Output shape: (N, NUM_FREQS)
    N = len(fp_arr)
    wp = np.where(fp_arr > 0, (TAU * fp_arr) / sr, 0.0)[:, None]
    wz = np.where(fz_arr > 0, (TAU * fz_arr) / sr, 0.0)[:, None]
    
    rp = rp_arr[:, None]
    rz = rz_arr[:, None]
    sc = scale_arr[:, None]
    
    w = w_vec[None, :] # (1, NUM_FREQS)
    z1 = np.exp(-1j * w)
    z2 = np.exp(-2j * w)
    
    num = sc * (1.0 - 2.0 * rz * np.cos(wz) * z1 + (rz ** 2) * z2)
    den = 1.0 - 2.0 * rp * np.cos(wp) * z1 + (rp ** 2) * z2
    
    H = num / (den + 1e-12)
    mag = np.abs(H)
    return 20.0 * np.log10(np.maximum(1e-6, mag))

def decode_armadillo_pole(k1, k2, sr=SR_DATUM):
    fp = (sr / 2.0) * (2.0 ** (10.0 * (k1 - 255.0) / 255.0))
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

def run_fast_audit():
    t0 = time.time()
    
    # 1. Load P2K
    p2k_files = sorted(glob.glob('recipes/architectures/P2k_*.json'))
    p2k_names = [os.path.basename(f).replace('.json', '') for f in p2k_files]
    corner_keys = ['M0_Q0', 'M100_Q0', 'M0_Q100', 'M100_Q100']
    
    # P2K response tensor: (33, 4, 128)
    p2k_tensor = np.zeros((len(p2k_files), 4, NUM_FREQS), dtype=np.float32)
    
    for p_idx, f in enumerate(p2k_files):
        raw = json.load(open(f))
        for c_idx, ckey in enumerate(corner_keys):
            fps, rps, fzs, rzs, scs = [], [], [], [], []
            for sec in raw.get('sections', []):
                c = sec.get('corners', {}).get(ckey, {})
                p = c.get('pole', {})
                z = c.get('zero', {})
                fps.append(p.get('hz', 0.0))
                rps.append(p.get('r', 0.0))
                fzs.append(z.get('hz', 0.0))
                rzs.append(z.get('r', 0.0))
                scs.append(c.get('scale', 1.0))
            
            stg_db = biquad_mag_db_vec(np.array(fps), np.array(rps), np.array(fzs), np.array(rzs), np.array(scs), eval_w, SR_DATUM)
            p2k_tensor[p_idx, c_idx, :] = np.sum(stg_db, axis=0)
            
    print(f"Built P2K reference tensor: shape {p2k_tensor.shape}")

    # 2. Demodulate 289 cubes
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
    num_cubes = 289

    cube_names = []
    # Precompute cube individual stage DB curves: (289, 8, 7, 128)
    cube_stages_db = np.zeros((num_cubes, 8, 7, NUM_FREQS), dtype=np.float32)

    for c_idx in range(num_cubes):
        pos = first_cube_offset + c_idx * record_len
        rec = data[pos : pos + record_len]
        raw_name = rec[:12]
        clean_name = ''.join(chr(b) if 32 <= b <= 126 else ' ' for b in raw_name).strip()
        cube_names.append(clean_name)
        payload = rec[12:332]
        
        for corner_i in range(8):
            fps, rps, fzs, rzs, scs = [], [], [], [], []
            for stg_i in range(7):
                b_off = corner_i * 28 + stg_i * 4
                if b_off + 4 <= len(payload):
                    pk1, pk2, zk1, zk2 = payload[b_off : b_off + 4]
                    fp, rp = decode_armadillo_pole(pk1, pk2, SR_DATUM)
                    fz, rz = decode_armadillo_zero(zk1, zk2, SR_DATUM)
                    fps.append(fp); rps.append(rp); fzs.append(fz); rzs.append(rz); scs.append(1.0)
                else:
                    fps.append(0.0); rps.append(0.0); fzs.append(0.0); rzs.append(0.0); scs.append(1.0)
            
            c_db = biquad_mag_db_vec(np.array(fps), np.array(rps), np.array(fzs), np.array(rzs), np.array(scs), eval_w, SR_DATUM)
            cube_stages_db[c_idx, corner_i, :, :] = c_db

    print(f"Precomputed Cube stages tensor: shape {cube_stages_db.shape}")

    # 3. Define 6 Cube Faces x 4 Permutations
    faces = {
        'Z0 (Freq Low)':      [0, 1, 2, 3],
        'Z1 (Freq High)':     [4, 5, 6, 7],
        'Y0 (X-Z Morph/Q)':   [0, 1, 4, 5],
        'Y1 (X-Z Trans/Q)':   [2, 3, 6, 7],
        'X0 (Y-Z Trans/Morph)':[0, 2, 4, 6],
        'X1 (Y-Z Trans/Morph)':[1, 3, 5, 7]
    }
    permutations = [
        [0, 1, 2, 3],
        [0, 2, 1, 3],
        [1, 0, 3, 2],
        [2, 3, 0, 1]
    ]

    # Precompute all 6-stage face cascade configurations:
    # 6 faces * 4 perms = 24 planar corner selections
    # 7 stage drop choices
    # Total configurations = 24 * 7 = 168 configurations
    # For each config, precompute the 4-corner curve for all 289 cubes: (289, 4, 128)
    
    configs = []
    for face_name, face_c in faces.items():
        for perm_idx, perm in enumerate(permutations):
            c_indices = [face_c[p] for p in perm] # 4 corner indices
            for omit_s in range(7):
                configs.append((face_name, perm_idx, omit_s, c_indices))
    
    print(f"Evaluating {len(configs)} face/stage-drop configurations across 33 presets...")

    # Pre-aggregate the 6-stage cascades for all 7 omitted stages:
    # (num_cubes, 8, 7_omitted, 128)
    cube_all_7 = np.sum(cube_stages_db, axis=2) # (289, 8, 128)
    # 6-stage sum with omit_s removed = cube_all_7 - cube_stages_db[:, :, omit_s, :]
    cube_6stage = cube_all_7[:, :, None, :] - cube_stages_db # (289, 8, 7, 128)

    results = []

    for p_idx, p_name in enumerate(p2k_names):
        p_ref = p2k_tensor[p_idx] # (4, 128)
        
        cube_best_mses = np.full(num_cubes, np.inf)
        cube_best_meta = [None] * num_cubes

        for (face_name, perm_idx, omit_s, c_indices) in configs:
            # Extract the 4 corners for all cubes: (289, 4, 128)
            cand_curves = cube_6stage[:, c_indices, omit_s, :] # (289, 4, 128)
            
            # Compute MSE across (4, 128)
            diff = cand_curves - p_ref[None, :, :] # (289, 4, 128)
            mse_vals = np.mean(diff ** 2, axis=(1, 2)) # (289,)
            
            improved = mse_vals < cube_best_mses
            cube_best_mses[improved] = mse_vals[improved]
            for c_i in np.where(improved)[0]:
                cube_best_meta[c_i] = (face_name, omit_s + 1, perm_idx)

        ranked_indices = np.argsort(cube_best_mses)
        best_c = ranked_indices[0]
        second_c = ranked_indices[1]

        best_mse = float(cube_best_mses[best_c])
        second_mse = float(cube_best_mses[second_c])
        margin = second_mse - best_mse
        
        null_mean = float(np.mean(cube_best_mses))
        null_std = float(np.std(cube_best_mses)) + 1e-6
        z_score = (null_mean - best_mse) / null_std

        best_face, best_omit, best_perm = cube_best_meta[best_c]

        results.append({
            'p2k_preset': p_name,
            'best_cube_id': int(best_c),
            'best_cube_name': cube_names[best_c],
            'best_rmse_db': math.sqrt(best_mse),
            'second_cube_id': int(second_c),
            'second_cube_name': cube_names[second_c],
            'second_rmse_db': math.sqrt(second_mse),
            'margin_mse': margin,
            'z_score': z_score,
            'face': best_face,
            'omitted_stage': best_omit
        })

    results.sort(key=lambda x: x['best_rmse_db'])

    print("\n" + "="*115)
    print(f"{'P2K Preset':<24s} | {'Best Cube Match':<20s} | {'RMSE':<8s} | {'2nd Best Cube':<20s} | {'Margin':<8s} | {'Z-Score':<7s} | {'Face':<16s} | {'Drop'}")
    print("="*115)
    for r in results:
        print(f"{r['p2k_preset']:<24s} | #{r['best_cube_id']:03d} {r['best_cube_name'][:14]:<14s} | {r['best_rmse_db']:6.2f}dB | #{r['second_cube_id']:03d} {r['second_cube_name'][:14]:<14s} | {r['margin_mse']:7.2f}  | {r['z_score']:6.2f}σ | {r['face']:<16s} | S{r['omitted_stage']}")
    print("="*115)

    with open('ref/p2k_morpheus_acoustic_audit.json', 'w', encoding='utf-8') as fp:
        json.dump(results, fp, indent=2)
    print(f"\nAudit complete in {time.time() - t0:.2f}s. Saved to ref/p2k_morpheus_acoustic_audit.json")

if __name__ == '__main__':
    run_fast_audit()
