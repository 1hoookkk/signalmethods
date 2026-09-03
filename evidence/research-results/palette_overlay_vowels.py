import math, os, struct, zipfile
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SR = 44100.0
IDENT = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
HILLENBRAND_MALE = {"oo /u/": [(378.0, 50.0), (997.0, 80.0), (2343.0, 120.0)],
                    "ee /i/": [(342.0, 50.0), (2322.0, 80.0), (3000.0, 120.0)]}


def decode_word(w):
    u = w + 1
    if u >= 65536:
        return 1.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return math.ldexp(x, e - 15)


def biquad(words):
    d = [decode_word(x) for x in words]
    c0, c1, c2, c3, c4 = 4 * d[0] + d[1], d[1], 4 * d[2] + d[3], d[3], 4 * d[4]
    return (c4, (c0 - 2) * c4, (1 - c1) * c4, c2 - 2, 1 - c3)


def pole_of(b):
    a1, a2 = b[3], b[4]
    if a2 <= 0 or a1 * a1 - 4 * a2 >= 0:
        return None
    r = math.sqrt(a2)
    return math.acos(max(-1.0, min(1.0, -a1 / (2 * r)))) / (2 * math.pi) * SR, -math.log(r) * SR / math.pi


def emu_corner(words, corner):
    secs = []
    for s in range(6):
        w = words[(corner * 6 + s) * 5:(corner * 6 + s) * 5 + 5]
        if tuple(w) != IDENT:
            secs.append(biquad(w))
    return secs


def response(secs, f):
    z = np.exp(-2j * np.pi * f / SR)
    h = np.ones_like(z)
    for b0, b1, b2, a1, a2 in secs:
        h *= (b0 + b1 * z + b2 * z * z) / (1 + a1 * z + a2 * z * z)
    return 20 * np.log10(np.abs(h) + 1e-12)


def overlay(formants):
    secs = []
    for hz, bw in formants:
        r = math.exp(-math.pi * bw / SR)
        theta = 2 * math.pi * hz / SR
        a1, a2 = -2 * r * math.cos(theta), r * r
        b0 = 1 + a1 + a2
        secs.append((b0, 0.0, 0.0, a1, a2))
    return secs


def rms_match(secs, target_secs, f):
    ours = response(secs, f)
    theirs = response(target_secs, f)
    shift = np.sqrt(np.mean((10 ** (theirs / 20)) ** 2)) / np.sqrt(np.mean((10 ** (ours / 20)) ** 2))
    return [(b0 * shift, b1 * shift, b2 * shift, a1, a2) for b0, b1, b2, a1, a2 in secs], 20 * math.log10(shift)


def cents(a, b):
    return 1200.0 * math.log2(a / b)


def main():
    z = zipfile.ZipFile(os.path.join(ROOT, "evidence", "factory-data", "p2k", "bodies", "p2k.zip"))
    raw = z.read([i for i in z.infolist() if "ooh_to_eee" in i.filename][0])
    words = struct.unpack("<120H", raw)
    f = np.geomspace(60, 12000, 900)
    fig, axes = plt.subplots(1, 2, figsize=(15, 5.2))
    report = []
    for ax, (label, formants), corner in zip(axes, HILLENBRAND_MALE.items(), (0, 1)):
        emu = emu_corner(words, corner)
        emu_poles = sorted(p for p in (pole_of(b) for b in emu) if p)
        ov, shift_db = rms_match(overlay(formants), emu, f)
        ax.semilogx(f, response(emu, f), color="#c0392b", lw=1.6, label=f"Ooh To Eee corner {corner} (E-mu)")
        ax.semilogx(f, response(ov, f), color="#1f77b4", lw=1.4, label=f"overlay of Hillenbrand {label}, RMS matched ({shift_db:+.1f} dB)")
        for hz, _ in formants:
            ax.axvline(hz, color="#1f77b4", alpha=0.25, lw=0.8)
        for hz, _ in emu_poles:
            ax.axvline(hz, color="#c0392b", alpha=0.25, lw=0.8, ls="--")
        ax.set_ylim(-40, 40)
        ax.set_xlim(60, 12000)
        ax.axhline(0, color="k", lw=0.8)
        ax.grid(True, which="both", alpha=0.25)
        ax.set_title(f"{label}: E-mu poles " + " ".join(f"{p[0]:.0f}" for p in emu_poles) + " Hz", fontsize=9)
        ax.legend(fontsize=8, loc="lower left")
        lines = [f"== {label}: E-mu corner {corner}"]
        for i, (hz, bw) in enumerate(formants):
            nearest = min(emu_poles, key=lambda p: abs(cents(p[0], hz)))
            lines.append(f"  F{i+1} {hz:6.0f} Hz  nearest E-mu pole {nearest[0]:6.0f} Hz bw {nearest[1]:5.0f}  distance {cents(nearest[0], hz):+6.0f} cents")
        lines.append("  E-mu poles with no formant within 300 cents: " + ", ".join(f"{p[0]:.0f}" for p in emu_poles if all(abs(cents(p[0], hz)) > 300 for hz, _ in formants)))
        lines.append(f"  RMS shift to match level: {shift_db:+.1f} dB; worst |diff| over 60 Hz..12 kHz: {np.max(np.abs(response(emu, f) - response(ov, f))):.1f} dB")
        report.append("\n".join(lines))
    fig.suptitle("palette by overlay, no fitting: textbook formants placed as poles, RMS-matched, against E-mu's own vowel corners", fontsize=11)
    fig.tight_layout()
    out = os.path.join(ROOT, "evidence", "research-results", "palette_overlay_vowels.png")
    fig.savefig(out, dpi=110)
    print(out)
    print("\n".join(report))


if __name__ == "__main__":
    main()
