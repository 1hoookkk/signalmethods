"""ESPS-style pole scout: WAV in, F/BW pole candidates out.

LPC per frame (autocorrelation + Levinson-Durbin), roots of the prediction
polynomial, resonant roots kept as F/BW (bw = -ln|r| * rate / pi), tracks
merged across frames by median. Output: one F/BW row per pole, ready for the
POLES ingestion lane.
"""
import sys, wave, math
import numpy as np

def read_wav_mono(path):
    with wave.open(path, 'rb') as w:
        rate = w.getframerate(); n = w.getnframes(); ch = w.getnchannels()
        sw = w.getsampwidth(); raw = w.readframes(n)
    if sw == 2:
        x = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768.0
    elif sw == 4:
        x = np.frombuffer(raw, dtype='<i4').astype(np.float64) / 2147483648.0
    elif sw == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = ((b[:,0].astype(np.int32)) | (b[:,1].astype(np.int32) << 8)
             | (b[:,2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x).astype(np.float64) / (1 << 23)
    else:
        raise SystemExit('unsupported sample width %d' % sw)
    if ch > 1:
        x = x.reshape(-1, ch).mean(axis=1)
    return x, rate

def lpc(frame, order):
    r = np.correlate(frame, frame, 'full')[len(frame)-1:len(frame)+order]
    if r[0] <= 0: return None
    R = np.array([[r[abs(i-j)] for j in range(order)] for i in range(order)])
    try:
        coef = np.linalg.solve(R + np.eye(order) * r[0] * 1e-9, -r[1:order+1])
    except np.linalg.LinAlgError:
        return None
    return np.concatenate(([1.0], coef))

def frame_poles(frame, rate, order, min_hz=60.0, max_bw=1500.0):
    frame = frame * np.hamming(len(frame))
    a = lpc(frame, order)
    if a is None: return []
    roots = np.roots(a)
    out = []
    for z in roots:
        if z.imag <= 0: continue
        r = abs(z)
        if r >= 1.0 or r < 0.6: continue
        hz = math.atan2(z.imag, z.real) / (2 * math.pi) * rate
        bw = -math.log(r) * rate / math.pi
        if hz > min_hz and hz < rate * 0.47 and bw < max_bw:
            out.append((hz, bw))
    return sorted(out)

def resample(x, rate, target):
    if rate <= target: return x, rate
    spec = np.fft.rfft(x)
    keep = int(len(spec) * target / rate)
    return np.fft.irfft(spec[:keep], n=int(len(x) * target / rate)), target

def scout(path, order=14, top=6, frame_ms=40, hop_ms=20, analysis_rate=10000):
    x, rate = read_wav_mono(path)
    x, rate = resample(x, rate, analysis_rate)
    peak = np.max(np.abs(x))
    if peak > 0: x = x / peak
    fl = int(rate * frame_ms / 1000); hop = int(rate * hop_ms / 1000)
    tracks = []
    for start in range(0, max(1, len(x) - fl), hop):
        frame = x[start:start+fl]
        if len(frame) < fl or np.sqrt(np.mean(frame**2)) < 1e-3: continue
        tracks.append(frame_poles(frame, rate, order))
    if not tracks: raise SystemExit('no voiced frames found')
    pool = [p for fr in tracks for p in fr]
    pool.sort()
    clusters = []
    for hz, bw in pool:
        for c in clusters:
            if abs(math.log2(hz / np.median([h for h,_ in c]))) < 1/6:
                c.append((hz, bw)); break
        else:
            clusters.append([(hz, bw)])
    clusters = [c for c in clusters if len(c) >= max(2, len(tracks) * 0.25)]
    clusters.sort(key=len, reverse=True)
    result = []
    for c in clusters[:top]:
        result.append((float(np.median([h for h,_ in c])),
                       float(np.median([b for _,b in c])), len(c)))
    result.sort()
    return result, rate, len(tracks)



def track(path, ntracks=4, order=12, analysis_rate=10000,
          frame_ms=7.5, hop_ms=5.0, TCOST=0.1, TFACT=5.0, FREQ_WT=0.015):
    """ESPS-style Viterbi: per-frame LPC root candidates assigned to
    continuous tracks. Costs follow the ESPS formant constants: a stationary
    cost on |log f - log f_prev| scaled by TFACT + TCOST, and FREQ_WT pulling
    each track toward its running seat. Returns per-frame (t, [(hz,bw)*ntracks])."""
    x, rate = read_wav_mono(path)
    x, rate = resample(x, rate, analysis_rate)
    peak = np.max(np.abs(x))
    if peak > 0: x = x / peak
    fl = int(rate * frame_ms / 1000); hop = int(rate * hop_ms / 1000)
    frames = []
    for start in range(0, max(1, len(x) - fl), hop):
        fr = x[start:start+fl]
        if len(fr) < fl: break
        if np.sqrt(np.mean(fr**2)) < 1e-3:
            frames.append((start / rate, None)); continue
        cands = frame_poles(fr, rate, order, max_bw=3000.0)
        frames.append((start / rate, cands if cands else None))
    from itertools import combinations
    tracks = None; out = []
    for t, cands in frames:
        if cands is None:
            out.append((t, tracks[:] if tracks else None)); continue
        best = None
        for pick in combinations(range(len(cands)), min(ntracks, len(cands))):
            chosen = [cands[i] for i in pick]
            cost = 0.0
            if tracks:
                for (hz, bw), (phz, pbw) in zip(chosen, tracks):
                    d = abs(math.log(max(hz, 1.0) / max(phz, 1.0)))
                    cost += TCOST + TFACT * d + FREQ_WT * hz / 1000.0
            else:
                cost = sum(FREQ_WT * hz / 1000.0 for hz, bw in chosen)
            if best is None or cost < best[0]:
                best = (cost, chosen)
        tracks = best[1]
        out.append((t, tracks[:]))
    return out



def arma(path, npoles=12, nzeros=12, lpc_order=24, analysis_rate=10000):
    """The combined-chain caricature (Prony-Shanks): model the recording as
    all-pole LPC, render that model's impulse response, then solve linear
    systems for a full pole-zero state - denominator from prediction over the
    IR tail, numerator by least squares against the IR. No optimiser."""
    x, rate = read_wav_mono(path)
    x, rate = resample(x, rate, analysis_rate)
    peak = np.max(np.abs(x))
    if peak > 0: x = x / peak
    seg = x[len(x)//4 : len(x)//4 + max(int(rate*0.5), 2048)]
    a = lpc(seg * np.hamming(len(seg)), lpc_order)
    if a is None: raise SystemExit('lpc failed')
    N = 512
    h = np.zeros(N); h[0] = 1.0
    for n in range(1, N):
        h[n] = -sum(a[k]*h[n-k] for k in range(1, len(a)) if n-k >= 0)
    rows = [[h[n-k] for k in range(1, npoles+1)] for n in range(nzeros+1, N)]
    rhs = [-h[n] for n in range(nzeros+1, N)]
    acoef, *_ = np.linalg.lstsq(np.array(rows), np.array(rhs), rcond=None)
    a_est = np.concatenate(([1.0], acoef))
    g = np.zeros(N)
    for n in range(N):
        g[n] = (1.0 if n == 0 else 0.0) - sum(
            a_est[k]*g[n-k] for k in range(1, len(a_est)) if n-k >= 0)
    G = np.zeros((N, nzeros+1))
    for k in range(nzeros+1):
        G[k:, k] = g[:N-k]
    b_est, *_ = np.linalg.lstsq(G, h, rcond=None)
    def fbw(poly, reflect):
        out = []
        for z in np.roots(poly):
            if z.imag <= 1e-9: continue
            r = abs(z)
            if reflect and r > 1.0: r = 1.0/r
            hz = math.atan2(z.imag, z.real)/(2*math.pi)*rate
            bw = -math.log(min(r, 0.999999))*rate/math.pi
            if 20.0 < hz < rate*0.49:
                out.append((hz, bw))
        return sorted(out)
    return fbw(a_est, False), fbw(b_est, True), rate

if __name__ == '__main__':
    if sys.argv[1] == 'arma':
        poles, zeros, rate = arma(sys.argv[2])
        print('# sections (pole | zero), %d Hz analysis' % rate)
        for i in range(max(len(poles), len(zeros))):
            ps = '%7.1f/%6.1f' % poles[i] if i < len(poles) else '   parked    '
            zs = '%7.1f/%6.1f' % zeros[i] if i < len(zeros) else '   parked    '
            print('S%d  P %s   Z %s' % (i+1, ps, zs))
        raise SystemExit
    if sys.argv[1] == 'track':
        rows = track(sys.argv[2], ntracks=int(sys.argv[3]) if len(sys.argv) > 3 else 4)
        step = max(1, len(rows) // 12)
        for t, tr in rows[::step]:
            if tr is None: print('%7.3fs  -' % t); continue
            print('%7.3fs  ' % t + '  '.join('%6.0f/%4.0f' % p for p in tr))
        raise SystemExit
    path = sys.argv[1]
    order = int(sys.argv[2]) if len(sys.argv) > 2 else 14
    poles, rate, frames = scout(path, order)
    print('# %s  rate %d  frames %d  order %d' % (path, rate, frames, order))
    for hz, bw, votes in poles:
        print('%9.2f %8.2f   # votes %d' % (hz, bw, votes))
