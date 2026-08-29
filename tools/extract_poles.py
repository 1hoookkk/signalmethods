"""One-button all-pole caricature: WAV in, six (f, B) stages out.

    python tools/extract_poles.py sound.wav [--out-dir DIR] [--mode stages|lpc]
                                            [--pairs N] [--orders 4,6,8,10,12] [--all]

stages (default): the energy-weighted peak-hold envelope is split into the six
acoustic bands (sub / warmth / vowel-horn / presence / sizzle / air); each
band contributes its dominant peak as centre frequency + bandwidth, in stage
order S1..S6 (the workstation's ordered sections).

lpc: several candidate regions (attack, peak, post-attack, sustained,
multi-frame average) x several Burg orders x three analysis rates are fitted,
scored against the same envelope on a log-frequency grid, and the best
penalised fit wins with 1-6 useful pairs.

Poles are exported as frequency / bandwidth (bw = -fs/pi * ln r) to .fbw
(workstation pole material), .json, .csv and a C++ initializer.
"""
import argparse, json, math, os
import numpy as np

MIN_HZ = 20.0
MIN_BW_HZ = 1.0
MAX_BW_HZ = 20000.0
ANALYSIS_RATE_CAP = 44100.0
FRAME = 4096
HOP = 1024
PAIR_PENALTY_DB = 0.12
PRUNE_DB = 0.5
MIN_SMOOTH_HZ = 80.0
ABSENT_BELOW_DB = 40.0

BANDS = [
    (50.0, 250.0, 10.0, 40.0, 'sub weight / chest thump'),
    (300.0, 800.0, 50.0, 120.0, 'warmth / throat / box resonance'),
    (900.0, 2200.0, 80.0, 200.0, 'vowel clarity / horn bite'),
    (2500.0, 4500.0, 150.0, 350.0, 'attack presence / metallic edge'),
    (5000.0, 8500.0, 300.0, 600.0, 'sizzle / crack / scrape'),
    (9000.0, 16000.0, 800.0, 2000.0, 'air sheen / top fizz'),
]


