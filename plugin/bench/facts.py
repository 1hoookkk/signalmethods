from __future__ import annotations

import glob
import json
import math
import os
import statistics
import struct
import sys
import textwrap
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

import numpy as np
from pyruntime.packed_interp import decode

SR = 44100.0
K = 4.0
CHARACTER = sorted(p for p in glob.glob(str(ROOT / "ref/presets/P2k_0*.bin"))
                   if int(os.path.basename(p)[4:7]) < 33)


def _rows(path, ci):
    raw = struct.unpack("<120H", open(path, "rb").read())
    out = []
    for s in range(6):
        o = (ci * 6 + s) * 5
        d = [decode(raw[o + k]) for k in range(5)]
        c0 = K * d[0] + d[1]; c1 = d[1]; c2 = K * d[2] + d[3]; c3 = d[3]; c4 = K * d[4]
        out.append((c4, (c0 - 2) * c4, (1 - c1) * c4, c2 - 2, 1 - c3))
    return out


def _cdb(rows, f):
    z = np.exp(-2j * np.pi * f / SR)
    acc = np.ones_like(f, dtype=complex)
    for b0, b1, b2, a1, a2 in rows:
        acc *= (b0 + b1 * z + b2 * z * z) / (1 + a1 * z + a2 * z * z)
    return 20 * np.log10(np.maximum(np.abs(acc), 1e-12))


def _conj(p, q):
    disc = p * p - 4 * q
    if disc >= 0:
        return None, None
    r = math.sqrt(max(q, 0.0))
    if r < 1e-12:
        return None, None
    return math.acos(max(-1.0, min(1.0, -p / (2 * r)))) / (2 * math.pi) * SR, r


def _peaks(f, d, prom=4.0, w=70):
    return [f[i] for i in range(2, len(d) - 2)
            if d[i] > d[i - 1] and d[i] >= d[i + 1]
            and d[i] - min(d[max(0, i - w):i].min(), d[i:i + w].min()) >= prom]


@dataclass
class Fact:
    name: str
    value: float | None
    unit: str
    kind: str
    used_by: str
    note: str
    derive: Callable[[], float] | None = None
    tol: float = 0.0
    sample: str = ""
    quote: str = ""
    cite: str = ""


def q_arm():
    frac = []
    for p in sorted(glob.glob(str(ROOT / "ref/presets/P2k_*.bin"))):
        w = struct.unpack("<120H", open(p, "rb").read())
        for m0, m1 in ((0, 2), (1, 3)):
            for si in range(6):
                rs = []
                for ci in (m0, m1):
                    o = (ci * 6 + si) * 5
                    d = [decode(w[o + k]) for k in range(5)]
                    a1 = K * d[2] + d[3] - 2.0
                    a2 = 1.0 - d[3]
                    rs.append(_conj(a1, a2)[1])
                a, b = rs
                if a and b and a < 0.9995 and b < 1.0:
                    frac.append((b - a) / (1.0 - a))
    return statistics.median(frac)


def pole_placement_error_st():
    f = np.geomspace(20, 20000, 8000)
    err = []
    for p in CHARACTER:
        for ci in range(4):
            rows = _rows(p, ci)
            pk = _peaks(f, _cdb(rows, f), prom=3.0)
            if not pk:
                continue
            for r in rows:
                hz, _ = _conj(r[3], r[4])
                if hz is None or not (40 < hz < 16000):
                    continue
                g = min(pk, key=lambda x: abs(math.log2(x / hz)))
                e = abs(12 * math.log2(g / hz))
                if e <= 12.0:
                    err.append(e)
    return statistics.median(err)


def pole_survival_pct():
    f = np.geomspace(20, 20000, 8000)
    live = total = 0
    for p in CHARACTER:
        for ci in range(4):
            rows = _rows(p, ci)
            pk = _peaks(f, _cdb(rows, f), prom=3.0)
            for r in rows:
                hz, _ = _conj(r[3], r[4])
                if hz is None or not (40 < hz < 16000):
                    continue
                total += 1
                if pk and abs(12 * math.log2(
                        min(pk, key=lambda x: abs(math.log2(x / hz))) / hz)) <= 12.0:
                    live += 1
    return 100.0 * live / total


