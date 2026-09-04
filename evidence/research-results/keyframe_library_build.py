import glob, json, os, re, wave
from collections import Counter
import numpy as np, h5py

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "native", "app", "templates", "keyframes_from_audio.json")
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}


def note_hz(t):
    m = re.fullmatch(r"([A-G]#?)(-?\d)", t)
    return None if not m else 440.0 * 2 ** ((12 * (int(m.group(2)) + 1) + NOTE[m.group(1)] - 69) / 12)


def read(path):
    try:
        import soundfile as sf
        x, fs = sf.read(path, always_2d=True)
        return x.mean(axis=1).astype(np.float64), fs
    except ImportError:
        w = wave.open(path, "rb"); n, ch, sw, fs = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n); w.close()
        if sw == 2:
            x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768
        elif sw == 3:
            b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
            x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16))
            x = np.where(x >= 1 << 23, x - (1 << 24), x).astype(np.float64) / (1 << 23)
        else:
            x = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648
        return x.reshape(-1, ch).mean(axis=1), fs


def resample(x, fs, to):
    if fs == to:
        return x
    cutoff = 0.9 * to / fs; taps = 127; n = np.arange(taps) - (taps - 1) / 2
    h = np.sinc(cutoff * n) * cutoff * np.hanning(taps); x = np.convolve(x, h, mode="same")
    return np.interp(np.arange(0, len(x) / fs, 1 / to), np.arange(len(x)) / fs, x)


def levinson(r, order):
    r = r.copy(); r[0] *= 1 + 1e-6; a = np.zeros(order + 1); a[0] = 1; e = r[0]
    for i in range(1, order + 1):
        k = -(r[i] + np.dot(a[1:i], r[i - 1:0:-1])) / e
        an = a.copy(); an[1:i] = a[1:i] + k * a[i - 1:0:-1]; an[i] = k; a = an; e *= 1 - k * k
    return a, np.sqrt(max(e, 1e-12))