def load_mono(path):
    try:
        import soundfile as sf
        x, rate = sf.read(path, dtype='float64', always_2d=True)
        x = x.mean(axis=1)
    except Exception:
        import wave
        with wave.open(path, 'rb') as w:
            rate = w.getframerate(); n = w.getnframes(); ch = w.getnchannels()
            sw = w.getsampwidth(); raw = w.readframes(n)
        if sw == 2:
            x = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768.0
        elif sw == 4:
            x = np.frombuffer(raw, dtype='<i4').astype(np.float64) / 2147483648.0
        elif sw == 3:
            b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
            x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8)
                 | (b[:, 2].astype(np.int32) << 16))
            x = np.where(x >= 1 << 23, x - (1 << 24), x).astype(np.float64) / (1 << 23)
        else:
            raise SystemExit('unsupported sample width %d' % sw)
        if ch > 1:
            x = x.reshape(-1, ch).mean(axis=1)
    x = x - x.mean()
    peak = np.max(np.abs(x)) if len(x) else 0.0
    if peak > 0:
        x = x / peak * 0.9
    if rate > ANALYSIS_RATE_CAP:
        from scipy.signal import resample_poly
        g = math.gcd(int(rate), int(ANALYSIS_RATE_CAP))
        x = resample_poly(x, int(ANALYSIS_RATE_CAP) // g, int(rate) // g)
        rate = ANALYSIS_RATE_CAP
    if len(x) < FRAME:
        x = np.pad(x, (0, FRAME - len(x)))
    return x, float(rate)


def burg(frame, order):
    x = np.asarray(frame, dtype=np.float64)
    if len(x) <= order + 1:
        return None
    f = x.copy(); b = x.copy()
    a = np.array([1.0])
    for _ in range(order):
        fm = f[1:]; bm = b[:-1]
        den = np.dot(fm, fm) + np.dot(bm, bm)
        if den <= 0:
            break
        k = -2.0 * np.dot(fm, bm) / den
        if abs(k) >= 1.0:
            break
        a = np.concatenate((a, [0.0]))
        a = a + k * a[::-1]
        f, b = fm + k * bm, bm + k * fm
    return a if len(a) > 1 else None


def levinson(r, order):
    if r[0] <= 0:
        return None
    a = np.zeros(order + 1); a[0] = 1.0
    err = r[0]
    for m in range(1, order + 1):
        acc = r[m] + np.dot(a[1:m], r[m - 1:0:-1])
        k = -acc / err
        if abs(k) >= 1.0:
            break
        prev = a.copy()
        for i in range(1, m):
            a[i] = prev[i] + k * prev[m - i]
        a[m] = k
        err *= (1.0 - k * k)
        if err <= 0:
            break
    return a


def frame_rms(x, frame=FRAME, hop=HOP):
    count = max(1, (len(x) - frame) // hop + 1)
    return np.array([math.sqrt(np.mean(x[i * hop:i * hop + frame] ** 2) + 1e-30)
                     for i in range(count)]), count


def log_grid(rate, points=384):
    return np.geomspace(MIN_HZ, min(20000.0, 0.49 * rate), points)


def envelope(x, rate, grid, frame=FRAME, hop=HOP):
    window = np.hanning(frame)
    count = max(1, (len(x) - frame) // hop + 1)
    acc = np.zeros(frame // 2 + 1); weight = 0.0
    for i in range(count):
        seg = x[i * hop:i * hop + frame]
        if len(seg) < frame:
            seg = np.pad(seg, (0, frame - len(seg)))
        if np.mean(seg ** 2) <= 1e-12:
            continue
        acc += np.abs(np.fft.rfft(seg * window)) ** 2; weight += 1.0
    if weight == 0:
        acc += 1e-12; weight = 1.0
    power = acc / weight
    bins = np.fft.rfftfreq(frame, 1.0 / rate)
    out = np.empty(len(grid))
    for k, f in enumerate(grid):
        half = max(f * (2 ** (1 / 6) - 1), MIN_SMOOTH_HZ)
        sel = (bins >= f - half) & (bins <= f + half)
        if not np.any(sel):
            idx = min(len(bins) - 1, int(round(f / rate * frame)))
            sel = np.zeros(len(bins), bool); sel[idx] = True
        out[k] = 10.0 * math.log10(np.max(power[sel]) + 1e-20)
    return out, power, bins


def all_pole_db(a, rate, grid):
    z = np.exp(-1j * 2 * np.pi * np.outer(grid, np.arange(len(a))) / rate)
    return -20.0 * np.log10(np.abs(z @ a) + 1e-20)


def pairs_from_poly(a, rate):
    roots = np.roots(a)
    pairs = []; real = 0; reflected = 0
    used = np.zeros(len(roots), bool)
    for i, z in enumerate(roots):
        if used[i]:
            continue
        if abs(z.imag) < 1e-9:
            real += 1; used[i] = True
            continue
        if z.imag < 0:
            continue
        d = np.abs(roots - np.conj(z))
        j = int(np.argmin(d))
        if d[j] > 1e-6 or used[j]:
            continue
        used[i] = used[j] = True
        r = abs(z)
        if r >= 1.0:
            r = 1.0 / r; reflected += 1
        hz = math.atan2(z.imag, z.real) / (2 * math.pi) * rate
        bw = -math.log(max(r, 1e-12)) * rate / math.pi
        if hz < MIN_HZ or hz > 0.49 * rate or bw > 0.5 * rate:
            continue
        pairs.append((hz, bw))
    return sorted(pairs), real, reflected


def poly_from_pairs(pairs, rate):
    a = np.array([1.0])
    for hz, bw in pairs:
        r = math.exp(-math.pi * bw / rate)
        th = 2 * math.pi * hz / rate
        a = np.convolve(a, [1.0, -2 * r * math.cos(th), r * r])
    return a


def level_weights(target, floor_db=30.0):
    return np.clip(10.0 ** ((target - np.max(target)) / 20.0), 10.0 ** (-floor_db / 20.0), 1.0)


def weighted_rms(model, target, weight):
    diff = model - target
    offset = np.sum(weight * diff) / np.sum(weight)
    return math.sqrt(np.sum(weight * (diff - offset) ** 2) / np.sum(weight))


def decimated(x, rate, factor):
    if factor == 1:
        return x, rate
    from scipy.signal import resample_poly
    return resample_poly(x, 1, factor), rate / factor


def candidate_regions(x, rate):
    rms, count = frame_rms(x)
    peak = int(np.argmax(rms))
    threshold = rms[peak] * 0.1
    onset = next((i for i in range(count) if rms[i] >= threshold), peak)
    post = min(count - 1, peak + max(1, int(round(0.1 * rate / HOP))))
    strong = [i for i in range(count) if rms[i] >= 0.5 * rms[peak]]
    sustained = strong[len(strong) // 2] if strong else peak
    regions = [('attack', onset), ('peak', peak), ('post-attack', post), ('sustained', sustained)]
    seen = set(); out = []
    for name, i in regions:
        if i in seen:
            continue
        seen.add(i)
        out.append((name, x[i * HOP:i * HOP + FRAME]))
    avg_frames = [i for i in range(count) if rms[i] >= 0.3 * rms[peak]]
    return out, avg_frames


def averaged_autocorrelation(x, frames, order):
    window = np.hanning(FRAME)
    r = np.zeros(order + 1); weight = 0.0
    for i in frames:
        seg = x[i * HOP:i * HOP + FRAME]
        if len(seg) < FRAME:
            continue
        seg = seg * window
        if np.dot(seg, seg) <= 0:
            continue
        r += np.correlate(seg, seg, 'full')[FRAME - 1:FRAME + order]; weight += 1.0
    return r / weight if weight else None


def lpc_fit(x, rate, grid, target, orders, max_pairs, exact_pairs=None,
            prune_db=PRUNE_DB, penalty_db=PAIR_PENALTY_DB):
    weight = level_weights(target)
    candidates = []
    for factor in (1, 2, 4):
        xr, rr = decimated(x, rate, factor)
        if len(xr) < FRAME:
            xr = np.pad(xr, (0, FRAME - len(xr)))
        regions, avg_frames = candidate_regions(xr, rr)
        for order in orders:
            for name, seg in regions:
                if len(seg) < order + 2:
                    continue
                a = burg(seg, order)
                if a is not None:
                    candidates.append((name, rr, order, a))
            r = averaged_autocorrelation(xr, avg_frames, order)
            if r is not None:
                a = levinson(r, order)
                if a is not None:
                    candidates.append(('multi-frame', rr, order, a))
    scored = []
    for name, rr, order, a in candidates:
        pairs, real, reflected = pairs_from_poly(a, rr)
        if not pairs:
            continue
        err = weighted_rms(all_pole_db(poly_from_pairs(pairs, rate), rate, grid), target, weight)
        scored.append({'region': name, 'rate': rr, 'order': order, 'pairs': pairs, 'error': err,
                       'penalised': err + penalty_db * len(pairs),
                       'real_roots': real, 'reflected': reflected})
    if not scored:
        return None, []
    scored.sort(key=lambda c: c['penalised'])
    if exact_pairs is not None:
        max_pairs = exact_pairs
        prune_db = 0.0
        enough = [c for c in scored if len(c['pairs']) >= exact_pairs]
        if enough:
            scored = sorted(enough, key=lambda c: c['error']) + [c for c in scored if c not in enough]
    best = dict(scored[0])
    pairs = list(best['pairs'])

    def error_of(kept):
        return weighted_rms(all_pole_db(poly_from_pairs(kept, rate), rate, grid), target, weight)

    while len(pairs) > max_pairs:
        pairs.pop(min(range(len(pairs)), key=lambda k: error_of(pairs[:k] + pairs[k + 1:])))

    def contributions(kept):
        base = error_of(kept)
        out = []
        for k in range(len(kept)):
            without = kept[:k] + kept[k + 1:]
            out.append((error_of(without) if without else float('inf')) - base)
        return base, out

    base, contribution = contributions(pairs)
    while len(pairs) > 1 and min(contribution) < prune_db:
        pairs.pop(int(np.argmin(contribution)))
        base, contribution = contributions(pairs)
    best['pairs'] = pairs; best['error'] = base; best['contribution'] = contribution
    return best, scored


def band_edges(rate):
    edges = [BANDS[0][0]]
    for k in range(len(BANDS) - 1):
        edges.append(math.sqrt(BANDS[k][1] * BANDS[k + 1][0]))
    edges.append(min(BANDS[-1][1], 0.49 * rate))
    return edges


def nearest_lpc_bandwidth(f0, scored, max_octaves=0.4):
    best = None
    for candidate in scored:
        for hz, bw in candidate['pairs']:
            distance = abs(math.log2(hz / f0))
            if distance > max_octaves:
                continue
            key = (candidate['error'], distance)
            if best is None or key < best[0]:
                best = (key, bw)
    return None if best is None else best[1]


def stage_caricature(grid, target, power, bins, rate, scored=()):
    edges = band_edges(rate)
    ceiling = float(np.max(target))
    stages = []
    for k, (band_lo, band_hi, bw_lo, bw_hi, role) in enumerate(BANDS):
        lo, hi = edges[k], edges[k + 1]
        inside = np.where((grid >= lo) & (grid <= hi))[0]
        if len(inside) == 0:
            stages.append({'stage': k + 1, 'freq_hz': math.sqrt(band_lo * band_hi), 'bandwidth_hz': bw_hi,
                           'raw_width_hz': 0.0, 'level_db': -120.0, 'present': False, 'role': role})
            continue
        g = int(inside[np.argmax(target[inside])])
        level = float(target[g])
        f0 = float(grid[g])
        half = max(f0 * (2 ** (1 / 6) - 1), MIN_SMOOTH_HZ)
        sel = np.where((bins >= max(lo, f0 - half)) & (bins <= min(hi, f0 + half)))[0]
        if len(sel):
            j = int(sel[np.argmax(power[sel])])
            if 0 < j < len(power) - 1:
                ya, yb, yc = (math.log(power[j - 1] + 1e-20), math.log(power[j] + 1e-20),
                              math.log(power[j + 1] + 1e-20))
                den = ya - 2 * yb + yc
                shift = 0.5 * (ya - yc) / den if abs(den) > 1e-12 else 0.0
                f0 = float(np.clip(bins[j] + shift * (bins[1] - bins[0]), lo, hi))
            else:
                f0 = float(bins[j])
        left = g
        while left > 0 and grid[left - 1] >= lo and target[left - 1] >= level - 3.0:
            left -= 1
        right = g
        while right < len(grid) - 1 and grid[right + 1] <= hi and target[right + 1] >= level - 3.0:
            right += 1
        left_w = grid[g] - grid[left]; right_w = grid[right] - grid[g]
        hit_left = left == 0 or grid[left] <= lo * 1.0001 or left == g
        hit_right = right == len(grid) - 1 or grid[right] >= hi * 0.9999 or right == g
        if hit_left and not hit_right:
            raw = 2.0 * right_w
        elif hit_right and not hit_left:
            raw = 2.0 * left_w
        else:
            raw = left_w + right_w
        envelope_bw = max(raw - 2.0 * half, bw_lo) if raw > 0 else bw_lo
        lpc_bw = nearest_lpc_bandwidth(f0, scored)
        source = 'envelope' if lpc_bw is None else 'lpc'
        bw = float(np.clip(envelope_bw if lpc_bw is None else lpc_bw, bw_lo * 0.5, bw_hi * 2.0))
        stages.append({'stage': k + 1, 'freq_hz': f0, 'bandwidth_hz': bw, 'raw_width_hz': float(raw),
                       'bandwidth_source': source, 'level_db': level - ceiling,
                       'present': level >= ceiling - ABSENT_BELOW_DB, 'role': role})
    return stages


def fmt_hz(v):
    return '%.2f kHz' % (v / 1000.0) if v >= 1000.0 else '%.0f Hz' % v


def clamp_pole(hz, bw, rate):
    return (round(min(max(hz, MIN_HZ), 0.5 * rate), 3), round(min(max(bw, MIN_BW_HZ), MAX_BW_HZ), 3))


def export(path, out_dir, rate, mode, stages, lpc):
    stem = os.path.splitext(os.path.basename(path))[0]
    base = os.path.join(out_dir, stem)
    if mode == 'stages':
        rows = [dict(clamped=clamp_pole(s['freq_hz'], s['bandwidth_hz'], rate), present=s['present']) for s in stages]
    else:
        rows = [dict(clamped=clamp_pole(hz, bw, rate), present=True) for hz, bw in lpc['pairs']]
    with open(base + '.fbw', 'w', newline='\n') as f:
        f.write('# %s (%s)\n' % (stem, mode))
        for row in rows:
            f.write('%.4f %.4f\n' % row['clamped'])
    with open(base + '.poles.json', 'w', newline='\n') as f:
        json.dump({'sample_rate': int(rate), 'source': os.path.basename(path), 'mode': mode,
                   'poles': [{'freq_hz': r['clamped'][0], 'bandwidth_hz': r['clamped'][1]} for r in rows],
                   'stages': [{'stage': s['stage'], 'freq_hz': round(s['freq_hz'], 3),
                               'bandwidth_hz': round(s['bandwidth_hz'], 3),
                               'raw_width_hz': round(s['raw_width_hz'], 3),
                               'bandwidth_source': s['bandwidth_source'],
                               'level_db': round(s['level_db'], 2), 'present': s['present'],
                               'role': s['role']} for s in stages],
                   'lpc': None if lpc is None else {
                       'selected': {'region': lpc['region'], 'order': lpc['order'],
                                    'analysis_rate': int(lpc['rate'])},
                       'fit_error_db_rms': round(lpc['error'], 3),
                       'poles': [{'freq_hz': round(hz, 3), 'bandwidth_hz': round(bw, 3)}
                                 for hz, bw in lpc['pairs']]}}, f, indent=2)
        f.write('\n')
    with open(base + '.poles.csv', 'w', newline='\n') as f:
        f.write('stage,freq_hz,bandwidth_hz,level_db,present,role\n')
        for s in stages:
            f.write('%d,%.3f,%.3f,%.2f,%s,%s\n' % (s['stage'], s['freq_hz'], s['bandwidth_hz'],
                                                   s['level_db'], 'true' if s['present'] else 'false', s['role']))
    with open(base + '.poles.h', 'w', newline='\n') as f:
        f.write('{"%s", {{' % stem.upper().replace('-', ' ').replace('_', ' '))
        cells = ['{%.2f, %.2f, %s}' % (r['clamped'][0], r['clamped'][1], 'true' if r['present'] else 'false')
                 for r in rows]
        cells += ['{22050.00, 1000000000.00, false}'] * (6 - len(cells))
        f.write(', '.join(cells[:6])); f.write('}}},\n')
    return base


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('wav')
    ap.add_argument('--out-dir', default=None)
    ap.add_argument('--mode', choices=('stages', 'lpc'), default='stages')
    ap.add_argument('--max-pairs', type=int, default=6)
    ap.add_argument('--orders', default='4,6,8,10,12')
    ap.add_argument('--pairs', type=int, default=None, help='lpc: force exactly this many pairs (1-6)')
    ap.add_argument('--prune-db', type=float, default=PRUNE_DB, help='lpc: drop a pair unless it buys this much fit')
    ap.add_argument('--penalty-db', type=float, default=PAIR_PENALTY_DB, help='lpc: per-pair cost when choosing')
    ap.add_argument('--all', action='store_true', help='print every scored lpc candidate')
    args = ap.parse_args()
    orders = sorted({int(v) for v in args.orders.split(',') if v.strip()})
    max_pairs = max(1, min(6, args.max_pairs))
    exact = None if args.pairs is None else max(1, min(6, args.pairs))

    x, rate = load_mono(args.wav)
    grid = log_grid(rate)
    target, power, bins = envelope(x, rate, grid)
    lpc, scored = lpc_fit(x, rate, grid, target, orders, max_pairs, exact, args.prune_db, args.penalty_db)
    stages = stage_caricature(grid, target, power, bins, rate, scored)
    if args.mode == 'lpc' and lpc is None:
        raise SystemExit('no stable all-pole candidate found')

    out_dir = args.out_dir or os.path.dirname(os.path.abspath(args.wav))
    os.makedirs(out_dir, exist_ok=True)
    base = export(args.wav, out_dir, rate, args.mode, stages, lpc)

    print('Six-stage caricature (dominant peak per band, -3 dB width):')
    for s in stages:
        flag = ' ' if s['present'] else 'x'
        print('S%d %s %-10s BW %-10s %6.1f dB  %-8s %s' % (s['stage'], flag, fmt_hz(s['freq_hz']),
                                                           fmt_hz(s['bandwidth_hz']), s['level_db'],
                                                           s['bandwidth_source'], s['role']))
    if lpc is not None:
        print('LPC check: %s, Burg order %d at %.0f Hz, %d useful pair%s, %.2f dB RMS' % (
            lpc['region'], lpc['order'], lpc['rate'], len(lpc['pairs']),
            '' if len(lpc['pairs']) == 1 else 's', lpc['error']))
        for k, (hz, bw) in enumerate(lpc['pairs'], 1):
            print('  %d   %-10s BW %-10s (+%.2f dB if removed)' % (k, fmt_hz(hz), fmt_hz(bw), lpc['contribution'][k - 1]))
        if args.all:
            print('%-12s %7s %5s %5s %8s %9s' % ('region', 'rate', 'order', 'pairs', 'rms dB', 'penalised'))
            for c in scored[:12]:
                print('%-12s %7.0f %5d %5d %8.2f %9.2f' % (c['region'], c['rate'], c['order'], len(c['pairs']), c['error'], c['penalised']))
    print('Wrote %s.fbw (%s) / .poles.json / .poles.csv / .poles.h' % (base, args.mode))


if __name__ == '__main__':
    main()