def _arch(stat):
    f = np.geomspace(20, 20000, 4000)
    npk, gaps, crowns, dips, tilts = [], [], [], [], []
    for p in CHARACTER:
        for ci in (0, 1):
            d = _cdb(_rows(p, ci), f)
            band = (f > 60) & (f < 16000)
            med = float(np.median(d[band]))
            pk = _peaks(f, d, prom=4.0)
            npk.append(len(pk))
            lo = float(np.mean(d[(f > 60) & (f < 300)]))
            hi = float(np.mean(d[(f > 4000) & (f < 12000)]))
            tilts.append(abs(lo - hi))
            if not pk:
                continue
            crowns.append(float(d[band].max() - med))
            dips.append(med - float(d[band].min()))
            gaps += [math.log2(pk[i + 1] / pk[i]) for i in range(len(pk) - 1)]
    return statistics.median({"peaks": npk, "gap": gaps, "crown": crowns,
                              "dip": dips, "tilt": tilts}[stat])


def zero_on_unit_circle_pct():
    n = hits = 0
    for p in CHARACTER:
        w = struct.unpack("<120H", open(p, "rb").read())
        for i in range(24):
            d = [decode(w[i * 5 + k]) for k in range(5)]
            hz, r = _conj(K * d[0] + d[1] - 2.0, 1.0 - d[1])
            if r is None:
                continue
            n += 1
            if r >= 0.9999:
                hits += 1
    return 100.0 * hits / n


def _lerp_u16(a, b, frac):
    """trench-core/src/minifloat.rs lerp_u16, bit for bit."""
    d = np.int32(np.float32(np.float32(np.int32(b) - np.int32(a)) * np.float32(frac)))
    d16 = np.int16(np.uint16(np.uint32(d) & np.uint32(0xFFFF)))
    return int((np.int32(d16) + np.int32(a)) & np.int32(0xFFFF))


def interp_order_max_lsb():
    """Morph-first vs Q-first on every factory corner set, 11x11 grid.

    Bilinear is a tensor product: in exact arithmetic the two orders are the
    same polynomial, so any difference is the u16 rounding inside lerp_u16.
    Returns the largest word-space disagreement, in LSB of 65536.
    """
    worst = 0
    for p in sorted(glob.glob(str(ROOT / "ref/presets/P2k_*.bin"))):
        w = struct.unpack("<120H", open(p, "rb").read())
        for si in range(6):
            for k in range(5):
                a, b, c, d = (w[(ci * 6 + si) * 5 + k] for ci in range(4))
                for mi in range(11):
                    for qi in range(11):
                        m, q = mi / 10.0, qi / 10.0
                        mf = _lerp_u16(_lerp_u16(a, b, m), _lerp_u16(c, d, m), q)
                        qf = _lerp_u16(_lerp_u16(a, c, q), _lerp_u16(b, d, q), m)
                        worst = max(worst, abs(mf - qf))
    return float(worst)


def _slot_tilt_db(slot):
    """Median LF-minus-HF tilt of one slot's own response, over every frame of
    every character body. Says where a body's broadband shape comes from."""
    f = np.geomspace(20, 20000, 1200)
    z = np.exp(-2j * np.pi * f / SR)
    out = []
    for p in CHARACTER:
        for ci in range(4):
            b0, b1, b2, a1, a2 = _rows(p, ci)[slot]
            d = 20 * np.log10(
                np.abs((b0 + b1 * z + b2 * z * z) / (1 + a1 * z + a2 * z * z)) + 1e-12)
            out.append(float(d[f < 80].mean()) - float(d[f > 8000].mean()))
    return statistics.median(out)


def _delivery(lo_st, hi_st, want):
    """For every conjugate pole in the character block: how much of the boost
    that section makes ON ITS OWN actually arrives in the whole cascade,
    bucketed by how far away the nearest OTHER pole in the same frame is.

    want="excess" -> median (delivered - authored) dB
    want="within" -> % of poles arriving within 6 dB of what they asked for
    """
    f = np.geomspace(20, 20000, 2400)
    z = np.exp(-2j * np.pi * f / SR)

    def sdb(r):
        b0, b1, b2, a1, a2 = r
        return 20 * np.log10(
            np.abs((b0 + b1 * z + b2 * z * z) / (1 + a1 * z + a2 * z * z)) + 1e-12)

    def prom(c, hz):
        i = int(np.argmin(np.abs(f - hz)))
        return c[i] - float(c[np.searchsorted(f, hz / 1.5):np.searchsorted(f, hz * 1.5)].min())

    hits = []
    for p in CHARACTER:
        for ci in range(4):
            rows = _rows(p, ci)
            curves = [sdb(r) for r in rows]
            total = np.sum(curves, axis=0)
            poles = [_conj(r[3], r[4])[0] for r in rows]
            for si, r in enumerate(rows):
                hz = poles[si]
                if hz is None or not (60 < hz < 12000):
                    continue
                own = prom(curves[si], hz)
                if own < 3:
                    continue
                others = [abs(12 * math.log2(h / hz))
                          for j, h in enumerate(poles) if j != si and h and h > 20]
                gap = min(others) if others else 99.0
                if lo_st <= gap < hi_st:
                    hits.append(prom(total, hz) - own)
    if want == "within":
        return 100.0 * sum(1 for x in hits if abs(x) <= 6.0) / len(hits)
    return statistics.median(hits)


