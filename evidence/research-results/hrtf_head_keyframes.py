import os
import numpy as np, h5py
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
SOFA = r"C:\Users\hooki\trench-authoring\recipes\holy_sources\hrtf\sonicom\P0001\HRTF\HRTF\48kHz\P0001_FreeFieldComp_48kHz.sofa"
INK = "#1d2126"; MUTE = "#8d959f"; GRIDC = "#dde1e6"; BLUE = "#2F6FDB"; GREY = "#9aa3ad"
VIEWS = [((0, 0), "front"), ((90, 0), "left side"), ((180, 0), "behind"), ((270, 0), "right side"), ((0, 45), "front, 45 up"), ((0, -30), "front, 30 down")]


def lpc(x, order):
    x = x - x.mean(); x = x / (np.max(np.abs(x)) + 1e-12)
    r = np.fft.irfft(np.abs(np.fft.rfft(x, 4 * len(x))) ** 2)[:order + 1]; r[0] *= 1 + 1e-6
    a = np.zeros(order + 1); a[0] = 1; e = r[0]
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


f = h5py.File(SOFA, "r"); ir = f["Data.IR"][:]; sp = f["SourcePosition"][:]; fs = float(f["Data.SamplingRate"][0])
grid = np.geomspace(200, 20000, 500)
fig, axes = plt.subplots(2, 3, figsize=(15, 7.5), dpi=105); fig.patch.set_facecolor("white")
lines = []
for ax, ((az, el), label) in zip(axes.flat, VIEWS):
    d = np.hypot(((sp[:, 0] - az + 180) % 360) - 180, sp[:, 1] - el); i = int(np.argmin(d))
    h = ir[i, 0, :]
    H = np.abs(np.fft.rfft(h, 8192)); hz = np.arange(len(H)) * fs / 8192
    meas = 20 * np.log10(H + 1e-9); keep = (hz > 200) & (hz < 20000); meas -= np.median(meas[keep])
    a, g = lpc(h, 30); res = bells(a, g, fs, 6, grid)
    e = env_db(a, g, fs, grid); e -= np.median(e)
    ax.plot(hz[keep], meas[keep], color=GREY, lw=1.0)
    ax.plot(grid, e, color=BLUE, lw=0.7, alpha=0.35)
    ax.plot(grid, frame_db(res, fs, grid), color=BLUE, lw=2.2)
    for p, fr, bw in res:
        ax.plot(fr, min(40, max(3, p)), "o", ms=6, mfc="white", mec=BLUE, mew=1.5)
        ax.annotate(f"{fr:.0f}", (fr, min(40, max(3, p))), textcoords="offset points", xytext=(0, 8), ha="center", fontsize=8, color=INK)
    ax.set_xscale("log"); ax.set_xlim(200, 20000); ax.set_ylim(-30, 40); ax.axhline(0, color=INK, lw=0.9)
    ax.set_xticks([320, 640, 1300, 2600, 5100, 10000, 20000]); ax.set_xticklabels(["320", "640", "1.3k", "2.6k", "5.1k", "10k", "20k"], fontsize=8, color=MUTE)
    ax.set_yticks([-20, 0, 20, 40]); ax.tick_params(axis="y", labelsize=8, colors=MUTE); ax.grid(True, color=GRIDC, lw=0.5); ax.minorticks_off()
    for s in ax.spines.values(): s.set_color(GRIDC)
    ax.set_title(f"{label}  ·  az {az} el {el}  ·  left ear", fontsize=10, color=INK, loc="left")
    lines.append(f"{label:<16} az {az:>3} el {el:>3}: " + "  ".join(f"{fr:.0f}/{bw:.0f}({p:+.0f})" for p, fr, bw in res))
fig.suptitle("A head as keyframes: SONICOM subject P0001, left-ear HRTF from six directions (grey), each read as six bells (blue). MORPH between two of these = turning the head.",
             fontsize=10, color=INK, x=0.01, ha="left")
fig.text(0.01, 0.004, "Source: recipes/holy_sources/hrtf/sonicom/P0001 (48 kHz, 256-sample free-field compensated HRIRs). LPC-30 on the impulse response; rules: true bells, >= 6 dB, seed Q >= 10, gain <= 40.", fontsize=8, color=MUTE)
fig.tight_layout(rect=(0, 0.02, 1, 0.96))
out = os.path.join(HERE, "hrtf_head_keyframes.png"); fig.savefig(out, facecolor="white")
open(os.path.join(HERE, "hrtf_head_keyframes.txt"), "w", encoding="utf-8").write("\n".join(lines) + "\n"); print(out)
