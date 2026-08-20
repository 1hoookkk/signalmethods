"""Map the entire P2K corpus into a seedable stage bank at 44100 Hz.

For each preset × corner × stage: decode at 39062.5 Hz datum, resample
root geometry to 44100 Hz, classify, and emit as a seedable entry.

Vocal/formant stages get Klatt-method Hz+bandwidth.
Everything else gets raw pole-zero coordinates.
"""
import json
import math
import os
import struct
import glob

SRC_DIR = "ref/presets"
OUT_PATH = "ref/corpus_stage_bank.json"
DATUM_SR = 39062.5
TARGET_SR = 44100.0
CORNERS = ["M0Q0", "M1Q0", "M0Q1", "M1Q1"]

PRESET_NAMES = {
    "P2k_000_ace_of_bass": "Sub Resonance",
    "P2k_001_megasweepz": "Full Range Sweep",
    "P2k_002_early_rizer": "Early Rizer",
    "P2k_003_millennium": "Millennium Sweep",
    "P2k_004_meaty_gizmo": "Dense Resonance Ladder",
    "P2k_005_klub_klassik": "Club Classic Filter",
    "P2k_006_acid_ravage": "Acid Distortion Sweep",
    "P2k_007_fuzzi_face": "Fuzz Resonance",
    "P2k_008_cruz_pusher": "Drive Pusher",
    "P2k_009_tb_or_not_tb": "Acid Cavity Sweep",
    "P2k_010_ooh_to_eee": "Round to Bright Vowel",
    "P2k_011_boland_bass": "Analog Bass Shape",
    "P2k_012_multi_q_vox": "Multi-Q Vocal",
    "P2k_013_talking_hedz": "Vocal Morph",
    "P2k_014_zoom_peaks": "Sweeping Peaks",
    "P2k_015_dj_alkaline": "DJ Filter Sweep",
    "P2k_016_bass_tracer": "Bass Tracer Sweep",
    "P2k_017_rogue_hertz": "Rogue Resonance",
    "P2k_018_razor_blades": "Razor Edge Sweep",
    "P2k_019_radio_craze": "Radio Band Filter",
    "P2k_020_eeh_to_aah": "Front to Open Vowel",
    "P2k_021_ubu_orator": "Deep Orator",
    "P2k_022_deep_bouche": "Deep Chamber Voice",
    "P2k_028_lucifers_q": "Extreme Q Ladder",
    "P2k_029_lucifer_s_q": "Extreme Q Ladder",
    "P2k_029_ear_bender": "Ear Bender Phaser",
    "P2k_030_tooth_comb": "Comb Tooth Sweep",
    "P2k_031_dead_ringer": "Bell Ring Decay",
    "P2k_031_ear_bender": "Ear Bender Phaser",
    "P2k_032_klang_kling": "Metallic Clang",
}


def decode_u16(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))


def kernel_biquad(words):
    d = [decode_u16(x) for x in words]
    k = (4.0 * d[0] + d[1], d[1], 4.0 * d[2] + d[3], d[3], 4.0 * d[4])
    return (k[4], (k[0] - 2.0) * k[4], (1.0 - k[1]) * k[4]), (1.0, k[2] - 2.0, 1.0 - k[3])


def roots_from_biquad(b, a, sr):
    a1, a2 = a[1], a[2]
    disc_p = a1 * a1 - 4 * a2
    if disc_p < 0:
        pole_r = math.sqrt(max(0, a2))
        if pole_r > 0:
            pole_hz = math.acos(max(-1, min(1, -a1 / (2 * pole_r)))) * sr / (2 * math.pi)
        else:
            pole_hz = 0
    else:
        r1 = abs((-a1 + math.sqrt(disc_p)) / 2)
        r2 = abs((-a1 - math.sqrt(disc_p)) / 2)
        pole_r = max(r1, r2)
        pole_hz = 0

    b0 = max(abs(b[0]), 1e-12)
    b1n, b2n = b[1] / b0, b[2] / b0
    disc_z = b1n * b1n - 4 * b2n
    if disc_z < 0:
        zero_r = math.sqrt(max(0, b2n))
        if zero_r > 0:
            zero_hz = math.acos(max(-1, min(1, -b1n / (2 * zero_r)))) * sr / (2 * math.pi)
        else:
            zero_hz = 0
    else:
        r1 = abs((-b1n + math.sqrt(disc_z)) / 2)
        r2 = abs((-b1n - math.sqrt(disc_z)) / 2)
        zero_r = max(r1, r2)
        zero_hz = 0

    return pole_hz, pole_r, zero_hz, zero_r, b[0]


def resample_radius(r, from_sr, to_sr):
    if r <= 0 or r >= 1:
        return r
    bw = -math.log(r) * from_sr / math.pi
    return math.exp(-math.pi * bw / to_sr)


