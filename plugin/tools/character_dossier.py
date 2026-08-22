from __future__ import annotations
import json, math, os, struct, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from pyruntime.packed_interp import (
    words_to_coeffs, kernel_to_biquad, coeffs_to_words)

ATLAS = r"C:\Users\hooki\df2\dev\tmp\rom_extract_verify_20260611_001341\presets"
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "dossiers", "characters")
SR = 39062.5
GRID = np.geomspace(20.0, 18000.0, 700)
CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]

MANUAL = {
    0:  ("Ace of Bass",  "EQ+", "Bass-boost to bass-cut morph"),
    1:  ("MegaSweepz",   "LPF", "'Loud' LPF with a hard Q. Tweeters beware!"),
    2:  ("EarlyRizer",   "LPF", "Classic analog sweeping with hot Q and low-end."),
    3:  ("Millennium",   "LPF", "Aggressive low-pass filter. Q gives you a variety of spiky tonal peaks."),
    4:  ("MeatyGizmo",   "REZ", "Filter inverts at mid-Q."),
    5:  ("KlubKlassik",  "LPF", "Responsive low-pass filter sweep with a wide spectrum of Q sounds"),
    6:  ("BassBox-303",  "LPF", "Pumped up lows with TB-like squelchy Q factor."),
    7:  ("FuzziFace",    "DST", "Nasty clipped distortion. Q functions as mid-frequency tone control."),
    8:  ("DeadRinger",   "REZ", "Permanent 'Ringy' Q response. Many Q variations."),
    9:  ("TB-OrNot-TB",  "EQ+", "Great Bassline 'Processor.'"),
    10: ("Ooh-To-Eee",   "VOW", "Oooh to Eeee formant morph."),
    11: ("BolandBass",   "EQ+", "Constant bass boost with mid-tone Q control."),
    12: ("MultiQVox",    "VOW", "Multi-Formant, Map Q To velocity."),
    13: ("TalkingHedz",  "VOW", "'Oui' morphing filter. Q adds peaks."),
    14: ("ZoomPeaks",    "REZ", "High resonance nasal filter."),
    15: ("DJAlkaline",   "EQ+", "Band accentuating filter, Q shifts 'ring' frequency."),
    16: ("BassTracer",   "EQ+", "Low Q boosts bass. Try sawtooth or square waveform with Q set to 115."),
    17: ("RogueHertz",   "EQ+", "Bass with mid-range boost and smooth Q. Sweep cutoff with Q at 127."),
    18: ("RazorBlades",  "EQ-", "Cuts a series of frequency bands. Q selects different bands."),
    19: ("RadioCraze",   "EQ-", "Band limited for a cheap radio-like EQ"),
    20: ("Eeh-To-Aah",   "VOW", "'E' to 'Ah' formant movement. Q accentuates 'peakiness.'"),
    21: ("UbuOrator",    "VOW", "Aah-Uuh vowel with no Q. Raise Q for throaty vocals."),
    22: ("DeepBouche",   "VOW", "French vowels! 'Ou-Est' vowel at low Q."),
    23: ("FreakShifta",  "PHA", "Phasey movement. Try major 6 interval and maximum Q."),
    24: ("CruzPusher",   "PHA", "Accentuates harmonics at high Q. Try it with a sawtooth LFO."),
    25: ("AngelzHairz",  "FLG", "Smooth sweep flanger. Good with vox waves. e.g. I094, Q=60"),
    26: ("DreamWeava",   "FLG", "Directional Flanger. Poles shift down at low Q and up at high Q."),
    27: ("AcidRavage",   "REZ", "Great analog Q response. Wide tonal range. Try it with a sawtooth LFO."),
    28: ("BassOMatic",   "REZ", "Low boost for basslines. Q goes to distortion at the maximum level."),
    29: ("LucifersQ",    "REZ", "Violent mid Q filter! Take care with Q values 40-90."),
    30: ("ToothComb",    "REZ", "Highly resonant harmonic peaks shift in unison. Try mid Q."),
    31: ("EarBender",    "WAH", "Midway between wah & vowel. Strong mid-boost. Nasty at high Q settings."),
    32: ("KlangKling",   "SFX", "Ringing Flange filter. Q 'tunes' the ring frequency."),
}

NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

def note_name(hz: float) -> str:
    if hz <= 0:
        return "-"
    n = 12.0 * math.log2(hz / 440.0) + 69.0
    k = int(round(n))
    cents = int(round((n - k) * 100))
    return f"{NOTES[k % 12]}{k // 12 - 1}{cents:+d}c"

def quad_roots(b: float, c: float):
    disc = b * b - 4.0 * c
    if disc >= 0.0:
        s = math.sqrt(disc)
        return complex((-b + s) / 2.0, 0.0), complex((-b - s) / 2.0, 0.0)
    s = math.sqrt(-disc)
    return complex(-b / 2.0, s / 2.0), complex(-b / 2.0, -s / 2.0)

def root_row(r: complex):
    hz = abs(math.atan2(abs(r.imag), r.real)) / (2.0 * math.pi) * SR
    return {"hz": round(hz, 2), "radius": round(abs(r), 6),
            "note": note_name(hz), "real_pair": r.imag == 0.0}

