import sys, os, math, json, struct
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'cell_dictionary'))
import decode_lib as dl
import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 39062.5
FREQS = np.array([40 * (16000/40)**(i/255) for i in range(256)])

def load_cube(cid):
    path = os.path.join(REPO, 'ref', 'morpheus', 'bodies', f'{cid:03d}_*.body')
    import glob
    matches = glob.glob(path)
    if not matches:
        return None
    data = open(matches[0], 'rb').read()
    if len(data) == 560:
        return dl.decode_native_body(data, SR)
    elif len(data) == 240:
        return dl.decode_p2k_body(data, SR)
    return None

def response_db(corners, m, q, z):
    ci_weights = []
    for ci in range(len(corners)):
        wm = (m if (ci & 1) else (1 - m))
        wq = (q if (ci & 2) else (1 - q))
        wz = (z if (ci & 4) else (1 - z)) if len(corners) == 8 else 1.0
        ci_weights.append((ci, wm * wq * wz))

    db = np.zeros(len(FREQS))
    for fi, f in enumerate(FREQS):
        total = 0
        for ci, w in ci_weights:
            if w < 1e-6:
                continue
            stage_sum = 0
            for geom in corners[ci]:
                if dl.stage_is_identity(geom):
                    continue
                gg = geom if geom.scale is not None else dl.StageGeometry(geom.pole, geom.zero, 1.0)
                bq = dl.stage_biquad(gg, SR)
                stage_sum += dl.stage_response_db(bq, [f], SR)[0]
            total += w * stage_sum
        db[fi] = total
    return db

def summarize(db):
    peak_i = int(np.argmax(db))
    trough_i = int(np.argmin(db))
    low = float(np.mean(db[:20]))
    mid = float(np.mean(db[80:150]))
    high = float(np.mean(db[200:]))
    return {
        'peak_hz': float(FREQS[peak_i]), 'peak_db': float(db[peak_i]),
        'trough_hz': float(FREQS[trough_i]), 'trough_db': float(db[trough_i]),
        'low': round(low, 1), 'mid': round(mid, 1), 'high': round(high, 1),
        'slope': round(high - low, 1),
    }

TEST_CUBES = [
    (0, 'Null Cube', 'Flat response under all settings'),
    (1, 'LPFlange.4', 'Flanger: notches at octave intervals, LP filter'),
    (29, 'Vocal Cube', 'VOW: variety of vocal sounds, Freq=vowel movement'),
    (48, 'MdQ 4PoleLP', 'LPF: 4-pole lowpass, Morph=cutoff, Freq=cutoff'),
    (70, 'BassEQ 1.4', 'EQ: bass equalization'),
    (82, 'BrassRez.4', 'Complex: brass resonance'),
    (103, 'BassXpress', 'Bass boost + sharp notch, Freq=notch freq'),
    (199, '2 Poles', 'Standard 2-pole filter'),
    (230, 'Vowel Space2', 'VOW: different vowel at each corner'),
    (278, 'Analog.4', 'Analog-style filter'),
]

print('Cube decoder validation against Rossum manual descriptions')
print('=' * 70)

for cid, name, expected in TEST_CUBES:
    corners = load_cube(cid)
    if not corners:
        print(f'\nC{cid:03d} {name}: FILE NOT FOUND')
        continue

    n_corners = len(corners)
    is_4 = n_corners == 4

    r_m0 = response_db(corners, 0, 0, 0)
    r_m1 = response_db(corners, 1, 0, 0)
    r_q1 = response_db(corners, 0, 1, 0)
    r_mq = response_db(corners, 1, 1, 0)

    s_m0 = summarize(r_m0)
    s_m1 = summarize(r_m1)
    s_q1 = summarize(r_q1)

    morph_change = round(float(np.sqrt(np.mean((r_m1 - r_m0)**2))), 1)
    q_change = round(float(np.sqrt(np.mean((r_q1 - r_m0)**2))), 1)

    print(f'\nC{cid:03d} {name}  ({"2D" if is_4 else "3D"}, {n_corners} corners)')
    print(f'  Expected: {expected}')
    print(f'  M0Q0: low={s_m0["low"]:+.0f} mid={s_m0["mid"]:+.0f} high={s_m0["high"]:+.0f}  slope={s_m0["slope"]:+.0f}  peak@{s_m0["peak_hz"]:.0f}Hz')
    print(f'  M1Q0: low={s_m1["low"]:+.0f} mid={s_m1["mid"]:+.0f} high={s_m1["high"]:+.0f}  slope={s_m1["slope"]:+.0f}  peak@{s_m1["peak_hz"]:.0f}Hz')
    print(f'  M0Q1: low={s_q1["low"]:+.0f} mid={s_q1["mid"]:+.0f} high={s_q1["high"]:+.0f}  slope={s_q1["slope"]:+.0f}  peak@{s_q1["peak_hz"]:.0f}Hz')
    print(f'  Morph RMS change: {morph_change:.1f} dB   Q RMS change: {q_change:.1f} dB')

    ok = True
    verdict = []
    if cid == 0:
        flat = all(abs(s_m0[k]) < 1 for k in ['low','mid','high'])
        no_change = morph_change < 0.5 and q_change < 0.5
        verdict.append(f'flat={flat} nochange={no_change}')
        if not (flat and no_change): ok = False
    elif 'LPF' in expected or 'lowpass' in expected.lower():
        is_lp = s_m0['slope'] < -5
        morph_moves = morph_change > 3
        verdict.append(f'lowpass_shape={is_lp} morph_active={morph_moves}')
        if not is_lp: ok = False
    elif 'VOW' in expected or 'vowel' in expected.lower():
        has_peaks = s_m0['peak_db'] - min(s_m0['low'], s_m0['high']) > 5
        morph_different = morph_change > 3
        verdict.append(f'formant_peaks={has_peaks} morph_varies={morph_different}')
        if not has_peaks: ok = False
    elif 'notch' in expected.lower() or 'Flang' in expected:
        has_notch = s_m0['trough_db'] < s_m0['peak_db'] - 10
        verdict.append(f'has_notch={has_notch}')
    elif 'EQ' in expected or 'bass' in expected.lower():
        morph_active = morph_change > 2
        verdict.append(f'morph_active={morph_active}')
    else:
        morph_active = morph_change > 1
        verdict.append(f'morph_active={morph_active}')

    status = 'PASS' if ok else 'CHECK'
    print(f'  {status}: {" ".join(verdict)}')

print(f'\n{"=" * 70}')
print('Done. CHECK items need manual review against the Rossum description.')
