import collections
import glob
import itertools
import math
import os
import struct

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BODIES = os.path.join(REPO, "evidence", "factory-data", "p2k", "bodies")
HEADER = os.path.join(REPO, "native", "app", "mask_shelf.hpp")

RATE = 44100.0
CORNERS = ["M0Q0", "M100Q0", "M0Q100", "M100Q100"]
EXCLUDED = {"meaty_gizmo", "fuzzi_face"}
ABSENT = (22050.0, 1000000000.0, False)
SHARED_WORDS = 4


def decode_word(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    exponent = (u >> 12) & 0xF
    mantissa = float(u & 0xFFF)
    x = mantissa / 4096.0 if exponent == 0 else (mantissa + 4096.0) / 8192.0
    return math.ldexp(x, exponent - 15)


def pair_geometry(mag_word, rsq_word):
    q = 1.0 - decode_word(rsq_word)
    p = 4.0 * decode_word(mag_word) + decode_word(rsq_word) - 2.0
    disc = p * p - 4.0 * q
    if disc < 0.0:
        radius = math.sqrt(q)
        cosine = max(-1.0, min(1.0, -p / (2.0 * radius)))
        return math.acos(cosine) / (2.0 * math.pi) * RATE, radius
    return None


def bandwidth(radius):
    return -RATE * math.log(radius) / math.pi


def display(body):
    return body.replace("_", " ")


def read_bodies():
    corners = {}
    for path in sorted(glob.glob(os.path.join(BODIES, "P2k_0*.bin"))):
        body = os.path.basename(path)[8:-4]
        if body in EXCLUDED:
            continue
        data = open(path, "rb").read()
        words = struct.unpack("<120H", data)
        for corner in range(4):
            rows = []
            for section in range(6):
                base = (corner * 6 + section) * 5
                rows.append(tuple(words[base:base + 5]))
            corners[(body, corner)] = rows
    return corners


def overlap(a, b):
    return sum((collections.Counter(a) & collections.Counter(b)).values())


def zero_words(rows):
    return [row[:2] for row in rows[:5]]


def clusters(corners):
    keys = sorted(corners, key=lambda k: (k[0], k[1]))
    parent = {k: k for k in keys}

    def find(k):
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k

    for a, b in itertools.combinations(keys, 2):
        if overlap(zero_words(corners[a]), zero_words(corners[b])) >= SHARED_WORDS:
            parent[find(a)] = find(b)
    groups = collections.defaultdict(list)
    for k in keys:
        groups[find(k)].append(k)
    found = [sorted(g) for g in groups.values() if len(g) > 1]
    found.sort(key=lambda g: (-len(g), -len({k[0] for k in g}), g[0]))
    return found


def exemplar(group, corners):
    counts = collections.Counter(w for k in group for w in zero_words(corners[k]))
    core = {w for w, n in counts.items() if n >= max(2, len(group) // 2)}
    return max(group, key=lambda k: (sum(1 for w in zero_words(corners[k]) if w in core), [-ord(c) for c in k[0]], -k[1]))


def slots_of(rows):
    slots = [list(ABSENT) for _ in range(6)]
    for section in range(5):
        pair = pair_geometry(rows[section][0], rows[section][1])
        if pair is None:
            continue
        hz, radius = pair
        if radius >= 0.45:
            slots[section] = [hz, bandwidth(radius), True]
    return slots


def slot_text(slot):
    return "{%.2f, %.2f, %s}" % (slot[0], slot[1], "true" if slot[2] else "false")


def entries():
    corners = read_bodies()
    out = []
    for number, group in enumerate(clusters(corners), start=1):
        source = exemplar(group, corners)
        slots = slots_of(corners[source])
        present = [slot for slot in slots if slot[2]]
        if not present:
            continue
        bodies = len({k[0] for k in group})
        hz = " ".join("%.0f" % slot[0] for slot in sorted(present))
        name = "M%02d  %s  DOT %d %s" % (number, hz, bodies, "body" if bodies == 1 else "bodies")
        out.append((name, source, group, slots))
    return out


def main():
    made = entries()
    lines = ["#pragma once", "", '#include "template_shelf.hpp"', "", "#include <array>", "",
             "namespace trench::app {", "", "struct MaskEntry {", "  const char* name;",
             "  std::array<TemplatePole, 6> zeros;", "};", "",
             "inline const std::array<MaskEntry, %d> kMaskShelf{{" % len(made)]
    for name, source, group, slots in made:
        text = name.replace("DOT", "\\xc2\\xb7")
        lines.append('    {"%s", {{%s}}},' % (text, ", ".join(slot_text(s) for s in slots)))
    lines += ["}};", "", "}", ""]
    with open(HEADER, "w", encoding="utf-8", newline="\n") as stream:
        stream.write("\n".join(lines))
    for name, source, group, slots in made:
        print("%-44s from %s %s  corners %d" % (name, display(source[0]), CORNERS[source[1]], len(group)))
    print("masks", len(made))


if __name__ == "__main__":
    main()