def section_row(words):
    c = words_to_coeffs(tuple(words))
    b0, b1, b2, a1, a2 = kernel_to_biquad(c)
    zr = quad_roots(b1 / b0, b2 / b0) if b0 != 0 else (0j, 0j)
    pr = quad_roots(a1, a2)
    return {
        "words": list(words),
        "scale_db": round(20.0 * math.log10(abs(b0)) if b0 else -120.0, 3),
        "zero": root_row(zr[0]), "zero2": root_row(zr[1]),
        "pole": root_row(pr[0]), "pole2": root_row(pr[1]),
        "biquad": [b0, b1, b2, a1, a2],
    }

def cascade_db(rows):
    z = np.exp(-1j * 2.0 * np.pi * GRID / SR)
    h = np.ones_like(z)
    for r in rows:
        b0, b1, b2, a1, a2 = r
        h = h * (b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z)
    return 20.0 * np.log10(np.abs(h) + 1e-12)

def reconstruct_words(sec):
    scale = 10.0 ** (sec["scale_db"] / 20.0)
    def pair(a, b):
        ra, rb = a["radius"], b["radius"]
        if a["real_pair"]:
            th_a = 0.0 if a["hz"] < SR / 4 else math.pi
            th_b = 0.0 if b["hz"] < SR / 4 else math.pi
            s = ra * math.cos(th_a) + rb * math.cos(th_b)
            p = ra * math.cos(th_a) * rb * math.cos(th_b)
            return -s, p
        th = 2.0 * math.pi * a["hz"] / SR
        return -2.0 * ra * math.cos(th), ra * ra
    zb, zc = pair(sec["zero"], sec["zero2"])
    pb, pc = pair(sec["pole"], sec["pole2"])
    b0, b1, b2 = scale, scale * zb, scale * zc
    a1, a2 = pb, pc
    c4 = b0
    c0 = b1 / c4 + 2.0 if c4 else 2.0
    c1 = 1.0 - (b2 / c4 if c4 else 0.0)
    c2 = a1 + 2.0
    c3 = 1.0 - a2
    return coeffs_to_words(c0, c1, c2, c3, c4)

def travel_st(hz0, hz1):
    if hz0 <= 0 or hz1 <= 0:
        return None
    return round(12.0 * math.log2(hz1 / hz0), 2)

def main():
    os.makedirs(OUT, exist_ok=True)
    summary = []
    for idx in sorted(MANUAL):
        name, ftype, desc = MANUAL[idx]
        matches = [f for f in os.listdir(ATLAS) if f.startswith(f"P2k_{idx:03d}_")]
        raw = open(os.path.join(ATLAS, matches[0]), "rb").read()
        w = struct.unpack("<120H", raw)
        corners = {}
        for ci, label in enumerate(CORNERS):
            rows = [section_row(w[ci * 30 + s * 5: ci * 30 + s * 5 + 5])
                    for s in range(6)]
            corners[label] = rows

        null = {}
        for label, rows in corners.items():
            ref = cascade_db([r["biquad"] for r in rows])
            rec = cascade_db([kernel_to_biquad(words_to_coeffs(
                reconstruct_words(r))) for r in rows])
            d = np.abs(ref - rec)
            null[label] = {"rms_db": round(float(np.sqrt((d * d).mean())), 4),
                           "max_db": round(float(d.max()), 4)}

        travels = [travel_st(corners["M0_Q0"][s]["pole"]["hz"],
                             corners["M100_Q0"][s]["pole"]["hz"])
                   for s in range(6)]
        q_lift = [round(corners["M0_Q100"][s]["pole"]["radius"]
                        - corners["M0_Q0"][s]["pole"]["radius"], 4)
                  for s in range(6)]

        dossier = {
            "schema": "trench-character-dossier-v1",
            "index": idx, "name": name, "x3_type": ftype,
            "x3_one_liner": desc, "datum_sr_hz": SR,
            "source": "rom_extract_verify_20260611_001341 (verified vs rom-decoded json + candidates)",
            "corners": {k: [{kk: vv for kk, vv in r.items() if kk != "biquad"}
                            for r in rows] for k, rows in corners.items()},
            "responses_db": {k: [round(float(x), 3) for x in
                                 cascade_db([r["biquad"] for r in rows])]
                             for k, rows in corners.items()},
            "grid_hz": [round(float(g), 2) for g in GRID],
            "pole_travel_semitones_M0_to_M100_at_Q0": travels,
            "pole_radius_lift_Q0_to_Q100_at_M0": q_lift,
            "null": null,
        }
        fn = os.path.join(OUT, f"P2k_{idx:03d}_{name.replace('/', '_')}.json")
        json.dump(dossier, open(fn, "w"), indent=1)
        worst = max(v["max_db"] for v in null.values())
        summary.append((idx, name, ftype, worst))
        print(f"P2k_{idx:03d} {name:14s} {ftype:4s} null max {worst:7.4f} dB")

    bad = [s for s in summary if s[3] > 0.5]
    print(f"\n{len(summary)} dossiers -> {OUT}")
    print("NULL GATE:", "ALL PASS (<0.5 dB)" if not bad else f"FAIL: {bad}")

if __name__ == "__main__":
    main()
