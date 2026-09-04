import os, re, glob
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
INK = "#1d2126"; MUTE = "#8d959f"; GRIDC = "#dde1e6"; BLUE = "#2F6FDB"; ORANGE = "#D9782D"; GREY = "#9aa3ad"
FS = 11025.0

emu = {}
for line in open(os.path.join(HERE, "emu_vowel_poles.txt"), encoding="utf-8"):
    m = re.match(r"(\S+)\s+M(\d)\s+poles:\s+(.*)", line.strip())
    if m:
        emu[(m.group(1), int(m.group(2)))] = sorted((float(t.split("/")[0]), float(t.split("/")[1])) for t in m.group(3).split())


def load(path):
    d = np.loadtxt(path, skiprows=1); return d[:, 0], d[:, 1]


PAIRS = [(("eeh_to_aah", 1), "E-mu aah (Eeh To Aah M1)", "s1-01-bahn-tense-a", "DVTD /a/ 'bahn' subject 1"),
         (("ooh_to_eee", 1), "E-mu ee (Ooh To Eee M1)", "s1-03-tiere-tense-i", "DVTD /i/ 'tiere' subject 1"),
         (("ooh_to_eee", 0), "E-mu oo (Ooh To Eee M0)", "s2-04-boote-tense-o", "DVTD /o/ 'boote' subject 2"),
         (("deep_bouche", 0), "E-mu Deep Bouche M0", "s1-02-beet-tense-e", "DVTD /e/ 'beet' subject 1")]
fig, axes = plt.subplots(len(PAIRS), 1, figsize=(11, 2.6 * len(PAIRS)), dpi=100)
fig.patch.set_facecolor("white")
for ax, (ek, elabel, dname, dlabel) in zip(axes, PAIRS):
    path = glob.glob(os.path.join(ROOT, "recipes", "vocal", "dvtd", "subject-*", dname, "*-vvtf-measured.txt"))[0]
    hz, mag = load(path)
    meas = 20 * np.log10(np.maximum(mag, 1e-6)); meas -= np.median(meas[(hz > 60) & (hz < 5500)])
    keep = (hz > 60) & (hz < 5500)
    ax.plot(hz[keep], meas[keep], color=GREY, lw=1.4, label=dlabel)
    for fp, bw in emu[ek]:
        if fp < 5500:
            y = float(np.interp(fp, hz, meas))
            ax.plot(fp, y, "o", ms=8, mfc="white", mec=BLUE, mew=2.0)
            ax.annotate(f"{fp:.0f}", (fp, y), textcoords="offset points", xytext=(0, 9), ha="center", fontsize=8, color=BLUE)
        if fp * 2 < 5500:
            y = float(np.interp(fp * 2, hz, meas))
            ax.plot(fp * 2, y, "s", ms=7, mfc="white", mec=ORANGE, mew=1.6)
    ax.plot([], [], "o", ms=8, mfc="white", mec=BLUE, mew=2.0, label=elabel + " poles, as voiced")
    ax.plot([], [], "s", ms=7, mfc="white", mec=ORANGE, mew=1.6, label=elabel + " poles, one octave up")
    ax.set_xscale("log"); ax.set_xlim(60, 5500); ax.set_ylim(-30, 40)
    ax.axhline(0, color=INK, lw=0.9)
    ax.set_xticks([80, 160, 320, 640, 1300, 2600, 5100]); ax.set_xticklabels(["80", "160", "320", "640", "1.3k", "2.6k", "5.1k"], fontsize=8, color=MUTE)
    ax.set_yticks([-20, 0, 20, 40]); ax.tick_params(axis="y", labelsize=8, colors=MUTE)
    ax.grid(True, color=GRIDC, lw=0.5); ax.minorticks_off()
    for sp in ax.spines.values():
        sp.set_color(GRIDC)
    ax.legend(fontsize=8, frameon=False, loc="upper right")
fig.suptitle("Do E-mu's vowel poles need an octave shift? Measured tract (grey) with the E-mu corner's poles as voiced (blue circles) and shifted one octave up (orange squares).",
             fontsize=9, color=INK, x=0.01, ha="left")
fig.text(0.01, 0.004, "Hz axis 60 to 5.5 k, octave grid, hard 0 dB line. Sources: p2k.zip via emu_vowel_poles.txt; recipes/vocal/dvtd.", fontsize=7, color=MUTE)
fig.tight_layout(rect=(0, 0.01, 1, 0.97))
out = os.path.join(HERE, "emu_vowels_vs_dvtd_octave.png"); fig.savefig(out, facecolor="white"); print(out)
