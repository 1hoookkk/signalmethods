import glob
import io
import json
import math
import os
import wave
import zipfile

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ZIP = os.path.join(ROOT, "factory-data", "morpheus", "rossum", "cubes_v1.01vc_170120.wav.zip")
MANUAL = os.path.join(ROOT, "factory-data", "morpheus", "decoded", "*", "*.json")
OUT_JSON = os.path.join(ROOT, "factory-data", "morpheus", "rossum", "rossum_cubes_v1.01.json")
OUT_TXT = os.path.join(os.path.dirname(__file__), "rossum_morpheus_cubes_decode.txt")

RECORD_BYTES = 332
RECORDS = 289
NAME_BYTES = 12
HEADER_BYTES = 24
ROW_BYTES = 44
ROWS = 7
PITCH_FIELDS = {1: (256, 10, 64.0, 0.29), 4: (267, 10, 64.0, 0.31), 0: (278, 10, 64.0, 0.28), 6: (289, 8, 16.0, 0.32), 2: (298, 10, 64.0, 0.28), 5: (309, 10, 64.0, 0.29), 7: (329, 10, 64.0, 0.29), 3: (340, 10, 64.0, 0.28)}


def demodulate(wav_bytes):
    w = wave.open(io.BytesIO(wav_bytes))
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).astype(float) - 128.0
    sgn = np.sign(x)
    for i in range(1, len(sgn)):
        if sgn[i] == 0:
            sgn[i] = sgn[i - 1]
    t = np.where(np.diff(sgn) != 0)[0] + 1
    intervals = np.diff(t)
    symbols = ["S" if v <= 6 else "L" for v in intervals]
    bits = []
    i = 0
    while i < len(symbols):
        if symbols[i] == "L":
            bits.append(0)
            i += 1
        elif i + 1 < len(symbols) and symbols[i + 1] == "S":
            bits.append(1)
            i += 2
        else:
            i += 1
    return bits, w.getframerate(), len(symbols)


def to_bytes_lsb_first(bits, bit_offset):
    out = bytearray()
    for i in range(bit_offset, len(bits) - 7, 8):
        v = 0
        for k in range(8):
            v |= bits[i + k] << k
        out.append(v)
    return bytes(out)


def field(record_bits, row, offset, width):
    base = HEADER_BYTES * 8 + row * ROW_BYTES * 8 + offset
    v = 0
    for k in range(width):
        v |= record_bits[base + k] << k
    return v


def pitch_hz(count, counts_per_octave, zero_hz):
    return zero_hz * 2.0 ** (count / counts_per_octave)


def main():
    z = zipfile.ZipFile(ZIP)
    wav_name = [n for n in z.namelist() if n.endswith(".wav") and not n.startswith("__MACOSX")][0]
    bits, rate, symbol_count = demodulate(z.read(wav_name))
    leader = 0
    while leader < len(bits) and bits[leader] == 1:
        leader += 1
    body = bits[leader:]
    start = 3 + 64
    records = []
    for r in range(RECORDS):
        rb = body[start + r * RECORD_BYTES * 8:start + (r + 1) * RECORD_BYTES * 8]
        raw = to_bytes_lsb_first(rb, 0)
        name = raw[:NAME_BYTES].decode("latin1").rstrip(" \x00")
        pitches = {}
        for row in range(ROWS - 1):
            sec = row + 2
            pitches[sec] = {}
            for c, (o, w, per_octave, zero_hz) in PITCH_FIELDS.items():
                count = field(rb, row, o, w)
                pitches[sec][c] = round(pitch_hz(count, per_octave, zero_hz), 1) if 0 < count < 2 ** w - 1 else None
        records.append({"index": r, "name": name, "raw_hex": raw.hex(), "pole_pitch_hz_by_section_1993_corner": pitches})
    manual = {}
    for p in glob.glob(MANUAL):
        d = json.load(open(p, encoding="utf-8"))
        manual[d["filter_number"]] = d
    def norm(s):
        return "".join(ch for ch in (s or "").lower() if ch.isalnum())
    exact = sum(1 for r in range(1, RECORDS) if (r - 1) in manual and norm(records[r]["name"]) == norm(manual[r - 1]["manual_name"]))
    errs = []
    per_corner = {}
    stored_total = 0
    for r in range(1, RECORDS):
        d = manual.get(r - 1)
        if not d:
            continue
        for c in d["corner_data"]:
            for s in c["sections"]:
                k = s["section"]
                if k < 2 or s["pole"]["kind"] != "conjugate" or s["pole"]["hz"] < 30:
                    continue
                got = records[r]["pole_pitch_hz_by_section_1993_corner"][k][c["corner"]]
                stored_total += 1
                if got is not None:
                    errs.append(12.0 * math.log2(got / s["pole"]["hz"]))
                    per_corner.setdefault(c["corner"], []).append(abs(errs[-1]) < 3.0)
    errs = np.array(errs)
    within_half = float(np.mean(np.abs(errs) < 0.5)) if len(errs) else 0.0
    summary = {
        "wav": wav_name, "sample_rate": rate, "modulation": "6 kHz biphase, half cycle 4 samples = short, 8 = long; FM decode short-short = 1, long = 0; 6000-bit leader of ones",
        "symbols": symbol_count, "bits": len(bits), "leader_bits": leader,
        "bytes": "LSB-first, record grid starts 67 bits into the body, 289 records x 332 bytes, 0xFF padding after",
        "record": "12-byte name, 12 header bytes, 7 rows x 44 bytes (352 bits); rows 0..5 = sections 2..7 of the 1993 cube, row 6 unresolved",
        "pole_pitch_fields": "LSB-first note counts (offset, width, counts per octave, Hz at zero) per 1993 corner: " + json.dumps(PITCH_FIELDS),
        "names_exact_match_to_manual": exact, "names_total": RECORDS - 1,
        "pitch_error_semitones_vs_1993": {"1993_conjugate_poles_sections_2_to_7": int(stored_total), "stored_in_module": int(len(errs)), "median_abs": float(np.median(np.abs(errs))), "within_half_semitone": within_half},
        "within_3_semitones_by_1993_corner": {c: round(float(np.mean(v)), 3) for c, v in sorted(per_corner.items())},
        "unresolved": "bandwidth, gain, zero fields, section 1, header semantics, corners 4 and 6 fit worse (probably narrower fields)",
    }
    json.dump({"summary": summary, "records": records}, open(OUT_JSON, "w", encoding="utf-8"), indent=1)
    with open(OUT_TXT, "w", encoding="utf-8") as f:
        for k, v in summary.items():
            f.write("%s: %s\n" % (k, v))
        f.write("\nnames (index: rossum | 1993 manual):\n")
        for r in range(RECORDS):
            f.write("%3d: %-14s | %s\n" % (r, records[r]["name"], manual[r - 1]["manual_name"] if (r - 1) in manual else ""))
    for k, v in summary.items():
        print("%-36s %s" % (k, v))


if __name__ == "__main__":
    main()
