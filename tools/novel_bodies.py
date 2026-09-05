import json
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "native", "python"))
import trench_core as tc

DATUM = 44100.0
IDENT = [0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF]
FRAMES = {f["name"]: f["words"] for f in json.load(open(os.path.join(ROOT, "native", "python", "workstation", "frames_3d.json"), encoding="utf-8"))}
OUT = os.path.join(ROOT, "plugin", "presets", "user", "novel")


def frame(name):
    return [list(r) for r in FRAMES[name]]


def geom_get(row):
    g = tc.TrenchSectionGeometry()
    w = (tc.ctypes.c_uint16 * 5)(*row)
    tc._dll.trench_section_geometry_get(tc.ctypes.byref(w), tc.ctypes.c_double(DATUM), tc.ctypes.byref(g))
    return g


def geom_set(g):
    w = (tc.ctypes.c_uint16 * 5)()
    tc._dll.trench_section_geometry_set(tc.ctypes.byref(g), tc.ctypes.c_double(DATUM), tc.ctypes.byref(w))
    return [int(w[i]) for i in range(5)]


def clamp_pair(hz, r, cap):
    return min(max(hz, 20.0), DATUM * 0.495), min(max(r, 0.0), cap)


def set_pole(row, hz, r):
    g = geom_get(row)
    g.pole_type = 1
    g.pole_a, g.pole_b = clamp_pair(hz, r, 0.9995)
    out = geom_set(g)
    out[4] = row[4]
    return out


def set_zero(row, hz, r):
    g = geom_get(row)
    g.zero_type = 1
    g.zero_a, g.zero_b = clamp_pair(hz, r, 1.0)
    out = geom_set(g)
    out[4] = row[4]
    return out


def swap_pole_zero(row):
    g = geom_get(row)
    if g.pole_type != 1 or g.zero_type != 1 or g.zero_a > 15000.0:
        return list(row)
    ph, pr, zh, zr = g.pole_a, g.pole_b, g.zero_a, g.zero_b
    g.pole_a, g.pole_b = clamp_pair(zh, zr, 0.9995)
    g.zero_a, g.zero_b = clamp_pair(ph, pr, 1.0)
    out = geom_set(g)
    out[4] = row[4]
    return out


def transpose(row, ratio):
    g = geom_get(row)
    if g.pole_type == 1:
        g.pole_a, g.pole_b = clamp_pair(g.pole_a * ratio, g.pole_b, 0.9995)
    if g.zero_type == 1:
        g.zero_a, g.zero_b = clamp_pair(g.zero_a * ratio, g.zero_b, 1.0)
    out = geom_set(g)
    out[4] = row[4]
    return out


def sharpen(words, keep=0.25):
    out = []
    for row in words:
        g = geom_get(row)
        if g.pole_type == 1:
            out.append(set_pole(row, g.pole_a, 1.0 - (1.0 - g.pole_b) * keep))
        else:
            out.append(list(row))
    return out


def body_of(corners):
    b = tc.Body.from_legacy_bytes(bytes([0] * 240))
    for c in range(8):
        for s in range(6):
            b.set_words(c, s, corners[c % 4][s])
    return b


def response(corners, morph, q, hz):
    return tc.cascade_response_db(body_of(corners).cascade(morph, q, 0.0, DATUM, DATUM)[:6], hz, DATUM)


def unity(words):
    words = [list(r) for r in words]
    for r in words:
        r[4] = tc.encode_word(0.25)
    probe = [words, words, words, words]
    dc = float(response(probe, 0.0, 0.0, [5.0])[0])
    g = 10.0 ** (-dc / 20.0 / 6.0)
    word = tc.encode_word(min(1.0, max(0.0, g / 4.0)))
    for r in words:
        r[4] = word
    return words


def poles_of(words):
    out = []
    for row in words:
        g = geom_get(row)
        if g.pole_type == 1 and g.pole_b > 0.5:
            out.append(f"{g.pole_a:.0f}")
    return " ".join(out)


def write(name, corners, note):
    corners = [unity(c) for c in corners]
    b = body_of(corners)
    data = b.to_legacy_bytes()
    assert len(data) == 240 and b.is_legacy_representable()
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".body240")
    open(path, "wb").write(data)
    dc = [float(response(corners, m, q, [5.0])[0]) for m, q in ((0, 0), (1, 0), (0, 1), (1, 1))]
    print(f"{name:<12} dc {max(abs(x) for x in dc):.3f} dB  " + " | ".join(poles_of(c) for c in corners) + f"   {note}")
    return corners


def head():
    front, left, behind, above = (frame(n) for n in ("left ear az 0 el 0", "left ear az 90 el 0", "left ear az 180 el 0", "left ear az 0 el 60"))
    return write("head", [front, left, behind, above], "MORPH front>left, Q front>behind / left>above")


