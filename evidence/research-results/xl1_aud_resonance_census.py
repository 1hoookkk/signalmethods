import glob, os, re, wave
from collections import defaultdict
import numpy as np

K = r"C:\Users\hooki\Downloads\42531\Digital.Sound.Factory.E-MU.Xtreme.Lead-1.KONTAKT-KRock\Digital Sound Factory - E-MU Xtreme Lead-1\Xtreme Lead\Xtreme Lead Samples"
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}


def note_hz(token):
    m = re.fullmatch(r"([A-G]#?)(-?\d)", token)
    if not m:
        return None
    midi = 12 * (int(m.group(2)) + 1) + NOTE[m.group(1)]
    return 440.0 * 2 ** ((midi - 69) / 12)


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


def env(a, g, fs, hz):
    hz = np.asarray(hz, dtype=float)[:, None]
    return 20 * np.log10(g / np.abs(np.sum(a * np.exp(-1j * 2 * np.pi * hz / fs * np.arange(len(a))), axis=1)))


def top_resonance(a, g, fs):
    grid = np.geomspace(20, 0.45 * fs, 256); med = np.median(env(a, g, fs, grid))
    roots = np.roots(a); roots = roots[(roots.imag > 0) & (np.abs(roots) < 1)]
    best = None
    for z in roots:
        hz = np.angle(z) * fs / (2 * np.pi); bw = -np.log(np.abs(z)) * fs / np.pi
        if hz < 30 or hz > 0.45 * fs:
            continue
        p = env(a, g, fs, [hz])[0] - med
        if best is None or p > best[0]:
            best = (p, hz, bw)
    return best


rows = defaultdict(list)
for path in sorted(glob.glob(os.path.join(K, "**", "Aud *.wav"), recursive=True)):
    name = os.path.splitext(os.path.basename(path))[0]
    parts = name.split(); f0 = note_hz(parts[-1])
    if f0 is None:
        continue
    family = " ".join(parts[:-1])
    x, fs = read(path)
    y = np.tile(x, max(1, int(np.ceil(2.0 * fs / len(x)))))
    a, g = lpc(y, 30)
    best = top_resonance(a, g, fs)
    if best is None:
        continue
    p, hz, bw = best
    rows[family].append((parts[-1], f0, hz, bw, p, len(x) / fs * 1000))

print(f"{'family':<14} n  ratio hz/f0 per note (median, spread)     Q median   prominence   verdict")
for family in sorted(rows):
    r = sorted(rows[family], key=lambda t: t[1])
    ratios = np.array([hz / f0 for _, f0, hz, bw, p, _ in r]); hzs = np.array([hz for _, f0, hz, bw, p, _ in r])
    qs = np.array([hz / max(bw, 1) for _, f0, hz, bw, p, _ in r]); ps = np.array([p for *_, p, _ in r])
    ratio_spread = (ratios.max() / ratios.min()) if ratios.min() > 0 else np.inf
    hz_spread = hzs.max() / hzs.min()
    strong = np.median(ps) >= 12
    if not strong:
        verdict = "no strong resonance"
    elif ratio_spread < 1.25 and hz_spread > 1.8:
        verdict = "KEY-TRACKED resonance"
    elif hz_spread < 1.25:
        verdict = "FIXED resonance"
    else:
        verdict = "mixed"
    print(f"{family:<14} {len(r):<2} {np.median(ratios):5.2f}x (x{ratio_spread:4.2f})   fixed-hz spread x{hz_spread:5.2f}   Q {np.median(qs):5.1f}   {np.median(ps):+5.1f} dB   {verdict}")
    print("   " + "  ".join(f"{n}:{hz:.0f}Hz/{hz/f0:.1f}x Q{hz/max(bw,1):.0f} {p:+.0f}dB" for n, f0, hz, bw, p, _ in r))
