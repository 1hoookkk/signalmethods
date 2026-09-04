import os, wave
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
INK = "#1d2126"; MUTE = "#8d959f"; GRIDC = "#dde1e6"; BLUE = "#2F6FDB"; ORANGE = "#D9782D"; GREY = "#c6ccd3"
FILES = [("303 sweep A", os.path.join(ROOT, "recipes", "recordings", "303-one-note-sweep-A.wav")),
         ("303 sweep C", os.path.join(ROOT, "recipes", "recordings", "303-one-note-sweep-C.wav")),
         ("303 sweep E", os.path.join(ROOT, "recipes", "recordings", "303-one-note-sweep-E.wav"))]


def read(path):
    try:
        import soundfile as sf
        x, fs = sf.read(path, always_2d=True); return x.mean(axis=1).astype(np.float64), fs
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


def lpc(x, order, frame=4096):
    x = x - x.mean(); x = x / (np.max(np.abs(x)) + 1e-12); frame = min(frame, len(x)); hop = max(1, frame // 2); w = np.hanning(frame)
    r = np.zeros(order + 1); count = 0
    for s in range(0, max(1, len(x) - frame + 1), hop):
        seg = x[s:s + frame] * w; r += np.fft.irfft(np.abs(np.fft.rfft(seg, 2 * frame)) ** 2)[:order + 1]; count += 1
    r /= max(count, 1); r[0] *= 1 + 1e-6; a = np.zeros(order + 1); a[0] = 1; e = r[0]
    for i in range(1, order + 1):
        k = -(r[i] + np.dot(a[1:i], r[i - 1:0:-1])) / e; an = a.copy(); an[1:i] = a[1:i] + k * a[i - 1:0:-1]; an[i] = k; a = an; e *= 1 - k * k
    return a, np.sqrt(max(e, 1e-12))


def env_db(a, g, fs, hz):
    hz = np.asarray(hz, float)[:, None]; return 20 * np.log10(g / np.abs(np.sum(a * np.exp(-1j * 2 * np.pi * hz / fs * np.arange(len(a))), axis=1)))


def bells(a, g, fs, count, grid):
    e = env_db(a, g, fs, grid); med = np.median(e); roots = np.roots(a); roots = roots[(roots.imag > 0) & (np.abs(roots) < 1)]
    out = []
    for z in roots:
        f = np.angle(z) * fs / (2 * np.pi); bw = -np.log(np.abs(z)) * fs / np.pi
        if f < 30 or f > 0.45 * fs or bw > f: continue
        p = env_db(a, g, fs, [f])[0] - med
        if p >= 6: out.append((p, f, bw))
    out.sort(reverse=True); return sorted(out[:count], key=lambda t: t[1])


def section_db(fs, fp, bwp, fz, bwz, hz):
    w = 2 * np.pi * hz / fs; rp = np.exp(-np.pi * bwp / fs); tp = 2 * np.pi * fp / fs; rz = np.exp(-np.pi * bwz / fs); tz = 2 * np.pi * fz / fs
    num = np.abs(1 - 2 * rz * np.cos(tz) * np.exp(-1j * w) + rz * rz * np.exp(-2j * w)); den = np.abs(1 - 2 * rp * np.cos(tp) * np.exp(-1j * w) + rp * rp * np.exp(-2j * w))
    dc = (1 - 2 * rp * np.cos(tp) + rp * rp) / (1 - 2 * rz * np.cos(tz) + rz * rz); return 20 * np.log10(num / den * abs(dc))


def frame_db(res, fs, hz):
    t = np.zeros_like(hz)
    for p, f, bw in res:
        bw = max(1.0, min(bw, f / 10)); t += section_db(fs, f, bw, f, bw * 10 ** (min(40, max(3, p)) / 20), hz)
    return t


grid = np.geomspace(40, 16000, 500)
fig, axes = plt.subplots(len(FILES), 2, figsize=(13, 3.2 * len(FILES)), dpi=100, gridspec_kw={"width_ratios": [1.6, 1]})
fig.patch.set_facecolor("white")
lines = []
for row, (name, path) in enumerate(FILES):
    x, fs = read(path); x = x - x.mean()
    n = len(x); hop = int(0.05 * fs); frame = 4096
    times, peaks = [], []
    for s in range(0, n - frame, hop):
        a, g = lpc(x[s:s + frame], 20); r = bells(a, g, fs, 1, grid)
        times.append(s / fs); peaks.append(r[0][1] if r else np.nan)
    ax = axes[row, 0]
    ax.plot(times, peaks, color=INK, lw=1.6)
    ax.set_yscale("log"); ax.set_ylim(80, 8000); ax.set_yticks([100, 200, 400, 800, 1600, 3200, 6400]); ax.set_yticklabels(["100", "200", "400", "800", "1.6k", "3.2k", "6.4k"], fontsize=8, color=MUTE)
    ax.tick_params(axis="x", labelsize=8, colors=MUTE); ax.grid(True, color=GRIDC, lw=0.5); ax.minorticks_off()
    for sp in ax.spines.values(): sp.set_color(GRIDC)
    ax.set_title(f"{name}: strongest bell over time (the cutoff sweep), {fs} Hz, {n / fs:.1f} s", fontsize=9, color=INK, loc="left")
    ax.set_xlabel("seconds", fontsize=8, color=MUTE)
    valid = [(t, p) for t, p in zip(times, peaks) if np.isfinite(p)]
    lo_t = valid[0][0] if valid else 0.0
    hi_i = int(np.nanargmax(peaks)); hi_t = times[hi_i]
    keys = []
    for label, colour, t0 in (("LO keyframe", BLUE, lo_t), ("HI keyframe", ORANGE, hi_t)):
        seg = x[int(t0 * fs): int(t0 * fs) + int(0.3 * fs)]
        a, g = lpc(seg, 30); res = bells(a, g, fs, 6, grid); keys.append((label, colour, t0, res, a, g))
        ax.axvline(t0, color=colour, lw=1.2, ls="--")
    ax2 = axes[row, 1]
    for label, colour, t0, res, a, g in keys:
        e = env_db(a, g, fs, grid); e -= np.median(e)
        ax2.plot(grid, e, color=colour, lw=0.7, alpha=0.4)
        ax2.plot(grid, frame_db(res, fs, grid), color=colour, lw=2.0, label=f"{label} at {t0:.2f} s")
        for p, f, bw in res:
            ax2.plot(f, min(40, max(3, p)), "o", ms=5, mfc="white", mec=colour, mew=1.3)
        lines.append(f"{name} {label} t={t0:.2f}s: " + "  ".join(f"{f:.0f}/{bw:.0f}({p:+.0f})" for p, f, bw in res))
    ax2.set_xscale("log"); ax2.set_xlim(40, 16000); ax2.set_ylim(-20, 40); ax2.axhline(0, color=INK, lw=0.9)
    ax2.set_xticks([80, 160, 320, 640, 1300, 2600, 5100, 10000]); ax2.set_xticklabels(["80", "160", "320", "640", "1.3k", "2.6k", "5.1k", "10k"], fontsize=8, color=MUTE)
    ax2.set_yticks([-20, 0, 20, 40]); ax2.tick_params(axis="y", labelsize=8, colors=MUTE); ax2.grid(True, color=GRIDC, lw=0.5); ax2.minorticks_off()
    for sp in ax2.spines.values(): sp.set_color(GRIDC)
    ax2.legend(fontsize=8, frameon=False, loc="upper right"); ax2.set_title("two keyframes as six bells", fontsize=9, color=INK, loc="left")
fig.suptitle("A real 303 as a MORPH body: the strongest bell tracked through each recorded sweep (left), and the closed and open moments as two six-bell keyframes (right).", fontsize=9, color=INK, x=0.01, ha="left")
fig.text(0.01, 0.004, "Sources: recipes/recordings/303-one-note-sweep-{A,C,E}.wav. LPC-20 per 4096-sample frame for the track; LPC-30 on 0.3 s at each keyframe.", fontsize=7, color=MUTE)
fig.tight_layout(rect=(0, 0.01, 1, 0.97))
out = os.path.join(HERE, "tb303_sweep_keyframes.png"); fig.savefig(out, facecolor="white")
open(os.path.join(HERE, "tb303_sweep_keyframes.txt"), "w", encoding="utf-8").write("\n".join(lines) + "\n"); print(out)
