import os, struct, zipfile, importlib.util
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
spec = importlib.util.spec_from_file_location("va", os.path.join(HERE, "vowel_bodies_anatomy.py"))
va = importlib.util.module_from_spec(spec)
src = open(os.path.join(HERE, "vowel_bodies_anatomy.py"), encoding="utf-8").read().replace("if __name__", "if False and __name__")
exec(compile(src, "va", "exec"), va.__dict__)
FS = 44100.0
BODIES = ["ooh_to_eee", "eeh_to_aah", "talking_hedz", "ubu_orator", "multi_q_vox", "deep_bouche"]
INK = "#1d2126"; MUTE = "#8d959f"; GRIDC = "#dde1e6"; BLUE = "#2F6FDB"; GREY = "#9aa3ad"


def section_db(fp, bwp, fz, bwz, hz):
    w = 2 * np.pi * hz / FS
    rp = np.exp(-np.pi * bwp / FS); tp = 2 * np.pi * fp / FS
    den = np.abs(1 - 2 * rp * np.cos(tp) * np.exp(-1j * w) + rp * rp * np.exp(-2j * w))
    dc = 1 - 2 * rp * np.cos(tp) + rp * rp
    if fz is None:
        return 20 * np.log10(abs(dc) / den)
    rz = 1.0 if bwz <= 0 else np.exp(-np.pi * bwz / FS); tz = 2 * np.pi * fz / FS
    num = np.abs(1 - 2 * rz * np.cos(tz) * np.exp(-1j * w) + rz * rz * np.exp(-2j * w))
    dcz = 1 - 2 * rz * np.cos(tz) + rz * rz
    return 20 * np.log10(num / den * abs(dc / dcz))


z = zipfile.ZipFile(os.path.join(ROOT, "evidence", "factory-data", "p2k", "bodies", "p2k.zip"))
names = {n.split("_", 2)[2].rsplit(".", 1)[0]: n for n in z.namelist() if n.endswith(".bin")}
grid = np.geomspace(60, 16000, 500)
fig, axes = plt.subplots(len(BODIES), 2, figsize=(11, 2.1 * len(BODIES)), dpi=100)
fig.patch.set_facecolor("white")
lines = []
for row, body in enumerate(BODIES):
    words = struct.unpack("<120H", z.read(names[body])[:240])
    for col, corner in enumerate((0, 1)):
        ax = axes[row, col]
        secs = va.corner_sections(words, corner)
        full = np.zeros_like(grid); poles = np.zeros_like(grid); marks = []
        for s in secs:
            if s is None:
                continue
            b, p, zz = s
            fp, bwp = p[0], p[1]
            if not (20 < fp < 20000):
                continue
            poles += section_db(fp, bwp, None, None, grid)
            if zz is None:
                full += section_db(fp, bwp, None, None, grid)
            else:
                full += section_db(fp, bwp, zz[0], zz[1], grid)
            marks.append((fp, bwp))
        full -= np.median(full); poles -= np.median(poles)
        ax.plot(grid, full, color=GREY, lw=1.2)
        ax.plot(grid, poles, color=BLUE, lw=2.0)
        for fp, bwp in sorted(marks):
            level = float(np.interp(fp, grid, poles))
            ax.plot(fp, level, "o", ms=5, mfc="white", mec=BLUE, mew=1.4)
            ax.annotate(f"{fp:.0f}", (fp, level), textcoords="offset points", xytext=(0, 7), ha="center", fontsize=7, color=INK)
        ax.set_xscale("log"); ax.set_xlim(60, 16000); ax.set_ylim(-40, 40)
        ax.axhline(0, color=INK, lw=0.9)
        ax.set_xticks([80, 160, 320, 640, 1300, 2600, 5100, 10000])
        ax.set_xticklabels(["80", "160", "320", "640", "1.3k", "2.6k", "5.1k", "10k"], fontsize=7, color=MUTE)
        ax.set_yticks([-40, -20, 0, 20, 40]); ax.tick_params(axis="y", labelsize=7, colors=MUTE)
        ax.grid(True, color=GRIDC, lw=0.5); ax.minorticks_off()
        for sp in ax.spines.values():
            sp.set_color(GRIDC)
        ax.set_title(f"{body}  ·  corner M{corner} Q0", fontsize=9, color=INK, loc="left")
        lines.append(f"{body:<13} M{corner}  poles: " + "  ".join(f"{fp:.0f}/{bwp:.0f}" for fp, bwp in sorted(marks)))
fig.suptitle("E-mu's vowel bodies, poles only: the hand-voiced corner (grey, poles and zeros) and its six poles alone (blue), both at DC unity, relative to their median. "
             "Dots = the poles (Hz).", fontsize=9, color=INK, x=0.01, ha="left")
fig.text(0.01, 0.003, "Hz axis 60 to 16 k, octave grid, hard 0 dB line. Source: evidence/factory-data/p2k/bodies/p2k.zip, decoded by vowel_bodies_anatomy.py.", fontsize=7, color=MUTE)
fig.tight_layout(rect=(0, 0.01, 1, 0.975))
out = os.path.join(HERE, "emu_vowel_poles.png"); fig.savefig(out, facecolor="white")
open(os.path.join(HERE, "emu_vowel_poles.txt"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
print(out)
