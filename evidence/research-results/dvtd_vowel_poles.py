import glob, os, re
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "recipes", "vocal", "dvtd")
FS = 11025.0
ORDER = 12
INK = "#1d2126"; MUTE = "#8d959f"; GRIDC = "#dde1e6"; BLUE = "#2F6FDB"; GREY = "#9aa3ad"
VOWELS = ["tense-a", "tense-e", "tense-i", "tense-o", "tense-u", "tense-ae", "tense-oe", "tense-y",
          "lax-a", "lax-ae", "lax-i", "lax-o", "lax-u", "lax-y", "lax-oe"]


def load(path):
    d = np.loadtxt(path, skiprows=1)
    return d[:, 0], d[:, 1]


def lpc_from_power(hz, mag, order):
    grid = np.arange(0, FS / 2, FS / 16384)
    power = np.interp(grid, hz, mag, left=mag[0], right=mag[-1]) ** 2
    full = np.concatenate([power, power[-2:0:-1]])
    r = np.fft.ifft(full).real[:order + 1]
    r[0] *= 1 + 1e-6
    a = np.zeros(order + 1); a[0] = 1; e = r[0]
    for i in range(1, order + 1):
        k = -(r[i] + np.dot(a[1:i], r[i - 1:0:-1])) / e
        an = a.copy(); an[1:i] = a[1:i] + k * a[i - 1:0:-1]; an[i] = k; a = an; e *= 1 - k * k
    return a, np.sqrt(max(e, 1e-12))


def env_db(a, g, hz):
    hz = np.asarray(hz, dtype=float)[:, None]
    return 20 * np.log10(g / np.abs(np.sum(a * np.exp(-1j * 2 * np.pi * hz / FS * np.arange(len(a))), axis=1)))


def resonances(a, g, count, grid):
    e = env_db(a, g, grid); med = np.median(e)
    roots = np.roots(a); roots = roots[(roots.imag > 0) & (np.abs(roots) < 1)]
    out = []
    for z in roots:
        f = np.angle(z) * FS / (2 * np.pi); bw = -np.log(np.abs(z)) * FS / np.pi
        if f < 30 or f > 0.45 * FS or bw > f:
            continue
        out.append((env_db(a, g, [f])[0] - med, f, bw))
    out.sort(reverse=True)
    out = [t for t in out if t[0] >= 3.0]
    return sorted(out[:count], key=lambda t: t[1])


def section_db(fp, bwp, fz, bwz, hz):
    w = 2 * np.pi * hz / FS
    rp = np.exp(-np.pi * bwp / FS); tp = 2 * np.pi * fp / FS
    rz = np.exp(-np.pi * bwz / FS); tz = 2 * np.pi * fz / FS
    num = np.abs(1 - 2 * rz * np.cos(tz) * np.exp(-1j * w) + rz * rz * np.exp(-2j * w))
    den = np.abs(1 - 2 * rp * np.cos(tp) * np.exp(-1j * w) + rp * rp * np.exp(-2j * w))
    dc = (1 - 2 * rp * np.cos(tp) + rp * rp) / (1 - 2 * rz * np.cos(tz) + rz * rz)
    return 20 * np.log10(num / den * abs(dc))


def six_peq(res, hz):
    total = np.zeros_like(hz)
    for p, fp, bwp in res:
        bwp = max(1.0, min(bwp, fp / 10.0))
        gain = min(40.0, max(3.0, p))
        total += section_db(fp, bwp, fp, bwp * 10 ** (gain / 20), hz)
    return total


files = {}
for path in glob.glob(os.path.join(ROOT, "subject-*", "*", "*-vvtf-measured.txt")):
    name = os.path.basename(os.path.dirname(path))
    m = re.match(r"(s\d)-\d+-([a-z]+)-((?:tense|lax)-[a-z]+)$", name)
    if m:
        files[(m.group(1), m.group(3))] = (m.group(2), path)

grid = np.geomspace(60, 0.49 * FS, 500)
subjects = ["s1", "s2"]
fig, axes = plt.subplots(len(VOWELS), len(subjects), figsize=(11, 2.1 * len(VOWELS)), dpi=100)
fig.patch.set_facecolor("white")
report = []
for row, vowel in enumerate(VOWELS):
    for col, subject in enumerate(subjects):
        ax = axes[row, col]
        key = (subject, vowel)
        if key not in files:
            ax.set_visible(False); continue
        word, path = files[key]
        hz, mag = load(path)
        meas = 20 * np.log10(np.maximum(mag, 1e-6)); meas -= np.median(meas[(hz > 60) & (hz < 16000)])
        a, g = lpc_from_power(hz, mag, ORDER)
        body = env_db(a, g, grid); body -= np.median(body)
        roots = np.roots(a); roots = roots[(roots.imag > 0) & (np.abs(roots) < 1)]
        poles = sorted((np.angle(z) * FS / (2 * np.pi), -np.log(np.abs(z)) * FS / np.pi) for z in roots)
        keep = (hz > 60) & (hz < 16000)
        ax.plot(hz[keep], meas[keep], color=GREY, lw=0.9)
        ax.plot(grid, body, color=BLUE, lw=2.2)
        res = []
        for fp, bwp in poles:
            if fp < 60 or fp > 0.49 * FS:
                continue
            level = float(np.interp(fp, grid, body))
            ax.plot(fp, level, "o", ms=5, mfc="white", mec=BLUE, mew=1.4)
            ax.annotate(f"{fp:.0f}", (fp, level), textcoords="offset points", xytext=(0, 7), ha="center", fontsize=7, color=INK)
            res.append((level, fp, bwp))
        ax.set_xscale("log"); ax.set_xlim(60, 16000); ax.set_ylim(-30, 40)
        ax.axhline(0, color=INK, lw=0.9)
        ax.set_xticks([80, 160, 320, 640, 1300, 2600, 5100, 10000])
        ax.set_xticklabels(["80", "160", "320", "640", "1.3k", "2.6k", "5.1k", "10k"], fontsize=7, color=MUTE)
        ax.set_yticks([-20, 0, 20, 40]); ax.tick_params(axis="y", labelsize=7, colors=MUTE)
        ax.grid(True, color=GRIDC, lw=0.5); ax.minorticks_off()
        for s in ax.spines.values():
            s.set_color(GRIDC)
        ax.set_title(f"{vowel}  ·  '{word}'  ·  subject {subject[1]}", fontsize=9, color=INK, loc="left")
        report.append((vowel, subject, word, [(round(f), round(bw), round(p, 1)) for p, f, bw in res]))
fig.suptitle("DVTD vowels, poles only: measured vocal-tract transfer function (grey, Birkholz et al., 3D-printed tracts) and its twelfth-order all-pole model (blue), "
             "six pole pairs and no zeros at 11,025 Hz, E-mu's Env for speech. Dots = the poles (Hz), heights read off the model.", fontsize=9, color=INK, x=0.01, ha="left")
fig.text(0.01, 0.003, "Hz axis 60 to 16 k, octave grid, hard 0 dB line, curves relative to their median. Source: recipes/vocal/dvtd (Dresden Vocal Tract Dataset).", fontsize=7, color=MUTE)
fig.tight_layout(rect=(0, 0.01, 1, 0.985))
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dvtd_vowel_poles.png")
fig.savefig(out, facecolor="white")
with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "dvtd_vowel_poles.txt"), "w", encoding="utf-8") as fh:
    for vowel, subject, word, rows in report:
        fh.write(f"{vowel:<9} {subject} {word:<9} " + "  ".join(f"{f}/{bw}({p:+})" for f, bw, p in rows) + "\n")
print(out)