def _zero_offset_share(lo_st, hi_st):
    """Share of factory sections whose zero sits |lo..hi| semitones from its own
    pole. Two populations, not a continuum: a 4.6x spike inside +/-6 st and a
    long tail past +/-18. Uniform over this range would be ~8% per 12 st."""
    import glob as _g
    out = []
    for p in CHARACTER:
        w = struct.unpack("<120H", open(p, "rb").read())
        for ci in range(4):
            for s in range(6):
                d = [decode(w[(ci * 6 + s) * 5 + k]) for k in range(5)]
                zhz, _zr = _conj(K * d[0] + d[1] - 2.0, 1.0 - d[1])
                phz, _pr = _conj(K * d[2] + d[3] - 2.0, 1.0 - d[3])
                if zhz and phz and zhz > 20 and phz > 20:
                    out.append(abs(12 * math.log2(zhz / phz)))
    return 100.0 * sum(1 for x in out if lo_st <= x < hi_st) / len(out)


def scale_uniform_pct():
    """Share of corners whose six SCALE words are all the same value."""
    same = tot = 0
    for p in CHARACTER:
        w = struct.unpack("<120H", open(p, "rb").read())
        for ci in range(4):
            tot += 1
            same += len({w[(ci * 6 + s) * 5 + 4] for s in range(6)}) == 1
    return 100.0 * same / tot


