"""Rewrite decoded/*.json and decoded/index.json to the 48 kHz hardware datum.

    python redatum_decoded_48k.py

The stored words are datum free: a pole or zero is a normalised angle
theta in [0, pi] and a radius. Every hz field is recomputed from the words
as f = theta / pi * 24000, datum_hz becomes 48000, and frequency_grid_hz is
the same 256 point geometric grid from 20 Hz to 0.98 * 24000 Hz. Words, hex,
radii, scale and span are untouched.
"""
import glob, json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, r'C:\Users\hooki\morph6-experiment\tools')
from source_target import decode_word, _geo
FS = 48000.0
def hz_of(k1w, k2w):
    q1, q2 = decode_word(k1w), decode_word(k2w)
    if q1 <= 0 or q2 <= 0: return None
    hz, r = _geo({'k1': -math.log(q1), 'k2': -math.log(q2)}, FS)
    return hz
def redatum(d):
    d['datum_hz'] = FS
    n = len(d.get('frequency_grid_hz', [])) or 256
    top = FS / 2 * 0.98
    d['frequency_grid_hz'] = [round(20.0 * (top / 20.0) ** (i / (n - 1)), 4) for i in range(n)]
    changed = 0
    for c in d.get('corner_data', []):
        for s in c.get('sections', []):
            w = s['words']
            for key, (i1, i2) in (('zero', (0, 1)), ('pole', (2, 3))):
                g = s.get(key)
                if isinstance(g, dict) and 'hz' in g:
                    hz = hz_of(w[i1], w[i2])
                    if hz is not None: g['hz'] = round(hz, 3); changed += 1
    return changed
def main():
    files = sorted(glob.glob(os.path.join(HERE, 'decoded', '*', '*.json')))
    total = 0
    for path in files:
        d = json.load(open(path, encoding='utf-8'))
        total += redatum(d)
        json.dump(d, open(path, 'w', encoding='utf-8', newline='\n'), indent=1)
    idx_path = os.path.join(HERE, 'decoded', 'index.json')
    idx = json.load(open(idx_path, encoding='utf-8')); idx['datum_hz'] = FS
    json.dump(idx, open(idx_path, 'w', encoding='utf-8', newline='\n'), indent=1)
    print(f'{len(files)} records, {total} hz fields recomputed at {FS:.0f} Hz')
if __name__ == '__main__':
    main()