def r_to_bw(r, sr):
    if r <= 0 or r >= 1:
        return 0
    return -math.log(r) * sr / math.pi


def is_identity(words):
    return all(w in (0xDFFF, 0xFFFF) for w in words)


def is_formant_like(pole_hz, pole_r, zero_hz, zero_r):
    if pole_r < 0.9 or pole_hz < 150:
        return False
    if zero_r > 0.99 and zero_hz > 0:
        return False
    if pole_hz > 6000:
        return False
    return True


def process_body(data, preset_name, corpus, n_corners, n_stages, bank, seen):
    corner_names = CORNERS if n_corners == 4 else [f"C{i}" for i in range(8)]
    bytes_per_corner = n_stages * 10
    for ci in range(n_corners):
        base = ci * bytes_per_corner
        for si in range(n_stages):
            words = []
            for wi in range(5):
                off = base + si * 10 + wi * 2
                w = struct.unpack_from("<H", data, off)[0]
                words.append(w)

            if is_identity(words):
                continue

            b, a = kernel_biquad(words)
            ph, pr, zh, zr, scale = roots_from_biquad(b, a, DATUM_SR)

            ph_44 = min(ph, TARGET_SR * 0.49)
            pr_44 = resample_radius(pr, DATUM_SR, TARGET_SR)
            zh_44 = min(zh, TARGET_SR * 0.49)
            zr_44 = resample_radius(zr, DATUM_SR, TARGET_SR)

            key = f"{ph_44:.1f}|{pr_44:.4f}|{zh_44:.1f}|{zr_44:.4f}"
            if key in seen:
                continue
            seen.add(key)

            entry = {
                "source": preset_name,
                "corpus": corpus,
                "corner": corner_names[ci] if ci < len(corner_names) else f"C{ci}",
                "stage": si + 1,
                "pole_hz": round(ph_44, 1),
                "pole_r": round(pr_44, 5),
                "zero_hz": round(zh_44, 1),
                "zero_r": round(zr_44, 5),
                "scale": round(scale, 6),
            }

            if is_formant_like(ph, pr, zh, zr):
                entry["method"] = "klatt"
                entry["pole_bw_hz"] = round(r_to_bw(pr, DATUM_SR), 1)
                if zr > 0 and zh > 0:
                    entry["zero_bw_hz"] = round(r_to_bw(zr, DATUM_SR), 1)
            else:
                entry["method"] = "direct"

            if abs(zr - 1.0) < 0.001:
                entry["null"] = True

            bank.append(entry)


def main():
    bank = []
    seen = set()

    p2k_files = sorted(glob.glob(os.path.join(SRC_DIR, "P2k_*.bin")))
    for path in p2k_files:
        data = open(path, "rb").read()
        if len(data) != 240:
            continue
        basename = os.path.splitext(os.path.basename(path))[0]
        preset_name = PRESET_NAMES.get(basename, basename)
        process_body(data, preset_name, "P2K", 4, 6, bank, seen)

    morph_dir = "ref/morpheus/bodies"
    morph_files = sorted(glob.glob(os.path.join(morph_dir, "*.body")))
    for path in morph_files:
        data = open(path, "rb").read()
        basename = os.path.splitext(os.path.basename(path))[0]
        if len(data) == 560:
            process_body(data, basename, "Morpheus", 8, 7, bank, seen)
        elif len(data) == 240:
            process_body(data, basename, "Morpheus", 4, 6, bank, seen)

    bank.sort(key=lambda e: (e["pole_hz"], e["zero_hz"]))

    out = {
        "schema": "trench-corpus-stage-bank-v1",
        "datum_sr": DATUM_SR,
        "target_sr": TARGET_SR,
        "total_unique_stages": len(bank),
        "source_presets_p2k": len(p2k_files),
        "source_presets_morpheus": len(morph_files),
        "stages": bank,
    }
    with open(OUT_PATH, "w") as f:
        json.dump(out, f, indent=2)

    methods = {}
    corpora = {}
    for e in bank:
        m = e["method"]
        c = e["corpus"]
        methods[m] = methods.get(m, 0) + 1
        corpora[c] = corpora.get(c, 0) + 1

    print(f"Mapped {len(bank)} unique stages")
    print(f"  P2K: {corpora.get('P2K', 0)}  Morpheus: {corpora.get('Morpheus', 0)}")
    print(f"  klatt (formant): {methods.get('klatt', 0)}")
    print(f"  direct (pole-zero): {methods.get('direct', 0)}")
    print(f"  nulls: {sum(1 for e in bank if e.get('null'))}")
    print(f"-> {OUT_PATH}")


if __name__ == "__main__":
    main()
