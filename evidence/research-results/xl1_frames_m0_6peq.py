import glob, os, re, wave
from collections import defaultdict
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

K = r"C:\Users\hooki\Downloads\42531\Digital.Sound.Factory.E-MU.Xtreme.Lead-1.KONTAKT-KRock\Digital Sound Factory - E-MU Xtreme Lead-1\Xtreme Lead\Xtreme Lead Samples"
FAMILIES = ["Aud Lead 2", "Aud Bell 4", "Sync 2", "Ring Mod 2", "Aud Bell 1", "Aud Blend", "Aud Synth 14", "Vapor Vox", "Aud Sync 1"]
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
ORDER = 30
GRID = np.geomspace(20, 16000, 400)
INK = "#1d2126"; MUTE = "#8d959f"; GRIDC = "#dde1e6"
RAMP = ["#c9dcf5", "#9cbfec", "#6f9fe0", "#4a80d0", "#2f63b8", "#204a92", "#15346b", "#0c2148"]


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


def resonances(a, g, fs, count, grid):
    e = env_db(a, g, fs, grid); med = np.median(e)
    roots = np.roots(a); roots = roots[(roots.imag > 0) & (np.abs(roots) < 1)]
    out = []
    for z in roots:
        hz = np.angle(z) * fs / (2 * np.pi); bw = -np.log(np.abs(z)) * fs / np.pi
        if hz < 30 or hz > 0.45 * fs or bw > hz:
            continue
        out.append((env_db(a, g, fs, [hz])[0] - med, hz, bw))
    out.sort(reverse=True)
    out = [t for t in out if t[0] >= 6.0]
    return sorted(out[:count], key=lambda t: t[1])


def section_db(fs, fp, bwp, fz, bwz, hz):
    w = 2 * np.pi * hz / fs
    rp = np.exp(-np.pi * bwp / fs); tp = 2 * np.pi * fp / fs
    rz = 1.0 if bwz == 0 else np.exp(-np.pi * bwz / fs); tz = 2 * np.pi * fz / fs
    num = np.abs(1 - 2 * rz * np.cos(tz) * np.exp(-1j * w) + rz * rz * np.exp(-2j * w))
    den = np.abs(1 - 2 * rp * np.cos(tp) * np.exp(-1j * w) + rp * rp * np.exp(-2j * w))
    dc = (1 - 2 * rp * np.cos(tp) + rp * rp) / (1 - 2 * rz * np.cos(tz) + rz * rz)
    return 20 * np.log10(num / den * abs(dc))


def six_peq_db(res, fs, hz):
    total = np.zeros_like(hz, dtype=float)
    for p, fp, bwp in res:
        bwp = max(1.0, min(bwp, fp / 10.0))
        gain = min(40.0, max(3.0, p))
        total += section_db(fs, fp, bwp, fp, bwp * 10 ** (gain / 20), hz)
    return total


def env_db(a, g, fs, hz):
    hz = np.asarray(hz, dtype=float)[:, None]
    return 20 * np.log10(g / np.abs(np.sum(a * np.exp(-1j * 2 * np.pi * hz / fs * np.arange(len(a))), axis=1)))


pool = defaultdict(dict)
for path in sorted(glob.glob(os.path.join(K, "**", "*.wav"), recursive=True)):
    name = os.path.splitext(os.path.basename(path))[0]
    parts = name.split(); f0 = note_hz(parts[-1]) if parts else None
    if f0 is None:
        continue
    pool[" ".join(parts[:-1])].setdefault(parts[-1], (f0, path))

fig, axes = plt.subplots(3, 3, figsize=(16, 11), dpi=110)
fig.patch.set_facecolor("white")
for ax, family in zip(axes.flat, FAMILIES):
    notes = sorted(pool[family].items(), key=lambda kv: abs(np.log2(kv[1][0] / 196.0)))[:1]
    ramp = ["#2F6FDB"]
    for (note, (f0, path)), colour in zip(notes, ramp):
        x, fs = read(path)
        if len(x) < 64:
            continue
        y = np.tile(x, max(1, int(np.ceil(1.5 * fs / len(x)))))
        a, g = lpc(y, ORDER)
        grid = GRID[GRID < 0.45 * fs]
        e = env_db(a, g, fs, grid); e = e - np.median(e)
        frame = 4096; w = np.hanning(frame); power = np.zeros(frame + 1); count = 0
        for st in range(0, len(y) - frame + 1, frame // 2):
            power += np.abs(np.fft.rfft(y[st:st + frame] * w, 2 * frame)) ** 2; count += 1
        power /= max(count, 1)
        spec_hz = np.arange(len(power)) * fs / (2 * frame)
        spec_db = 10 * np.log10(power + 1e-18); spec_db -= np.median(spec_db[(spec_hz > 20) & (spec_hz < 0.45 * fs)])
        keep = (spec_hz > 20) & (spec_hz < 16000)
        ax.plot(spec_hz[keep], spec_db[keep], color="#c6ccd3", lw=0.5, alpha=0.8)
        res = resonances(a, g, fs, 6, grid)
        body = six_peq_db(res, fs, grid)
        ax.plot(grid, body, color=colour, lw=2.4, label=f"{note} · {f0:.0f} Hz · six PEQ")
        for p, fp, bwp in res:
            ax.plot(fp, min(40.0, max(3.0, p)), "o", ms=6, mfc="white", mec=colour, mew=1.5)
            ax.annotate(f"{fp:.0f}", (fp, min(40.0, max(3.0, p))), textcoords="offset points", xytext=(0, 8), ha="center", fontsize=8, color=INK)
    ax.set_xscale("log"); ax.set_xlim(20, 16000); ax.set_ylim(-40, 40)
    ax.axhline(0, color=INK, lw=1.0)
    ax.set_xticks([20, 40, 80, 160, 320, 640, 1300, 2600, 5100, 10000])
    ax.set_xticklabels(["20", "40", "80", "160", "320", "640", "1.3k", "2.6k", "5.1k", "10k"], fontsize=8, color=MUTE)
    ax.set_yticks([-40, -20, 0, 20, 40]); ax.tick_params(axis="y", labelsize=8, colors=MUTE)
    ax.grid(True, which="major", color=GRIDC, lw=0.6); ax.minorticks_off()
    for s in ax.spines.values():
        s.set_color(GRIDC)
    ax.set_title(family, fontsize=11, color=INK, loc="left")
    ax.legend(fontsize=8, frameon=False, loc="lower left")
fig.suptitle("One frame each: the XL-1 note nearest G3 in nine families, reduced to six EQ bells on a flat floor; only true bells (Q of 1 or more) may become rows; seed Q at least 10, gain up to 40 dB, a bell needs 6 dB of height or the row stays OFF. Grey = the sample's spectrum. Numbers = the six notes in Hz.",
             fontsize=10, color=INK, x=0.01, ha="left")
fig.text(0.01, 0.005, "Hz axis 20 to 16 k, octave grid, hard 0 dB line, each curve relative to its own median. Source: Digital Sound Factory XL-1 Kontakt samples (Downloads).",
         fontsize=8, color=MUTE)
fig.tight_layout(rect=(0, 0.02, 1, 0.96))
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "xl1_frames_m0_6peq.png")
fig.savefig(out, facecolor="white"); print(out)
