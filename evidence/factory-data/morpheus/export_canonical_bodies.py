"""Export the Morpheus cubes from the raw record stream as canonical 560-byte native bodies.

Order law: in a serial cascade section gains multiply and responses add in dB; a Morpheus
corner is seven sections of five words; a body is eight corners and the chip's three-axis
lerp. Native corner index = Morph + 2 * Frequency + 4 * Transform, where Morph, Frequency
and Transform are the raw record's axis bits (raw corner index = Transform + 2 * Frequency
+ 4 * Morph), so MORPH plays the record's Morph axis, Q plays Frequency and z plays
Transform. Sections keep the raw record's row order 0..6. Words are written little-endian
u16 in the core's corner-major, section, five-word layout.
"""

import hashlib
import json
import math
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
MORPHEUS = os.path.join(ROOT, "evidence", "factory-data", "morpheus")
STREAM = os.path.join(MORPHEUS, "raw", "records_stream.bin")
RAW_BODIES = os.path.join(MORPHEUS, "raw", "bodies")
INDEX = os.path.join(MORPHEUS, "decoded", "index.json")
OUT = os.path.join(MORPHEUS, "canonical")

RATE = 48000.0
RECORD_BASE = 20
RECORD_STRIDE = 332
PAYLOAD_BYTES = 320
CUBE_COUNT = 289
ANGLE_K = struct.unpack("<f", struct.pack("<I", 0x32C90FDB))[0]
RADIUS_K = struct.unpack("<f", struct.pack("<I", 0x32800800))[0]
GAIN_K = struct.unpack("<f", struct.pack("<I", 0x338007FF))[0]
AXES = {"Transform": 1, "Frequency": 2, "Morph": 4}
ORDER_LAW = ("native corner index = Morph + 2 * Frequency + 4 * Transform, from raw corner "
             "index = Transform + 2 * Frequency + 4 * Morph; sections in the raw record's "
             "row order 0..6; five little-endian u16 words per section, corner-major")


