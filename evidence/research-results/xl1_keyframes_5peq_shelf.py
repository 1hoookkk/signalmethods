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
        if hz < 30 or hz > 0.45 * fs:
            continue
        out.append((env_db(a, g, fs, [hz])[0] - med, hz, bw))
    out.sort(reverse=True)
    return sorted(out[:count], key=lambda t: t[1])


def section_db(fs, fp, bwp, fz, bwz, hz):
    w = 2 * np.pi * hz / fs
    rp = np.exp(-np.pi * bwp / fs); tp = 2 * np.pi * fp / fs
    rz = 1.0 if bwz == 0 else np.exp(-np.pi * bwz / fs); tz = 2 * np.pi * fz / fs
    num = np.abs(1 - 2 * rz * np.cos(tz) * np.exp(-1j * w) + rz * rz * np.exp(-2j * w))
    den = np.abs(1 - 2 * rp * np.cos(tp) * np.exp(-1j * w) + rp * rp * np.exp(-2j * w))
    dc = (1 - 2 * rp * np.cos(tp) + rp * rp) / (1 - 2 * rz * np.cos(tz) + rz * rz)
    return 20 * np.log10(num / den * abs(dc))


def shelf_db(fs, tilt_db, hz):
    w = 2 * np.pi * hz / fs
    rp = 0.985
    ratio = 10 ** (-tilt_db / 40.0)
    rz = 1 - np.clip((1 - rp) * ratio, 0.002, 0.6)
    num = np.abs(1 - rz * np.exp(-1j * w)); den = np.abs(1 - rp * np.exp(-1j * w))
    hf = (1 + rz) / (1 + rp)
    return 2 * 20 * np.log10(num / den / hf)


def six_peq_db(res, fs, hz, tilt_db):
    total = np.zeros_like(hz, dtype=float)
    for p, fp, bwp in res[:5]:
        bwp = max(1.0, bwp)
        gain = min(24.0, max(3.0, p))
        total += section_db(fs, fp, bwp, fp, bwp * 10 ** (gain / 20), hz)
    total += shelf_db(fs, tilt_db, hz)
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
    notes = sorted(pool[family].items(), key=lambda kv: kv[1][0])
    ramp = [RAMP[int(round(i * (len(RAMP) - 1) / max(1, len(notes) - 1)))] for i in range(len(notes))]
    for (note, (f0, path)), colour in zip(notes, ramp):
        x, fs = read(path)
        if len(x) < 64:
            continue
        y = np.tile(x, max(1, int(np.ceil(1.5 * fs / len(x)))))
        a, g = lpc(y, ORDER)
        grid = GRID[GRID < 0.45 * fs]
        e = env_db(a, g, fs, grid); e = e - np.median(e)
        ax.plot(grid, e, color=colour, lw=0.7, alpha=0.35)
        res = resonances(a, g, fs, 5, grid)
        lo = np.mean(e[(grid > 60) & (grid < 150)]); hi = np.mean(e[(grid > 5000) & (grid < 9000)])
        body = six_peq_db(res, fs, grid, hi - lo)
        ax.plot(grid, body, color=colour, lw=1.8, label=f"{note} · {f0:.0f} Hz")
        for p, fp, bwp in res:
            ax.plot(fp, min(24.0, max(3.0, p)), "o", ms=4, mfc="white", mec=colour, mew=1.2)
    ax.set_xscale("log"); ax.set_xlim(20, 16000); ax.set_ylim(-40, 40)
    ax.axhline(0, color=INK, lw=1.0)
    ax.set_xticks([20, 40, 80, 160, 320, 640, 1300, 2600, 5100, 10000])
    ax.set_xticklabels(["20", "40", "80", "160", "320", "640", "1.3k", "2.6k", "5.1k", "10k"], fontsize=8, color=MUTE)
    ax.set_yticks([-40, -20, 0, 20, 40]); ax.tick_params(axis="y", labelsize=8, colors=MUTE)
    ax.grid(True, which="major", color=GRIDC, lw=0.6); ax.minorticks_off()
    for s in ax.spines.values():
        s.set_color(GRIDC)
    ax.set_title(f"{family}  ·  {len(notes)} notes", fontsize=10, color=INK, loc="left")
    ax.legend(fontsize=7, frameon=False, loc="lower left", ncol=2)
fig.suptitle("XL-1 keyframes as five PEQ plus a stage-6 SHELF: five strongest resonances as EQ bells, stage 6 a two-pole shelf carrying the note's measured tilt (60-150 Hz against 5-9 kHz), "
             "the body TRENCH would hold (bold); the full 15-pair LPC model behind it (faint). Faint = the full 15-pair model.",
             fontsize=10, color=INK, x=0.01, ha="left")
fig.text(0.01, 0.005, "Hz axis 20 to 16 k, octave grid, hard 0 dB line, each curve relative to its own median. Source: Digital Sound Factory XL-1 Kontakt samples (Downloads).",
         fontsize=8, color=MUTE)
fig.tight_layout(rect=(0, 0.02, 1, 0.96))
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "xl1_keyframes_5peq_shelf.png")
fig.savefig(out, facecolor="white"); print(out)
