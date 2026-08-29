import csv
import json
import math
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SECTIONS = os.path.join(REPO, "evidence", "research-results", "corpus", "sections.tsv")
ARCHETYPES = os.path.join(REPO, "recipes", "tables", "synth_filter_archetypes.json")
HEADER = os.path.join(REPO, "native", "app", "corpus_shelf.hpp")

GROUP_ORDER = ["REZ", "VOW", "EQ+", "EQ-", "LPF", "PHA", "FLG", "DST", "WAH", "SFX"]
CORNER_SUFFIX = [" M0Q0", " M100Q0", " M0Q100", " M100Q100"]
ABSENT = (22050.0, 1000000000.0, False)


def bandwidth(radius, rate):
    return -rate * math.log(radius) / math.pi


def display(body):
    text = body
    if text.startswith("P2k_"):
        text = text[4:]
        cut = text.find("_")
        if cut >= 0:
            text = text[cut + 1:]
    return text.replace("_", " ")


def body_number(body):
    digits = body[4:7]
    return int(digits)


def read_corpus():
    bodies = {}
    dropped = 0
    with open(SECTIONS, newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            if row["source"] != "p2k":
                continue
            if row["variant"] != "0":
                continue
            if row["datum_hz"] != "44100":
                continue
            family = row["family"]
            if not family.endswith(" order 12"):
                continue
            group = family[:-len(" order 12")]
            corner = int(row["corner"])
            section = int(row["section"])
            if corner > 3 or section > 5:
                continue
            key = (group, row["body"])
            if key not in bodies:
                bodies[key] = [[list(ABSENT) for _ in range(6)] for _ in range(8)]
            slots = bodies[key]
            if row["pole_kind"] == "conjugate":
                radius = float(row["pole_r"])
                if radius >= 0.45:
                    slots[corner][section] = [float(row["pole_hz"]),
                                              bandwidth(radius, 44100.0), True]
            if row["zero_kind"] == "conjugate":
                radius = float(row["zero_r"])
                if radius >= 0.45:
                    slots[4 + corner][section] = [float(row["zero_hz"]),
                                                  bandwidth(radius, 44100.0), True]
                else:
                    dropped += 1
    entries = []
    for group in GROUP_ORDER:
        names = sorted((key[1] for key in bodies if key[0] == group), key=body_number)
        for body in names:
            slots = bodies[(group, body)]
            for corner in range(4):
                entries.append((group, display(body) + CORNER_SUFFIX[corner],
                                slots[corner], slots[4 + corner]))
    return entries, dropped


def read_archetypes():
    with open(ARCHETYPES, encoding="utf-8") as stream:
        table = json.load(stream)
    rate = float(table["sampling_rate_hz"])
    entries = []
    for archetype in table["archetypes"].values():
        poles = [list(ABSENT) for _ in range(6)]
        zeros = [list(ABSENT) for _ in range(6)]
        for index, stage in enumerate(archetype["stages"][:6]):
            radius = float(stage["pole_r"])
            if radius >= 0.45:
                poles[index] = [float(stage["pole_hz"]), bandwidth(radius, rate), True]
            radius = float(stage["zero_r"])
            if radius >= 0.45:
                zeros[index] = [float(stage["zero_hz"]), bandwidth(radius, rate), True]
        entries.append(("ARCHETYPES", archetype["name"], poles, zeros))
    return entries


def slot_text(slot):
    hz = slot[0] if slot[0] != 0.0 else 0.0
    bw = slot[1] if slot[1] != 0.0 else 0.0
    return "{%.2f, %.2f, %s}" % (hz, bw, "true" if slot[2] else "false")


def row_text(slots):
    return "{{" + ", ".join(slot_text(slot) for slot in slots) + "}}"


def main():
    entries, dropped = read_corpus()
    entries += read_archetypes()
    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "template_shelf.hpp"')
    lines.append("")
    lines.append("#include <array>")
    lines.append("")
    lines.append("namespace trench::app {")
    lines.append("")
    lines.append("struct CorpusEntry {")
    lines.append("  const char* group;")
    lines.append("  const char* name;")
    lines.append("  std::array<TemplatePole, 6> poles;")
    lines.append("  std::array<TemplatePole, 6> zeros;")
    lines.append("};")
    lines.append("")
    lines.append("inline const std::array<CorpusEntry, %d> kCorpusShelf{{" % len(entries))
    for group, name, poles, zeros in entries:
        lines.append('    {"%s", "%s", %s, %s},' % (group, name, row_text(poles), row_text(zeros)))
    lines.append("}};")
    lines.append("")
    lines.append("}")
    lines.append("")
    with open(HEADER, "w", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(lines))
    counts = {}
    for entry in entries:
        counts[entry[0]] = counts.get(entry[0], 0) + 1
    for group in GROUP_ORDER + ["ARCHETYPES"]:
        print(group, counts.get(group, 0))
    print("total", len(entries))
    print("zeros dropped", dropped)


main()