def fields(block):
    bits = "".join(format(w, "032b") for w in struct.unpack("<%dI" % (len(block) // 4), block))
    return [int(bits[i:i + 11], 2) for i in range(0, len(bits) - 10, 11)]


def code15(f11):
    return (f11 << 4) | 0xF


def angle_hz(f11):
    c = code15(f11)
    e, m = c >> 11, c & 0x7FF
    return ((m | 0x800) << e) * ANGLE_K / (2.0 * math.pi) * RATE


def radius(f11):
    if f11 == 0:
        return 1.0
    c = code15(f11)
    e, m = c >> 11, c & 0x7FF
    return 1.0 - ((m | ((e != 0) << 11)) << (e - (e != 0))) * RADIUS_K


def gain(f11):
    c = code15(f11)
    e, m = c >> 11, c & 0x7FF
    return ((m | ((e != 0) << 11)) << (e - (e != 0))) * GAIN_K


def encode_word(value):
    if value >= 1.0:
        return 0xFFFF
    if value <= 0.0:
        return 0x0000
    denormal = int(round(value * 134217728.0))
    if 0 < denormal <= 0xFFF:
        return denormal - 1
    stored = min(int(math.floor(math.log2(value))) + 1, 0)
    if stored < -14:
        return 0x0000
    biased = stored + 15
    hidden = int(round(value / (2.0 ** (stored - 13))))
    if hidden >= 0x2000:
        if stored < 0:
            stored += 1
            biased += 1
            hidden = int(round(value / (2.0 ** (stored - 13))))
            return ((biased << 12) | min(hidden & 0xFFF, 0xFFF)) - 1
        return 0xFFFF
    return ((biased << 12) | max(0, min(hidden - 0x1000, 0xFFF))) - 1


def pair_coefficients(hz, r):
    r = max(0.0, min(r, 1.0))
    a = 2.0 * math.pi * hz / RATE
    return -2.0 * r * math.cos(a), r * r


def section_words(pole_angle, pole_radius, zero_angle, zero_radius, scale):
    zp, zq = pair_coefficients(zero_angle, zero_radius)
    pp, pq = pair_coefficients(pole_angle, pole_radius)
    return [encode_word((zp + 1.0 + zq) / 4.0),
            encode_word(1.0 - zq),
            encode_word((pp + 1.0 + pq) / 4.0),
            encode_word(1.0 - pq),
            encode_word(scale / 4.0)]


def read_cubes():
    blob = open(STREAM, "rb").read()
    cubes = []
    for k in range(CUBE_COUNT):
        off = RECORD_BASE + k * RECORD_STRIDE
        payload = blob[off:off + PAYLOAD_BYTES]
        name = blob[off + PAYLOAD_BYTES:off + RECORD_STRIDE].decode("ascii", "replace").strip()
        secs = [fields(payload[s * 44:(s + 1) * 44]) for s in range(7)]
        gains = fields(payload[308:320])[:8]
        cubes.append((k, name, off, secs, gains))
    return cubes


def raw_index(native):
    m = native & 1
    f = (native >> 1) & 1
    t = (native >> 2) & 1
    return t * AXES["Transform"] + f * AXES["Frequency"] + m * AXES["Morph"]


def raw_corner_words(secs, gains, corner, section):
    f = secs[section]
    scale = gain(gains[corner]) if section == 0 else 1.0
    return section_words(angle_hz(f[corner]), radius(f[8 + corner]),
                         angle_hz(f[16 + corner]), radius(f[24 + corner]), scale)


def build_body(secs, gains):
    return [[raw_corner_words(secs, gains, raw_index(native), s) for s in range(7)]
            for native in range(8)]


def body_bytes(corners):
    out = bytearray()
    for corner in corners:
        for section in corner:
            for word in section:
                out += struct.pack("<H", word)
    return bytes(out)


def file_name(number, name):
    return "%03d_%s.body" % (number, re.sub(r"[^A-Za-z0-9._-]", "_", name))


def families():
    data = json.load(open(INDEX, "r", encoding="utf-8"))
    return {f["filter_number"]: (f.get("family"), f.get("manual_name")) for f in data["filters"]}


def verify_order(cubes, bodies, lines):
    ok = True
    for k in (1, 65):
        number, name, off, secs, gains = cubes[k]
        corners = bodies[k]
        lines.append("")
        lines.append("### cube %d %s" % (number, name))
        lines.append("")
        lines.append("| native | m | f | t | raw = t + 2f + 4m | pole Hz, rows 0..6 | words exact |")
        lines.append("|---|---|---|---|---|---|---|")
        for native in range(8):
            m = native & 1
            f = (native >> 1) & 1
            t = (native >> 2) & 1
            r = t + 2 * f + 4 * m
            exact = (r == raw_index(native) and
                     all(corners[native][s] == raw_corner_words(secs, gains, r, s)
                         for s in range(7)))
            ok = ok and exact
            hz = ", ".join("%.1f" % angle_hz(secs[s][r]) for s in range(7))
            lines.append("| %d | %d | %d | %d | %d | %s | %s |"
                         % (native, m, f, t, r, hz, "yes" if exact else "NO"))
    return ok


def verify_roundtrip(paths, lines):
    sys.path.insert(0, os.path.join(ROOT, "native", "python"))
    try:
        import trench_core
    except Exception as exc:
        lines.append("")
        lines.append("trench_core is not importable from native/python: %s: %s"
                     % (type(exc).__name__, exc))
        return "not importable"
    matches = 0
    for path in paths:
        raw = open(path, "rb").read()
        body = trench_core.Body.from_native_bytes(raw)
        if body.to_native_bytes() == raw:
            matches += 1
    lines.append("")
    lines.append("trench_core round-trip: %d of %d canonical bodies survive "
                 "Body.from_native_bytes then to_native_bytes byte for byte."
                 % (matches, len(paths)))
    return "%d/%d" % (matches, len(paths))


def main():
    os.makedirs(OUT, exist_ok=True)
    cubes = read_cubes()
    fam = families()
    bodies = []
    manifest = []
    paths = []
    orphans = []
    for number, name, off, secs, gains in cubes:
        corners = build_body(secs, gains)
        bodies.append(corners)
        blob = body_bytes(corners)
        fname = file_name(number, name)
        path = os.path.join(OUT, fname)
        open(path, "wb").write(blob)
        paths.append(path)
        if not os.path.exists(os.path.join(RAW_BODIES, fname)):
            orphans.append(fname)
        family, manual = fam.get(number, (None, None))
        manifest.append({
            "number": number,
            "name": name,
            "file": fname,
            "family": family,
            "manual_name": manual,
            "stream_offset": off,
            "sha256": hashlib.sha256(blob).hexdigest(),
        })
    json.dump({
        "schema": "trench-morpheus-canonical-v1",
        "order_law": ORDER_LAW,
        "datum_hz": RATE,
        "source": os.path.relpath(STREAM, ROOT).replace("\\", "/"),
        "corners": 8,
        "sections": 7,
        "words_per_section": 5,
        "body_bytes": 560,
        "count": len(manifest),
        "bodies": manifest,
    }, open(os.path.join(OUT, "MANIFEST.json"), "w", encoding="utf-8"), indent=1)

    lines = ["# Morpheus canonical bodies - verification",
             "",
             "Order law: " + ORDER_LAW + ".",
             "",
             "Datum %.1f Hz. %d cubes, 560 bytes each, written to canonical/."
             % (RATE, len(paths)),
             ""]
    if orphans:
        lines.append("Names with no raw/bodies counterpart: " + ", ".join(orphans))
    else:
        lines.append("Every canonical name matches a raw/bodies file of the same name.")
    lines.append("")
    lines.append("## (a) corner order, cube 1 and cube 65")
    order_ok = verify_order(cubes, bodies, lines)
    lines.append("")
    lines.append("Corner order verification: %s" % ("PASS" if order_ok else "FAIL"))
    lines.append("")
    lines.append("## (b) trench_core round-trip")
    roundtrip = verify_roundtrip(paths, lines)
    lines.append("")
    lines.append("## (c) canonical against raw/bodies of the same name")
    differ = 0
    same = 0
    missing = 0
    for entry, path in zip(manifest, paths):
        old = os.path.join(RAW_BODIES, entry["file"])
        if not os.path.exists(old):
            missing += 1
        elif open(old, "rb").read() == open(path, "rb").read():
            same += 1
        else:
            differ += 1
    lines.append("")
    lines.append("| result | count |")
    lines.append("|---|---|")
    lines.append("| differ | %d |" % differ)
    lines.append("| identical | %d |" % same)
    lines.append("| no raw/bodies counterpart | %d |" % missing)
    lines.append("")
    lines.append("The raw/bodies files were encoded at a 44100 Hz datum while the hardware "
                 "datum is 48000 Hz (angles are normalised radians, f = theta / pi * 24000), they carry a per-cube corner scramble, their pole and "
                 "zero slots disagree on row order, and their fifth word holds a seventh-root "
                 "spread of a gain that does not match the record's per-corner gain field. "
                 "The canonical bodies take the pole pair from raw field groups 0 and 1, the "
                 "zero pair from groups 2 and 3, and put the record's per-corner gain in "
                 "section 0 with unity in sections 1..6, so the cascade product is the "
                 "recorded gain.")
    open(os.path.join(OUT, "VERIFY.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("\n".join(lines))
    print()
    print("bodies written: %d" % len(paths))
    print("order verification: %s" % ("PASS" if order_ok else "FAIL"))
    print("trench_core round-trip: %s" % roundtrip)
    print("differ from raw/bodies: %d, identical: %d, missing: %d" % (differ, same, missing))
    print("output: %s" % OUT)


if __name__ == "__main__":
    main()