FACTS = [
    Fact("Q_ARM", 0.6214, "fraction of the gap to the unit circle", "MEASURED",
         "tools/body_from_endpoints.py",
         "Q0 -> Q100 pole radius, median over 402 conjugate rows in all 50 bodies",
         q_arm, tol=0.002, sample="402 rows"),
    Fact("POLE_PLACEMENT_ST", 0.02, "semitones", "MEASURED", "the whole placement thesis",
         "median offset between a placed pole and the cascade peak it makes",
         pole_placement_error_st, tol=0.05, sample="539 of 710 poles"),
    Fact("POLE_SURVIVAL_PCT", 76.0, "percent", "MEASURED", "the whole placement thesis",
         "share of conjugate poles that produce any peak in the total",
         pole_survival_pct, tol=2.0, sample="710 poles"),
    Fact("ARCH_PEAKS", 4.0, "peaks per corner", "MEASURED", "the architecture target",
         "median peak count, 33 character bodies, M0 and M100",
         lambda: _arch("peaks"), tol=0.5),
    Fact("ARCH_GAP_OCT", 0.50, "octaves", "MEASURED", "the architecture target",
         "median gap between adjacent peaks",
         lambda: _arch("gap"), tol=0.05),
    Fact("ARCH_CROWN_DB", 17.8, "dB above median", "MEASURED", "the architecture target",
         "median crown", lambda: _arch("crown"), tol=1.0),
    Fact("ARCH_DIP_DB", 69.8, "dB below median", "MEASURED", "the architecture target",
         "median deepest notch — the mechanism, not a defect",
         lambda: _arch("dip"), tol=3.0),
    Fact("ARCH_TILT_DB", 25.5, "dB", "MEASURED", "the architecture target",
         "median low-band to high-band level difference",
         lambda: _arch("tilt"), tol=2.0),
    Fact("ZERO_UNIT_CIRCLE_PCT", 17.5, "percent", "MEASURED", "zero placement law",
         "share of conjugate zeros at r >= 0.9999 — a null on the circle is E-mu "
         "practice. Claimed 23.2 on 2026-08-13 from bodies 000-029 while describing "
         "it as the 33; corrected here by re-derivation.",
         zero_on_unit_circle_pct, tol=1.5),
    Fact("CLUSTER_PEAKS", 4, "sections", "MEASURED", "tools/body_from_endpoints.py",
         "follows ARCH_PEAKS", lambda: round(_arch("peaks")), tol=0.5),
    Fact("ZERO_R_MAX", 0.999, "radius", "DERIVED", "tools/tf_ingest.py",
         "the word format stores 1 - r^2; encode clamps <= 0 to 0x0000, which decodes "
         "to r = 1.000 exactly, a perfect null. Anything above this is unrepresentable."),
    Fact("BODY_BYTES", 560, "bytes", "DERIVED", "trench-core/src/minifloat.rs",
         "8 corners x 7 stages x 5 words x 2 bytes"),
    Fact("LEGACY_BODY_BYTES", 240, "bytes", "DERIVED", "trench-core/src/minifloat.rs",
         "4 corners x 6 stages x 5 words x 2 bytes"),
    Fact("INTERP_AXIS_ORDER_LSB", 2.0, "LSB of 65536", "DERIVED",
         "trench-core/src/minifloat.rs interpolate_words",
         "there is no axis order to source. Multilinear interpolation is a tensor "
         "product and is order-independent by construction; the derivation shows "
         "exact arithmetic agrees to 1.5e-11 words and the only disagreement is "
         "the u16 rounding inside lerp_u16, at most 2 LSB.",
         interp_order_max_lsb, tol=0.0, sample="50 bodies x 11x11 grid",
         quote="6. A digital filter as in claim 5 including means for "
               "interpolating between said first and second coefficients.",
         cite="US5170369 claim 6 (col. 12 ll. 10-11) — states no order, and the "
              "hardware it claims has one interpolating variable x, not two"),
    Fact("CORNER_BIT_ORDER", 0, "0 = m|q<<1|z<<2", "SOURCED",
         "trench-core/src/minifloat.rs PackedCorners",
         "E-mu numbers corners 'frames', 1-based, morph varying fastest: for a "
         "square, frames 1,3 are M0 and frames 2,4 are M100, so morphing up moves "
         "1,3 -> 2,4. Frame 1 is the all-axes-zero corner. Our 0-based "
         "m|q<<1|z<<2 reproduces that numbering exactly.",
         quote="Frames 1 and 3 contain multiple, unevenly spaced notches and "
               "peaks, while frames 2 and 4 are essentially flat. Modulating "
               "upward along the Morph axis, pushes the filter towards flat.",
         cite="Morpheus Operation Manual, F066 Notcher 2.4 "
              "(ref/morpheus_manual_zplane_descriptions.txt:714). Corroborated by "
              "F003 Flange 2 .4 ':241' \"Frame 1 starts with notches at 50, 100, "
              "200, 400, 800, and 1600Hz\" + \"Morph: Moves all of the notches up\", "
              "and by ':1594' \"Cube construction - ... fades off to rear frames\""),
    Fact("ARMA_PLOT_RADIUS", None, "dB of peak height", "SOURCED",
         "the editor board",
         "radius axis of the ARMAdillo plot. R' = 20 log10(1/(1-R)).",
         quote="R' = 20log10 1/(1-R)",
         cite="Rossum 1991, ARMAdillo, p.2 "
              "(ref/patents/rossum_armadillo_coefficient_encoding.pdf)"),
    Fact("ARMA_PLOT_ANGLE", None, "radians", "SOURCED",
         "the editor board",
         "angle axis of the ARMAdillo plot: theta' = pi(10 + log2(theta/pi))/10. "
         "The log2 argument is theta/pi, NOT the musical octave number Omega. "
         "AUTHORING_SPEC said 'log2 Omega' until 2026-08-13 and was wrong. The "
         "formula is sample-rate free: ten octaves of normalised frequency from "
         "pi/1024 to pi map linearly onto 0..pi.",
         quote="theta' = pi(10+log2 theta/pi) / 10",
         cite="Rossum 1991, ARMAdillo, p.2"),
    Fact("ARMA_PLOT_FLOOR", 2048.0, "sample_rate / this = lowest plotted Hz", "SOURCED",
         "trench-core/src/stage_law.rs display_freq_min_hz",
         "poles below theta = pi/1024 are off the plot; theta = pi/1024 is "
         "f = Fs/2048, and it is exactly where theta' reaches 0. Code agrees.",
         quote="We have found a useful graphical analysis tool to be the plot of "
               "the poles translating the radii from R to R' and the angles theta "
               "to theta' (excluding all theta below pi/1024 = 20 Hz) such that:",
         cite="Rossum 1991, ARMAdillo, p.2. The paper prints '=' as an "
              "approximately-equal glyph; 20 Hz is the value at Fs = 40 kHz"),
    Fact("ARMA_EVEN_DENSITY", None, "", "SOURCED",
         "why the editor board is in ARMAdillo coordinates",
         "the claim we lean on. Note the direction: even density INDICATES an "
         "even perceptual mapping. It is stated of a tool for judging a "
         "coefficient ENCODING, not as an instruction to space poles evenly.",
         quote="Such a plot maps the poles onto a nominally log/log semicircle, "
               "and even coefficient density indicates an even perceptual mapping.",
         cite="Rossum 1991, ARMAdillo, p.2"),
    Fact("RESONANCE_DB_PER_K", 8.68, "dB of peak height per unit of k2", "SOURCED",
         "trench-core/src/stage_law.rs, the editor resonance readout",
         "peak height in dB is linear in the encoded radius word, offset 6.02 dB.",
         quote="rho = 8.68 k2 +6.02",
         cite="Rossum 1991, ARMAdillo, p.1"),
    Fact("OCTAVE_IN_K", 1.386, "units of k1 per octave", "SOURCED",
         "trench-core/src/stage_law.rs, the editor pitch ruler",
         "2 ln 2. Pitch is linear in the encoded frequency word.",
         quote="Omega = -k1/(2ln2) + log2 Fs/(20 pi) = -k1/1.38 + 9.31",
         cite="Rossum 1991, ARMAdillo, p.2 (for Fs=40kHz); Omega defined p.1 as "
              "'the \"musical octave number\" Omega varying from zero to ten which "
              "is logarithmic in resonant frequency based on the pole angle theta "
              "and sample rate Fs', Omega = log2(theta Fs / 40 pi)"),
    Fact("INTERP_LINEAR_IN_ENCODED", None, "", "SOURCED",
         "trench-core/src/minifloat.rs interpolate_words",
         "the morph is a straight line between stored words. Nothing is refitted "
         "at intermediate positions. Code agrees.",
         quote="logarithmic interpolation of the coefficients is required to "
               "produce audibly meaningful sweeps. This is accomplished not "
               "through true logarithmic interpolation, but rather through linear "
               "interpolation of approximately logarithmically encoded "
               "coefficients. ... The coefficients are interpolated according to "
               "the formula C(x)=Ca+x(Cb-Ca), where x varies from zero to unity.",
         cite="US5170369 col. 5 l. 60 - col. 6 l. 5 and col. 6 ll. 42-45. Same in "
              "Rossum 1991 p.2: 'It linearly interpolates the coefficients in the "
              "encoded space at the sample rate'"),
    Fact("NUM_STAGES", 7, "second-order sections in series", "SOURCED",
         "trench-core/src/cascade.rs NUM_STAGES",
         "E-mu's own block diagram of the Morpheus filter. Parameter count checks "
         "it: 2 + 6x3 = 20.",
         quote="In [1 Low Pass Section: Fc Q] [6 Parametric Equalizer Sections: "
               "Fc Bw Gain x6] Out ... Right away you can see that we now have 20 "
               "different parameters to control.",
         cite="Morpheus Operation Manual p.99 signal-flow figure + body text "
              "(ref/morpheus_manual_zplane_descriptions.txt:185-202). Figure text "
              "extracts out of reading order; the labels are verbatim"),
    Fact("LOWPASS_SECTIONS", 0, "lowpass sections in the factory corpus", "SOURCED",
         "nothing — do not give a section a fixed type",
         "CORRECTED 2026-08-13. This was entered as 1 off the p.99 block diagram, "
         "reading '1 Low Pass Section + 6 Parametric Equalizer Sections' as the "
         "topology. The sentence directly above that figure calls it ONE POSSIBLE "
         "CONFIGURATION. It is an example, not the machine. Measured: of 792 "
         "factory sections (33 character bodies x 4 frames x 6) exactly ZERO are "
         "a lowpass or a plain shelf. Every one is a pole and a zero making a "
         "peak (23.0%), a notch (13.4%), both (61.1%) or nothing (2.5%). A "
         "section has no type; it has two roots. See SECTION_TILT_S1_DB.",
         quote="The Morpheus filter is actually much more complex than the four "
               "parametric sections described above. As an example of its power, "
               "the diagram below shows one of the possible ways that the "
               "Morpheus filter can be configured.",
         cite="Morpheus Operation Manual p.99 "
              "(ref/morpheus_manual_zplane_descriptions.txt:169-172), the "
              "sentence introducing the figure quoted by NUM_STAGES"),
    Fact("SECTION_TILT_S1_DB", -22.5, "dB, LF minus HF, median", "MEASURED",
         "how a body gets its broadband shape",
         "the tilt lives at the ENDS and they oppose. The value is LF minus HF, "
         "so the SIGN reads backwards from the shape: S1 is -22.5, meaning it "
         "RISES with frequency (LF -23.8, HF -2.5); S6 is +29.8, meaning it "
         "FALLS (LF +11.3, HF -11.5). S2-S5 sit between -0.8 and +12.9. "
         "Corrected 2026-08-13 — this note said the opposite, though the plate "
         "always showed it right. That opposition is why a "
         "factory body's total sits near 0 dB. Both end sections carry the "
         "biggest peak/notch features in the body at the same time (median "
         "prominence 51 dB at S1, 71 dB at S6), so they are not shelves — they "
         "are pole/zero pairs placed far enough apart that the skirt becomes the "
         "tilt. 53% of all sections carry more than 12 dB of it.",
         lambda: _slot_tilt_db(0), tol=1.5, sample="132 frames"),
    Fact("SECTION_TILT_S6_DB", 29.8, "dB, LF minus HF, median", "MEASURED",
         "how a body gets its broadband shape",
         "the far end of the same opposition — see SECTION_TILT_S1_DB",
         lambda: _slot_tilt_db(5), tol=1.5, sample="132 frames"),
    Fact("TRANSFORM2_LIVE_CUBES", 89, "of 94 cube filters", "SOURCED",
         "what the third axis is FOR",
         "Transform 2 is a HOW MUCH axis, not a WHERE axis. Across the 89 cube "
         "filters whose Transform 2 the manual describes, the recurring words "
         "are volume (26), brightness (22), resonance (13), effect (13), depth "
         "(9), amplitude (6). Morph picks the shape, Freq. Tracking places it on "
         "the keyboard, Transform 2 sets intensity. 85 of the 90 squares say "
         "'Not used', which is what the '.4' suffix means. This is the only "
         "evidence anywhere about a designed third axis — no 3-axis body has "
         "ever been decoded — and it is in words, not bytes.",
         quote="Controls the amplitude of all of the resonances, with 000 "
               "providing the lowest resonance amplitude",
         cite="Morpheus Operation Manual, F022 AEParaVowel "
              "(ref/morpheus_manual_zplane_descriptions.txt). See also F004 "
              "CubeFlanger 'Increases resonance and introduces some peaks.' and "
              "F021 AEParLPVow 'Controls volume, depth of effect.'"),
    Fact("CUBE_CORNERS", 8, "frames", "SOURCED",
         "trench-core/src/minifloat.rs PackedCorners",
         "three axes is a cube (8 frames), two axes is a square (4 frames). This "
         "is E-mu's primary statement; '2^N sets of coefficients' was only "
         "ever quoted from Symbolic Sound and is not needed.",
         quote="A suffix of \"4\" or \".4\" indicates filter is square, not cube "
               "and does not contain a Transform 2 axis.",
         cite="Morpheus Operation Manual p.185 "
              "(ref/morpheus_manual_zplane_descriptions.txt:208)"),
    Fact("TRACKING_ZERO", None, "", "SOURCED",
         "zero placement law; the editor's zero affordance",
         "the only stated job for a zero anywhere in the sources. A digital "
         "pole's rolloff flattens at high frequency and a zero that tracks it "
         "restores the slope. Nothing in any source states a pole-zero minimum "
         "separation, a placement rule, or a section ordering law.",
         quote="Digital filters, however, operate at a fixed sample rate and as a "
               "result do not approach an asymptotic rolloff of 6dB/octave per "
               "pole at the high end of the audio band. Instead, due to the fact "
               "that the rolloff is akin to a cosine function in frequency, the "
               "rolloff of a pole levels out at high frequency. This, fortunately, "
               "is easily compensated using a tracking zero (see figure 2a and 2b).",
         cite="Rossum, Making Digital Filters Sound \"Analog\", ICMC 1992, p.31 "
              "sec.3 (ref/inputs/making_digital_filters_sound_analog.pdf)"),
    Fact("PLACEMENT_NOT_CRITICAL", None, "", "SOURCED",
         "the whole placement thesis; the case against a solver",
         "the machine's designer, on the thing the optimiser was built to worry "
         "about.",
         quote="Interestingly, our research has shown that the spectral behavior "
               "of filters is not tremendously important. While the difference "
               "between a 2nd order and 4th order rolloff is fairly audible, the "
               "precise relative placement of the poles seems to be not very "
               "critical.",
         cite="Rossum, ICMC 1992, p.31 sec.3"),
    Fact("SATURATE_INTO_DELAYS", None, "", "SOURCED",
         "trench-core/src/cascade.rs",
         "the nonlinearity: extra headroom on the accumulator, saturate only the "
         "value on its way into the delays, output tapped off the accumulator "
         "BEFORE the saturate (figure 3). Per second-order section. No detector.",
         quote="If we add some headroom to the accumulator portion of the filter, "
               "and instead saturate only when delaying the signal prior to the "
               "multiplier inputs, we have caused the saturation to occur at a "
               "point equivalent to the input of the filter (see figure 3). ... "
               "it is obvious that the onset of distortion is equivalent to "
               "changing the coefficients of the filter.",
         cite="Rossum, ICMC 1992, pp.32-33 sec.5 and figure 3"),
    Fact("COEFF_UPDATE_RATE", None, "", "SOURCED",
         "trench-core/src/engine.rs per-sample morph",
         "why the morph is evaluated per sample and not per block.",
         quote="To band limit the coefficient changes,the coefficients must be "
               "updated at the sample rate. To update them at any lower rate will "
               "cause audio images of the update rate to appear in the signal "
               "path, which may be somewhat attenuated by filtering.",
         cite="Rossum, ICMC 1992, p.32 sec.4"),
    Fact("CONTAINED_SECTION_PCT", 36.7, "percent of sections", "MEASURED",
         "what a section is FOR — the two-letter distinction",
         "zero within 6 semitones of its own pole. This is the minimum-output "
         "configuration: with the zero exactly on the pole a section makes "
         "+7.9 dB, against +41.6 dB with the zero two octaves above. It makes a "
         "local feature and leaves the rest of the band alone. A uniform spread "
         "would put ~8% in any 12-semitone window, so this is a 4.6x spike — "
         "the offsets are two populations, not a continuum.",
         lambda: _zero_offset_share(0.0, 6.0), tol=1.5, sample="739 sections"),
    Fact("SPANNING_SECTION_PCT", 41.7, "percent of sections", "MEASURED",
         "what a section is FOR — the two-letter distinction",
         "zero more than 18 semitones from its own pole. The skirt between the "
         "two roots becomes broadband tilt, and the section carries a feature "
         "AND a slope: zero below the pole makes it rise, zero above makes it "
         "fall. This is how a body gets its envelope without spending a section "
         "on tone control — see SECTION_TILT_S1_DB. Talking Hedz spells its two "
         "ends this way at -57 and +60 st while its three voice sections sit "
         "contained at +4.",
         lambda: _zero_offset_share(18.0, 999.0), tol=1.5, sample="739 sections"),
    Fact("STACK_EXCESS_DB", 14.6, "dB delivered above what the section asked",
         "MEASURED", "how to place a section and get the level you authored",
         "when the nearest OTHER pole in the frame is under 3 semitones away, "
         "the two skirts stack and the feature arrives 14.6 dB louder than the "
         "section makes on its own. Only 28% land within 6 dB of intent. Below "
         "3 st you are no longer authoring two sections, you are authoring one "
         "resonance whose height you do not set directly — the SPACING sets it.",
         lambda: _delivery(0.0, 3.0, "excess"), tol=1.5, sample="136 poles"),
    Fact("INDEPENDENT_DELIVERY_PCT", 78.7,
         "% landing within 6 dB when the nearest pole is 12+ semitones away",
         "MEASURED", "how to place a section and get the level you authored",
         "past 12 semitones apart, sections stop fighting: 78.7% of features "
         "arrive within 6 dB of what the section asked for, against 28% when "
         "the nearest pole is under 3 st, and the median excess falls to about "
         "-1 dB. Distance to the nearest ZERO barely matters by comparison — "
         "every zero bucket sits between +0.1 and +1.0 dB median. Pole SPACING "
         "is the level control, not the SCALE word.",
         lambda: _delivery(12.0, 99.0, "within"), tol=2.0, sample="122 poles"),
    Fact("SCALE_UNIFORM_PCT", 79.5, "percent of corners", "MEASURED",
         "SCALE handling in the editor",
         "one level per corner is a TENDENCY, not a law. 79.5% of character "
         "corners carry the same SCALE word in all six sections; 20.5% do not. "
         "No source states a law. US5170369 and ARMAdillo both give each section "
         "its own separated gain a0, so per-section level is the format's native "
         "shape and the editor must not collapse it.",
         scale_uniform_pct, tol=1.5, sample="132 corners",
         quote="While dealing with poles and feedback (bn) coefficients, the "
               "comments herein apply as well to zeroes and feedforward "
               "coefficients (an/a0) when the gain (a0) is separated as shown above.",
         cite="Rossum 1991, ARMAdillo, p.1"),
    Fact("NONLINEARITY_MATCHES_SOURCE", 0, "1 = agrees, 0 = recorded disagreement",
         "SOURCED", "trench-core/src/cascade.rs process_sample",
         "AGREES: output tapped off the accumulator before the saturate; only "
         "the value entering the delays is saturated; per section. DISAGREES: "
         "cascade.rs adds an explicit pole-radius modulator with a threshold vt "
         "and a stored y_prev. Rossum has no such step — the pole movement is a "
         "consequence of the saturation, not a second operation. It also runs "
         "the wrong way: he describes a shift in the PITCH of the resonance from "
         "a coefficient being REDUCED; ours holds cos(theta) fixed and RAISES R. "
         "Not changed — it shipped and the ears passed it. Recorded, not fixed.",
         quote="The cause of this behavior can be understood by considering the "
               "consequence of saturating the inputs to the multipliers. When "
               "this occurs, one could either say that the signal had been "
               "saturated (thus producing a smaller product at the output of the "
               "multiplier), or alternatively that the coefficient had been "
               "reduced in such a manner as to give the same smaller product.",
         cite="Rossum, ICMC 1992, pp.32-33 sec.5"),
    Fact("US5952599_RELEVANT", 0, "1 = relevant", "SOURCED",
         "nothing — do not open this file again",
         "read 2026-08-13. Interval Research, not E-mu. Mouse-gesture music "
         "generation for non-musicians: graphic objects that roll, spin and fly "
         "while a musical segment tracks them. No filter, no pole, no cascade, no "
         "z-plane anywhere in the document. The only tie to our sources is that "
         "William Martens is a co-inventor.",
         quote="INTERACTIVE MUSIC GENERATION SYSTEM MAKING USE OF GLOBAL FEATURE "
               "CONTROL BY NON-MUSICANS",
         cite="US5952599 title (ref/patents/US5952599_Dolby_et_al.pdf)"),
]


