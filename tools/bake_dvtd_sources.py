import argparse
import json
import os

FORMANTS = 5


def hz_of(note):
    return 440.0 * 2.0 ** ((note - 69.0) / 12.0)


def bandwidth_hz(hz, width):
    return hz * (2.0 ** (width / 12.0) - 1.0)


def load(path):
    doc = json.load(open(path, encoding="utf-8"))
    rows = []
    for chord in doc["chords"]:
        stages = chord["stages"]
        if len(stages) < FORMANTS + 1:
            continue
        poles, zeros = [], []
        for stage in stages[:FORMANTS]:
            pole, zero = stage.get("pole"), stage.get("zero")
            if not pole or not zero:
                break
            hz = hz_of(pole["note"])
            poles.append((hz, bandwidth_hz(hz, pole["width"])))
            zeros.append(bandwidth_hz(hz_of(zero["note"]), zero["width"]))
        if len(poles) != FORMANTS:
            continue
        shelf = stages[FORMANTS].get("pole")
        rows.append((chord["name"], poles, zeros, hz_of(shelf["note"]) if shelf else 0.0))
    return rows


def emit(rows):
    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <array>")
    lines.append("")
    lines.append("namespace headspace {")
    lines.append("")
    lines.append("struct DvtdEntry {")
    lines.append("    const char* name;")
    lines.append("    std::array<std::array<double, 2>, 5> poles;")
    lines.append("    std::array<double, 5> zeroWidthHz;")
    lines.append("    double shelfHz;")
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr std::array<DvtdEntry, %d> kDvtdChords{{" % len(rows))
    for name, poles, zeros, shelf in rows:
        body = ", ".join("{%.4f, %.4f}" % (hz, bw) for hz, bw in poles)
        widths = ", ".join("%.4f" % z for z in zeros)
        lines.append('    {"%s", {{%s}}, {{%s}}, %.4f},' % (name, body, widths, shelf))
    lines.append("}};")
    lines.append("")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    args = ap.parse_args()
    rows = load(args.input)
    text = emit(rows)
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    open(args.output, "w", encoding="utf-8", newline="\n").write(text)
    print("wrote %s  %d chords" % (args.output, len(rows)))


if __name__ == "__main__":
    main()
