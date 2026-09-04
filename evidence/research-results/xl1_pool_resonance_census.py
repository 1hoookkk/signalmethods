import glob, os, re, sys, wave
from collections import defaultdict
import numpy as np

K = r"C:\Users\hooki\Downloads\42531\Digital.Sound.Factory.E-MU.Xtreme.Lead-1.KONTAKT-KRock\Digital Sound Factory - E-MU Xtreme Lead-1\Xtreme Lead\Xtreme Lead Samples"
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
ORDER = 30
GRID = np.geomspace(40, 12000, 240)


def note_hz(token):
    m = re.fullmatch(r"([A-G]#?)(-?\d)", token)
    if not m:
        return None
    return 440.0 * 2 ** ((12 * (int(m.group(2)) + 1) + NOTE[m.group(1)] - 69) / 12)


def read(path):
    try:
        import soundfile as sf
        x, fs = sf.read(path, always_2d=True)
        return x.mean(axis=1).astype(np.float64), fs
    except ImportError:
        w = wave.open(path, "rb")
        n, ch, sw, fs = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n); w.close()
        x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768
        return x.reshape(-1, ch).mean(axis=1), fs


def lpc(x, order, frame=8192):
    x = x - x.mean(); x = x / (np.max(np.abs(x)) + 1e-12)
    frame = min(frame, len(x)); hop = max(1, frame // 2); w = np.hanning(frame)
    r = np.zeros(order + 1); count = 0
    for s in range(0, max(1, len(x) - frame + 1), hop):
        seg = x[s:s + frame] * w
        r += np.fft.irfft(np.abs(np.fft.rfft(seg, 2 * frame)) ** 2)[:order + 1]; count += 1
    r /= max(count, 1); r[0] *= 1 + 1e-6
    a = np.zeros(order + 1); a[0] = 1; e = r[0]
    for i in range(1, order + 1):
        k = -(r[i] + np.dot(a[1:i], r[i - 1:0:-1])) / e
        an = a.copy(); an[1:i] = a[1:i] + k * a[i - 1:0:-1]; an[i] = k; a = an; e *= 1 - k * k
    return a, np.sqrt(max(e, 1e-12))


def env_db(a, g, fs, hz):
    hz = np.asarray(hz, dtype=float)[:, None]
    return 20 * np.log10(g / np.abs(np.sum(a * np.exp(-1j * 2 * np.pi * hz / fs * np.arange(len(a))), axis=1)))


def peak_of(curve, axis):
    best = None
    for i in range(3, len(curve) - 3):
        if not (curve[i] >= curve[i - 1] and curve[i] > curve[i + 1]):
            continue
        lo = i
        while lo > 0 and curve[lo - 1] < curve[lo]:
            lo -= 1
        hi = i
        while hi < len(curve) - 1 and curve[hi + 1] < curve[hi]:
            hi += 1
        prominence = curve[i] - max(curve[lo], curve[hi])
        if best is None or prominence > best[0]:
            half = curve[i] - 3.0
            l = i
            while l > lo and curve[l] > half:
                l -= 1
            r = i
            while r < hi and curve[r] > half:
                r += 1
            bw = axis[r] - axis[l] if r > l else axis[i] * 0.05
            best = (prominence, axis[i], axis[i] / max(bw, 1e-6))
    if best is None:
        return axis[int(np.argmax(curve))], 0.0, 1.0
    return best[1], best[0], best[2]


families = defaultdict(list)
for path in sorted(glob.glob(os.path.join(K, "**", "*.wav"), recursive=True)):
    name = os.path.splitext(os.path.basename(path))[0]
    parts = name.split(); f0 = note_hz(parts[-1]) if parts else None
    if f0 is None or f0 < 40 or f0 > 3000:
        continue
    families[" ".join(parts[:-1])].append((f0, path))

print(f"{'family':<20} n   fixed-score  tracked-score  verdict          peak (fixed Hz | tracked ratio)   Q   height")
rows = []
for family, items in sorted(families.items()):
    seen = {}
    for f0, path in items:
        seen.setdefault(os.path.basename(path), (f0, path))
    items = sorted(seen.values())
    if len(items) < 3:
        continue
    envs_hz, envs_ratio = [], []
    for f0, path in items:
        x, fs = read(path)
        if len(x) < 64:
            continue
        y = np.tile(x, max(1, int(np.ceil(1.5 * fs / len(x)))))
        a, g = lpc(y, ORDER)
        grid_hz = GRID[GRID < 0.45 * fs]
        e = env_db(a, g, fs, grid_hz); e = e - np.median(e)
        envs_hz.append(np.interp(GRID, grid_hz, e, left=np.nan, right=np.nan))
        ratio_axis = grid_hz / f0
        common_ratio = np.geomspace(0.5, 64, 240)
        envs_ratio.append(np.interp(common_ratio, ratio_axis, e, left=np.nan, right=np.nan))
    if len(envs_hz) < 3:
        continue
    H = np.array(envs_hz); R = np.array(envs_ratio)

    def score(M):
        mean = np.nanmean(M, axis=0)
        valid = np.sum(~np.isnan(M), axis=0) >= 3
        if valid.sum() < 20:
            return -1.0, mean, valid
        dev = np.nanmean(np.abs(M[:, valid] - mean[valid]))
        return -dev, mean, valid

    fs_, mean_h, valid_h = score(H)
    ts_, mean_r, valid_r = score(R)
    if fs_ == -1.0 and ts_ == -1.0:
        continue
    tracked = ts_ > fs_ + 0.5
    if tracked:
        axis = np.geomspace(0.5, 64, 240)[valid_r]; curve = mean_r[valid_r]
    else:
        axis = GRID[valid_h]; curve = mean_h[valid_h]
    peak, height, q = peak_of(curve, axis)
    verdict = "KEY-TRACKED" if tracked else "FIXED"
    if height < 8:
        verdict = "flat / no resonance"
    label = f"{peak:7.2f}x" if tracked else f"{peak:7.0f} Hz"
    rows.append((height, family, len(envs_hz), -fs_, -ts_, verdict, label, q))
    print(f"{family:<20} {len(envs_hz):<3} {-fs_:11.2f}  {-ts_:13.2f}  {verdict:<18} {label:<32} {q:5.1f}  {height:+5.1f} dB", flush=True)
print()
print("STRONGEST RESONANCES (height >= 12 dB), by height:")
for height, family, n, fsc, tsc, verdict, label, q in sorted(rows, reverse=True):
    if height >= 12:
        print(f"  {family:<20} {verdict:<12} {label:<12} Q {q:5.1f}  {height:+5.1f} dB  (n={n}, spread fixed {fsc:.1f} / tracked {tsc:.1f} dB)")