def main() -> int:
    only = sys.argv[1] if len(sys.argv) > 1 else None
    drift, unsourced, sourced = [], [], []
    print(f"{'fact':28s} {'in use':>10s} {'re-derived':>11s} {'kind':>10s}  note")
    for fct in FACTS:
        if only and only.lower() not in fct.name.lower():
            continue
        got = ""
        if fct.derive is not None:
            v = fct.derive()
            got = f"{v:11.4f}"
            if abs(v - fct.value) > fct.tol:
                drift.append((fct.name, fct.value, v))
        if fct.kind == "UNSOURCED":
            unsourced.append(fct.name)
        if fct.kind == "SOURCED":
            sourced.append(fct)
        val = "         —" if fct.value is None else f"{fct.value:10.4f}"
        print(f"{fct.name:28s} {val} {got:>11s} {fct.kind:>10s}  {fct.note[:56]}")

    if sourced:
        print(f"\nSOURCED ({len(sourced)}) — verbatim, with its location:")
        for f in sourced:
            print(f"\n  {f.name}\n    {f.cite}")
            for line in textwrap.wrap(f.quote, 74):
                print(f"      > {line}")

    print()
    if drift:
        print("DRIFT — the corpus no longer supports these:")
        for n, want, got in drift:
            print(f"  {n}: recorded {want}, measured {got:.4f}")
    else:
        print("no drift: every MEASURED fact still matches the corpus")
    if unsourced:
        print(f"\nUNSOURCED ({len(unsourced)}) — numbers with no measurement behind them:")
        for n in unsourced:
            print(f"  {n}")
        print("  house rule: never fill a data gap with a number.")
    return 1 if drift else 0


if __name__ == "__main__":
    raise SystemExit(main())