def lpc(x, order, frame=8192):
    x = x - x.mean(); x = x / (np.max(np.abs(x)) + 1e-12)
    frame = min(frame, len(x)); hop = max(1, frame // 2); w = np.hanning(frame); r = np.zeros(order + 1); c = 0
    for s in range(0, max(1, len(x) - frame + 1), hop):
        r += np.fft.irfft(np.abs(np.fft.rfft(x[s:s + frame] * w, 2 * frame)) ** 2)[:order + 1]; c += 1
    return levinson(r / max(c, 1), order)


def env_db(a, g, fs, hz):
    hz = np.asarray(hz, float)[:, None]
    return 20 * np.log10(g / np.abs(np.sum(a * np.exp(-1j * 2 * np.pi * hz / fs * np.arange(len(a))), axis=1)))


def roots_of(a, g, fs, lo, hi):
    grid = np.geomspace(max(20, lo), hi, 256); med = np.median(env_db(a, g, fs, grid)); out = []
    for z in np.roots(a):
        if z.imag <= 0 or abs(z) >= 1:
            continue
        f = np.angle(z) * fs / (2 * np.pi); bw = -np.log(abs(z)) * fs / np.pi
        if f < lo or f > hi:
            continue
        out.append((env_db(a, g, fs, [f])[0] - med, f, bw))
    return out


def bells_rows(cands):
    cands = [t for t in cands if t[2] <= t[1] and t[0] >= 6.0]
    cands.sort(reverse=True); cands = sorted(cands[:6], key=lambda t: t[1])
    return [{"type": "EQ", "hz": round(f, 1), "bw_hz": round(max(1.0, min(bw, f / 10)), 1), "gain_db": round(min(40.0, max(3.0, p)), 1)} for p, f, bw in cands]


def pole_rows(cands):
    cands.sort(reverse=True); cands = sorted(cands[:6], key=lambda t: t[1])
    return [{"type": "POLE", "hz": round(f, 1), "bw_hz": round(max(1.0, bw), 1), "gain_db": 0.0} for p, f, bw in cands]


def six_bells(x, fs):
    a, g = lpc(x, 30); return bells_rows(roots_of(a, g, fs, 30, 0.45 * fs))


def speech_poles(x, fs):
    y = resample(x, fs, 11025.0); a, g = lpc(y, 12); return pole_rows(roots_of(a, g, 11025.0, 60, 0.45 * 11025))


def poles_from_tf(hz, mag):
    fs = 11025.0; grid = np.arange(0, fs / 2, fs / 16384)
    power = np.interp(grid, hz, mag, left=mag[0], right=mag[-1]) ** 2
    full = np.concatenate([power, power[-2:0:-1]]); r = np.fft.ifft(full).real[:13]; a, g = levinson(r, 12)
    return pole_rows(roots_of(a, g, fs, 60, 0.45 * fs))


frames = []


def add(group, name, source, mode, rows):
    if rows:
        frames.append({"group": group, "name": name, "source": source, "mode": mode, "rows": rows})


INSTRUMENTS = [("sung ah", "recipes/recordings/test-vowel-ah.wav"), ("cello", "recipes/08_clean_instruments/uiowa_cello_mf.wav"),
               ("bassoon", "recipes/08_clean_instruments/uiowa_bassoon_mf.wav"), ("alto sax", "recipes/08_clean_instruments/uiowa_alto_sax_mf.wav"),
               ("trombone", "recipes/08_clean_instruments/uiowa_tenor_trombone_mf.wav"), ("bass clarinet", "recipes/08_clean_instruments/uiowa_bass_clarinet_mf.wav"),
               ("double bass", "recipes/08_clean_instruments/uiowa_double_bass_mf.wav"), ("tuba", "recipes/08_clean_instruments/uiowa_tuba_mf.wav")]
for label, rel in INSTRUMENTS:
    x, fs = read(os.path.join(ROOT, rel)); x = x[:int(fs * 4)]
    add("INSTRUMENT", label, rel, "six bells", six_bells(x, fs))
    if label == "sung ah":
        add("VOWEL", "sung ah (speech)", rel, "speech", speech_poles(x, fs))

for path in sorted(glob.glob(os.path.join(ROOT, "evidence", "measured-bodies", "ir_library", "*", "*.wav"))):
    x, fs = read(path)
    if len(x) < 2048:
        continue
    add("BODY", os.path.splitext(os.path.basename(path))[0], os.path.relpath(path, ROOT), "six bells", six_bells(x[:int(fs)], fs))

pool = {}
for path in sorted(glob.glob(os.path.join(ROOT, "evidence", "factory-data", "xl1-dsf-aud", "Aud *.wav"))):
    parts = os.path.splitext(os.path.basename(path))[0].split(); f0 = note_hz(parts[-1])
    if f0:
        pool.setdefault(" ".join(parts[:-1]), []).append((abs(np.log2(f0 / 196.0)), parts[-1], path))
for family, items in sorted(pool.items()):
    _, note, path = sorted(items)[0]; x, fs = read(path); y = np.tile(x, max(1, int(np.ceil(1.5 * fs / len(x)))))
    add("XL-1 AUD", family + " " + note, os.path.relpath(path, ROOT), "six bells", six_bells(y, fs))

for path in sorted(glob.glob(os.path.join(ROOT, "recipes", "vocal", "dvtd", "subject-*", "*", "*-vvtf-measured.txt"))):
    name = os.path.basename(os.path.dirname(path)); m = re.match(r"(s\d)-\d+-([a-z]+)-((?:tense|lax)-[a-z]+)$", name)
    if not m:
        continue
    d = np.loadtxt(path, skiprows=1)
    add("VOWEL DVTD", m.group(3) + " " + m.group(2) + " " + m.group(1), os.path.relpath(path, ROOT), "poles", poles_from_tf(d[:, 0], d[:, 1]))

sofa = os.path.join(ROOT, "evidence", "measured-bodies", "hrtf", "P0001_FreeFieldComp_48kHz.sofa")
f = h5py.File(sofa, "r"); ir = f["Data.IR"][:]; sp = f["SourcePosition"][:]; fs = float(f["Data.SamplingRate"][0])
for az in range(0, 360, 30):
    for el in (-30, 0, 30, 60):
        d = np.hypot(((sp[:, 0] - az + 180) % 360) - 180, sp[:, 1] - el); i = int(np.argmin(d))
        h = ir[i, 0, :]; r = np.fft.irfft(np.abs(np.fft.rfft(h - h.mean(), 4 * len(h))) ** 2)[:31]; a, g = levinson(r, 30)
        add("HEAD", "left ear az %d el %d" % (az, el), os.path.relpath(sofa, ROOT) + " index %d" % i, "six bells", bells_rows(roots_of(a, g, fs, 200, 0.45 * fs)))


VOWEL_ORDER = ["i", "I", "e", "E", "{", "A", "O", "o", "U", "u", "V", "3'"]
VOWEL_NAMES = {"i": "ee heed", "I": "ih hid", "e": "ay hayed", "E": "eh head", "{": "ae had", "A": "ah hod", "O": "aw hawed",
               "o": "oh hoed", "U": "uh hood", "u": "oo who'd", "V": "uh hud", "3'": "er heard"}
SPEAKERS = [("man", "m"), ("woman", "w"), ("boy", "b"), ("girl", "g")]
h95 = os.path.join(ROOT, "evidence", "factory-data", "hillenbrand-1995", "h95.csv")
rows_h95 = [r.split(",") for r in open(h95, encoding="utf-8").read().splitlines()[1:]]
for v in VOWEL_ORDER:
    for label, code in SPEAKERS:
        sel = np.array([[float(r[4]), float(r[5]), float(r[6]), float(r[7])] for r in rows_h95 if r[0] == code and r[2] == v])
        f0, f1, f2, f3 = np.median(sel, axis=0)
        rows = [{"type": "POLE", "hz": round(float(f), 1), "bw_hz": round(50.0 + float(f) / 20.0, 1), "gain_db": 0.0} for f in (f1, f2, f3)]
        add("VOWEL H95", "%s %s" % (VOWEL_NAMES[v], label), "evidence/factory-data/hillenbrand-1995/h95.csv medians, %d tokens, f0 %d Hz" % (len(sel), round(f0)), "poles", rows)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump({"schema": "trench-keyframes-v1", "datum_hz": 44100.0,
           "rows_are": "TYPE/hz/bw_hz/gain_db in the Morph Designer grammar; EQ = zero on the pole with bw x 10^(gain/20); POLE = bare pole; compile with applyTypeRows",
           "frames": frames}, open(OUT, "w", encoding="utf-8"), indent=1)
print(OUT); print(Counter(fr["group"] for fr in frames)); print("total", len(frames))