def anti_vowel():
    v = frame("Ooh To Eee · M0 Q0")
    anti = [swap_pole_zero(r) for r in v]
    return write("anti_vowel", [v, anti, sharpen(v), sharpen(anti)], "MORPH vowel>its negative")


def zeros_only():
    a, b = frame("Eeh To Aah · M0 Q0"), frame("Eeh To Aah · M1 Q0")
    m1 = []
    for ra, rb in zip(a, b):
        gb = geom_get(rb)
        m1.append(set_zero(ra, gb.zero_a, gb.zero_b) if gb.zero_type == 1 else list(ra))
    return write("zeros_only", [a, m1, sharpen(a), sharpen(m1)], "same poles, zeros walk eeh>aah")


def chimera():
    rows0 = [("Aud Wall 1 C4", 0), ("eh head man", 0), ("uh hud woman", 1), ("Talking Hedz · M0 Q0", 3), ("left ear az 90 el 0", 0), ("Talking Hedz · M0 Q0", 5)]
    rows1 = [("Aud Wall 2 C#4", 0), ("eh head woman", 0), ("uh hud man", 1), ("Talking Hedz · M1 Q0", 3), ("left ear az 0 el 0", 0), ("Talking Hedz · M1 Q0", 5)]
    m0 = [frame(n)[s] for n, s in rows0]
    m1 = [frame(n)[s] for n, s in rows1]
    return write("chimera", [m0, m1, sharpen(m0), sharpen(m1)], "six rows from six sounds")


def fifth():
    a, b = frame("Ooh To Eee · M0 Q0"), frame("Ooh To Eee · M1 Q0")
    return write("fifth", [a, b, [transpose(r, 1.5) for r in a], [transpose(r, 1.5) for r in b]], "Q plays the fifth")


def sheet(bodies, path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    hz = np.geomspace(20, 20000, 400)
    fig, axes = plt.subplots(len(bodies), 1, figsize=(11, 2.4 * len(bodies)), dpi=110, facecolor="black")
    for ax, (name, corners) in zip(axes, bodies):
        ax.set_facecolor("black")
        for (m, q), col, lab in (((0, 0), "#ffffff", "M0 Q0"), ((1, 0), "#00e5ff", "M1 Q0"), ((0.5, 0), "#ffe100", "M 0.5"), ((0, 1), "#7a7a7a", "M0 Q1"), ((1, 1), "#3a8a95", "M1 Q1")):
            ax.plot(hz, response(corners, m, q, hz), color=col, lw=1.2 if m != 0.5 else 1.6, label=lab)
        ax.set_xscale("log"); ax.set_xlim(20, 20000); ax.set_ylim(-30, 30)
        ax.axhline(0, color="#7a7a7a", lw=0.8)
        for f in (100, 1000, 10000): ax.axvline(f, color="#383838", lw=0.6)
        ax.set_yticks([-20, 0, 20]); ax.set_xticks([100, 1000, 10000]); ax.set_xticklabels(["100", "1k", "10k"])
        ax.tick_params(colors="#7a7a7a", labelsize=8)
        for s in ax.spines.values(): s.set_color("#383838")
        ax.text(0.005, 0.92, name, transform=ax.transAxes, color="#c8c8c8", fontsize=10, family="monospace", va="top")
        if ax is axes[0]: ax.legend(loc="upper right", fontsize=7, frameon=False, labelcolor="#c8c8c8", ncol=5)
    fig.tight_layout()
    fig.savefig(path, facecolor="black")
    print(path)


if __name__ == "__main__":
    bodies = [("head", head()), ("anti_vowel", anti_vowel()), ("zeros_only", zeros_only()), ("chimera", chimera()), ("fifth", fifth())]
    sheet(bodies, sys.argv[1] if len(sys.argv) > 1 else os.path.join(OUT, "novel_bodies.png"))


def head_hedz():
    heads = [frame(n) for n in ("left ear az 0 el 0", "left ear az 90 el 0", "left ear az 180 el 0", "left ear az 0 el 60")]
    hedz = [frame(n) for n in ("Talking Hedz · M0 Q0", "Talking Hedz · M1 Q0", "Talking Hedz · M0 Q1", "Talking Hedz · M1 Q1")]
    corners = []
    for h, t in zip(heads, hedz):
        bells = [r for r in h if geom_get(r).pole_type == 1 and geom_get(r).pole_b > 0.5][:4]
        while len(bells) < 4:
            bells.append(list(IDENT))
        corners.append([t[0]] + bells + [t[5]])
    return write("head_hedz", corners, "Hedz row 1 tilt, head bells 2-5, Hedz row 6 ceiling")
