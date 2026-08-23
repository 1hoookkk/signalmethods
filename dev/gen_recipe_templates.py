import collections, glob, json, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cube_skeleton_library as csl

SR = 44100.0
OUT = 'native/trench-core/src/p2k/templates.cpp'


def peaks(f, db, lo=60, hi=12000, prom=6.0, maxn=6):
    f, db = np.asarray(f), np.asarray(db)
    m = (f >= lo) & (f <= hi)
    f, db = f[m], db[m]
    k = max(3, len(f) // 100)
    sm = np.convolve(db, np.ones(k) / k, mode='same')
    out = []
    for i in range(1, len(sm) - 1):
        if sm[i] >= sm[i - 1] and sm[i] > sm[i + 1]:
            l = i
            while l > 0 and sm[l] > sm[i] - prom:
                l -= 1
            r = i
            while r < len(sm) - 1 and sm[r] > sm[i] - prom:
                r += 1
            if sm[l] <= sm[i] - prom and sm[r] <= sm[i] - prom:
                l3 = i
                while l3 > 0 and sm[l3] > sm[i] - 3:
                    l3 -= 1
                r3 = i
                while r3 < len(sm) - 1 and sm[r3] > sm[i] - 3:
                    r3 += 1
                out.append((sm[i], f[i], max(f[r3] - f[l3], 10.0)))
    out.sort(reverse=True)
    return sorted((h, b) for _, h, b in out[:maxn])


def mouth_name(stem):
    parts = stem.split('-')
    rest = [p for p in parts[2:] if p != 'tense']
    return ' '.join([parts[0]] + rest)


rows = []

ph = json.load(open('recipes/tables/phonetic_formants.json'))
for key, v in ph['vowels'].items():
    poles = [(v['f%d' % i], v['b%d' % i]) for i in range(1, 6) if v.get('f%d' % i)]
    rows.append(('VOWELS', v['ipa'], poles))

for p in sorted(glob.glob('recipes/poses/*.json')):
    d = json.load(open(p))
    rows.append(('POSES', d['name'], [(f['hz'], f['bandwidth_hz']) for f in d['formants']]))

for p in sorted(glob.glob('recipes/vocal/dvtd/subject-*/*/*-vvtf-measured.txt')):
    stem = os.path.basename(os.path.dirname(p))
    a = np.loadtxt(p, skiprows=1)
    f, mag = a[:, 0], a[:, 1]
    m = (f > 1) & (mag > 0)
    group = 'MOUTHS %s' % stem.split('-')[0].upper()
    rows.append((group, mouth_name(stem), peaks(f[m], 20 * np.log10(mag[m]))))

for p in sorted(glob.glob('recipes/tfs/*.tf.json')):
    stem = os.path.basename(p)[:-8]
    d = json.load(open(p))
    if stem.startswith('uiowa_'):
        group, name = 'BODIES', stem[6:].replace('_', ' ')
    else:
        group, name = 'OBJECTS', stem.replace('_', ' ')
    rows.append((group, name, peaks(d['freqs_hz'], d['mag_db'])))

cube_corners = csl.load_corners()
cube_post, _ = csl.postures(cube_corners, min_cubes=3)
for group in cube_post:
    poles = csl.mean_poles(cube_corners, group)
    if all(hz < 40.0 for hz, _ in poles) or any(hz <= 0.0 for hz, _ in poles):
        continue
    poles = sorted(sorted(poles, key=lambda p: p[1])[:6])
    names = collections.Counter(cube_corners[i]['name'] for i in group)
    rows.append(('CUBES', '%s +%d' % (names.most_common(1)[0][0], len(names) - 1), poles))

order = ['VOWELS', 'POSES', 'MOUTHS S1', 'MOUTHS S2', 'BODIES', 'OBJECTS', 'CUBES']
rows = [r for key in order for r in rows if r[0] == key]
rows = [(g, n, sorted(p, key=lambda x: -x[1])[:6]) for g, n, p in rows]
rows = [(g, n, sorted(p)) for g, n, p in rows if p]

names = [n for _, n, _ in rows]
assert len(set(names)) == len(names), [n for n in names if names.count(n) > 1]

def cstr(text):
    out = '"'
    escaped = False
    for byte in text.encode('utf-8'):
        if byte < 0x80:
            if escaped:
                out += '" "'
                escaped = False
            out += chr(byte)
        else:
            out += chr(92) + 'x%02x' % byte
            escaped = True
    return out + '"'


lines = ['#include "trench/core/formants.hpp"', '', 'namespace trench::core::p2k {', '',
         'namespace {', '', 'constexpr std::array<Posture, %d> kTemplates{{' % len(rows)]
for group, name, poles in rows:
    body = ', '.join('{%.4f, %.4f}' % (hz, bw) for hz, bw in poles)
    lines.append('    {%s, "%s", %d, {{%s}}},' % (cstr(name), group, len(poles), body))
lines += ['}};', '', '}  // namespace', '',
          'std::span<const Posture> templates() { return kTemplates; }', '',
          '}  // namespace trench::core::p2k', '']
open(OUT, 'w').write('\n'.join(lines))
for key in order:
    print(key, sum(1 for g, _, _ in rows if g == key))
print('total', len(rows))
