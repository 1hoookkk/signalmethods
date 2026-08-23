import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TAU = 2.0 * math.pi
SR = 44100.0


def decode_word(word):
    u = word + 1
    if u >= 65536:
        return 1.0
    if u <= 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m + 4096.0) / 8192.0
    return x * 2.0 ** (e - 15)


def conjugate_pole(d_mag, d_rsq):
    q = 1.0 - d_rsq
    p = 4.0 * d_mag + d_rsq - 2.0
    if p == 0.0 and q == 0.0:
        return None
    if p * p - 4.0 * q >= 0.0:
        return None
    r = math.sqrt(q)
    c = max(-1.0, min(1.0, -p / (2.0 * r)))
    return (math.acos(c) / TAU * SR, r)


def body_poles(name, corner):
    data = (ROOT / "ref" / "presets" / (name + ".bin")).read_bytes()
    words = struct.unpack("<120H", data)
    out = []
    for section in range(6):
        base = (corner * 6 + section) * 5
        pole = conjugate_pole(decode_word(words[base + 2]), decode_word(words[base + 3]))
        if pole is None:
            continue
        hz, r = pole
        if r <= 0.95 or hz < 150.0 or hz > 9000.0:
            continue
        out.append((hz, -math.log(r) * SR / math.pi))
    out.sort()
    return out


def skeleton_postures():
    types = json.loads((ROOT / "ref" / "mophatt_filter_types.json").read_text())
    by_p2k = {f["p2k"]: f["type"] for f in types["filters"] if "p2k" in f}
    lines = (ROOT / "dev" / "pca" / "skeleton_library.txt").read_text().splitlines()
    out = []
    pending = False
    for line in lines:
        if line.startswith("posture "):
            pending = True
            continue
        if not pending or not line.strip():
            continue
        pending = False
        body, corner = line.split()
        stem = "P2k_" + body
        out.append((by_p2k[stem] + " " + body[4:] + " " + corner, by_p2k[stem],
                    body_poles(stem, int(corner[1:]))))
    return out


def vowel_postures():
    classes = json.loads((ROOT / "ref" / "x3_vocal_classes.json").read_text())
    corners = ["M0Q0", "M100Q0", "M0Q100", "M100Q100"]
    out = []
    for name, entry in classes["classes"].items():
        label = name.split("_", 1)[1]
        for corner in entry["44100"]:
            poles = sorted((row["pole_hz"], row["bw_hz"]) for row in corner["rows"])
            out.append(("VOW " + label + " " + corners[corner["corner"]], "VOW", poles))
    return out


def emit(postures):
    body = []
    body.append('#include "trench/core/formants.hpp"')
    body.append("")
    body.append("namespace trench::core::p2k {")
    body.append("")
    body.append("namespace {")
    body.append("")
    body.append("constexpr std::array<Posture, %d> kPostures{{" % len(postures))
    for name, type_code, poles in postures:
        cells = ", ".join("{%.4f, %.4f}" % (hz, bw) for hz, bw in poles)
        body.append('    {"%s", "%s", %d, {{%s}}},' % (name, type_code, len(poles), cells))
    body.append("}};")
    body.append("")
    body.append("}  // namespace")
    body.append("")
    body.append("std::span<const Posture> postures() { return kPostures; }")
    body.append("")
    body.append("const Posture* posture(std::string_view name) {")
    body.append("  for (const auto& p : kPostures) {")
    body.append("    if (p.name == name) return &p;")
    body.append("  }")
    body.append("  return nullptr;")
    body.append("}")
    body.append("")
    body.append("}  // namespace trench::core::p2k")
    body.append("")
    target = ROOT / "native" / "trench-core" / "src" / "p2k" / "postures.cpp"
    target.write_text("\n".join(body))
    return target


if __name__ == "__main__":
    table = skeleton_postures() + vowel_postures()
    path = emit(table)
    print(path, len(table))
    for name, type_code, poles in table:
        print(" ", name, type_code, len(poles))
