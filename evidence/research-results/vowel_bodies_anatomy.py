import math, os, struct, zipfile
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SR = 44100.0
IDENT = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
VOWELS = ["multi_q_vox", "ooh_to_eee", "talking_hedz", "eeh_to_aah", "ubu_orator", "deep_bouche"]
CORNER = ["M0 Q0", "M1 Q0", "M0 Q1", "M1 Q1"]
COLOUR = ["#c0392b", "#e67e22", "#1f77b4", "#7b1fa2"]


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


def pair(p, q):
    if q <= 0 or p * p - 4 * q >= 0:
        return None
    r = math.sqrt(q)
    return math.acos(max(-1.0, min(1.0, -p / (2 * r)))) / (2 * math.pi) * SR, (-math.log(r) * SR / math.pi if r < 1 else 0.0), r


def corner_sections(words, corner):
    out = []
    for s in range(6):
        w = words[(corner * 6 + s) * 5:(corner * 6 + s) * 5 + 5]
        if tuple(w) == IDENT:
            out.append(None)
            continue
        b = biquad(w)
        out.append((b, pair(b[3], b[4]), pair(b[1] / b[0], b[2] / b[0]) if abs(b[0]) > 1e-12 else None))
    return out


def response(secs, f):
    z = np.exp(-2j * np.pi * f / SR)
    h = np.ones_like(z)
    for s in secs:
        if s is None:
            continue
        b0, b1, b2, a1, a2 = s[0]
        h *= (b0 + b1 * z + b2 * z * z) / (1 + a1 * z + a2 * z * z)
    return 20 * np.log10(np.abs(h) + 1e-12)


def main():
    z = zipfile.ZipFile(os.path.join(ROOT, "evidence", "factory-data", "p2k", "bodies", "p2k.zip"))
    f = np.geomspace(40, 16000, 900)
    fig, axes = plt.subplots(2, 3, figsize=(18, 9))
    lines = []
    for ax, name in zip(axes.flat, VOWELS):
        raw = z.read([i for i in z.infolist() if name in i.filename][0])
        words = struct.unpack("<120H", raw)
        lines.append(f"===== {name}")
        for corner in range(4):
            secs = corner_sections(words, corner)
            ax.semilogx(f, response(secs, f), color=COLOUR[corner], lw=1.3, label=CORNER[corner])
            poles = [s[1] for s in secs if s and s[1]]
            zeros = [s[2] for s in secs if s and s[2]]
            b0 = next((s[0][0] for s in secs if s), 0.0)
            lines.append(f"  corner {CORNER[corner]}  b0 {b0:.3f}")
            lines.append("    poles: " + "  ".join(f"{p[0]:6.0f}/{p[1]:4.0f}" for p in sorted(poles)))
            lines.append("    zeros: " + "  ".join(f"{q[0]:6.0f}/{q[1]:4.0f}{'*' if q[2] >= 0.9999 else ''}" for q in sorted(zeros)))
        ax.set_ylim(-40, 40)
        ax.set_xlim(40, 16000)
        ax.axhline(0, color="k", lw=0.8)
        ax.grid(True, which="both", alpha=0.25)
        ax.set_title(name, fontsize=10)
        ax.legend(fontsize=7, loc="lower left")
    fig.suptitle("the six X3 vowel bodies, four corners each (Hz/bandwidth Hz per pole below; * = unit-circle zero)", fontsize=11)
    fig.tight_layout()
    out = os.path.join(ROOT, "evidence", "research-results", "vowel_bodies_anatomy.png")
    fig.savefig(out, dpi=105)
    open(os.path.join(ROOT, "evidence", "research-results", "vowel_bodies_anatomy.txt"), "w").write("\n".join(lines))
    print(out)
    print("\n".join(lines))


if __name__ == "__main__":
    main()
