import argparse
import json
import os

SCHEMA = "trench-funcgen-v4"
TOTAL = 64
PERCENT_PER_SEMITONE = 3.125
UNITS_PER_PERCENT = 0.01
GRID = 0.5
FOUR_BARS = 16.0
HALF_AXIS_PERCENT = 50.0

FWD, REV, PEND, RAND, BROWN, ONCE = 0, 1, 2, 3, 4, 5
KEY, FREERUN, CHANNEL = 0, 1, 2

DIVISIONS = (
    (0.125, "1/32"),
    (1.0 / 6.0, "1/16t"),
    (0.25, "1/16"),
    (1.0 / 3.0, "1/8t"),
    (0.375, "1/16d"),
    (0.5, "1/8"),
    (2.0 / 3.0, "1/4t"),
    (0.75, "1/8d"),
    (1.0, "1/4"),
    (4.0 / 3.0, "1/2t"),
    (1.5, "1/4d"),
    (2.0, "1/2"),
    (8.0 / 3.0, "1/1t"),
    (3.0, "1/2d"),
    (4.0, "1/1"),
    (8.0, "2/1"),
)

MINOR = (0, 2, 3, 5, 7, 9, 10)
FOURTHS = (0, 5, 10)


def transpose(cell, delta, reps):
    return [note + rep * delta for rep in range(reps) for note in cell]


def accumulate(cell, delta, reps):
    return [rep * delta + note for rep in range(reps) for note in cell]


def repeat(cell, times):
    return list(cell) * times


def scale_contour(degrees):
    return list(degrees) + [12]


def rate_label(step_beats, rate_hz):
    if rate_hz > 0.0:
        return "%g Hz" % rate_hz
    for beats, name in DIVISIONS:
        if abs(step_beats - beats) <= beats * 0.01:
            return name
    return "%g beats" % step_beats


def rate_sort_key(step_beats, rate_hz):
    return 0.5 / rate_hz if rate_hz > 0.0 else step_beats


def write_float(value):
    text = repr(float(value))
    if text.endswith(".0"):
        text = text[:-2]
    return "0" if text in ("-0", "-0.0") else text


def table(name, semitones, direction, smooth, step_beats, gates=None, cycle=False):
    return {"name": name, "semitones": list(semitones), "cycle": cycle,
            "gates": list(range(len(semitones))) if gates is None else list(gates),
            "direction": direction, "smooth": smooth, "stepBeats": step_beats}


def bank():
    return [
        table("Triplet Relay", repeat([-8, 0, 8], 4), FWD, False, 1.0 / 6.0),
        table("Rail Switch", repeat([16, -16, 8, -8], 4), FWD, False, 0.25),
        table("Backbeat Bloom", [-8, -4, 0, 4, 8, 12, 16, 12, 8, 4, 0, -4, -8, -12, -8, -4],
              FWD, True, 0.25),
        table("Eighth Sway", [-8, -4, 0, 4, 8, 4, 0, -4], PEND, True, 0.5),
        table("Long Return", list(range(16)), ONCE, True, 0.25, gates=[0, 4, 8, 12]),
        table("Broken Ladder", [-16, -12, -8, -4, 0, 4, 8, 12, 16, 12, 8, 4], REV, True, 0.5),
        table("Quarter Arc", [-16, -8, 0, 8, 16, 8, 0, -8], FWD, True, 1.0),
        table("Pendulum Teeth", [-16, -11, -5, 0, 5, 11, 16, 11], PEND, False, 1.0),
        table("Relay Teeth", repeat([-12, 12], 32), FWD, False, 0.125),
        table("Rolling Thirds", transpose([-5, 0, 5], 1, 12), FWD, False, 1.0 / 3.0),
        table("Dotted Climb", transpose([-5, 0, 5], 1, 10), FWD, False, 0.375),
        table("Waltz Ladder", transpose([-4, 0, 4], 1, 8), FWD, False, 2.0 / 3.0),
        table("Limping Fifths", transpose([-6, 1, 6], 1, 7), FWD, False, 0.75),
        table("Slow Trip", transpose([0, 5, 10], 2, 4), FWD, True, 4.0 / 3.0),
        table("Creeping Fifths", [0, 7, 1, 8, 2, 9, 3, 10, 4, 11], FWD, True, 1.5),
        table("Minor March", scale_contour(MINOR), FWD, True, 2.0),
        table("Triplet Rise", [0, 3, 6, 9, 12, 16], FWD, True, 8.0 / 3.0),
        table("Wide Breath", [0, 8, 16, 8, 0], FWD, True, 3.0),
        table("Long Arc", [0, 8, 16, 8], FWD, True, 4.0),
        table("Slow Tide", [0, 12], FWD, True, 8.0),
        table("Whole Crawl", accumulate([0, -2, -4], 1, 8), FWD, True, 0.5),
        table("Nerve Tick", [-2, 2], FWD, True, 0.25, gates=[0]),
        table("Flip Relay", [-12, 12], FWD, True, 0.25, gates=[0]),
        table("Hold And Turn", [0] * 24 + [0, 1.5, 3, 4.5, 6, 7.5, 9, 10.5], FWD, False, 0.5),
        table("Square Bloom", [0, 12, 0, -12], FWD, False, 0.5),
        table("Endless Climb", [2 * step for step in range(16)], FWD, False, 0.25, cycle=True),
        table("Three Up", transpose([0, 4, 7], 2, 4), FWD, False, 0.25),
    ]


def build(spec):
    notes = spec["semitones"]
    for note in notes:
        if abs(note * 2.0 - round(note * 2.0)) > 1e-9:
            raise SystemExit("%s: %g leaves the 1/64 semitone grid" % (spec["name"], note))
        if abs(note) * PERCENT_PER_SEMITONE > (100.0 if spec.get("cycle") else HALF_AXIS_PERCENT):
            raise SystemExit("%s: %g%% is past the morph, it would wrap at the resting wheel"
                             % (spec["name"], note * PERCENT_PER_SEMITONE))
    if len(spec["gates"]) != len(set(spec["gates"])):
        raise SystemExit("%s: a gate is set twice" % spec["name"])
    steps = len(notes)
    if steps < 1 or steps > TOTAL:
        raise SystemExit("%s: %d steps, must be 1..64" % (spec["name"], steps))
    for gate in spec["gates"]:
        if gate >= steps:
            raise SystemExit("%s: gate %d is past the end step" % (spec["name"], gate))
    rate_hz = float(spec.get("rateHz", 0.0))
    cycle = steps * spec["stepBeats"] if rate_hz <= 0.0 else 0.0
    if cycle > FOUR_BARS:
        raise SystemExit("%s: %.3f beats, past four bars" % (spec["name"], cycle))
    return {"name": spec["name"], "steps": steps, "direction": spec["direction"],
            "smooth": bool(spec["smooth"]), "semitones": notes,
            "percent": [note * PERCENT_PER_SEMITONE for note in notes],
            "values": [note * PERCENT_PER_SEMITONE * UNITS_PER_PERCENT for note in notes],
            "swingPercent": max(abs(note) for note in notes) * PERCENT_PER_SEMITONE,
            "gates": list(spec["gates"]), "stepBeats": spec["stepBeats"], "rateHz": rate_hz,
            "rate": rate_label(spec["stepBeats"], rate_hz), "cycleBeats": cycle}


def header_text(patterns):
    out = ['#pragma once',
           'namespace trench', '{', 'struct FuncGenPattern', '{',
           '    const char* name;', '    int steps;', '    int direction;', '    bool smooth;',
           '    const float* values;', '    const unsigned char* trigs;',
           '    double stepBeats;', '    double rateHz;', '};']
    for i, pattern in enumerate(patterns):
        values = ", ".join("%.9ff" % v for v in pattern["values"])
        out.append("inline constexpr float kFgV%d[] = { %s };" % (i, values))
        trigs = [0] * pattern["steps"]
        for gate in pattern["gates"]:
            trigs[gate] = 1
        out.append("inline constexpr unsigned char kFgT%d[] = { %s };" % (i, ", ".join(str(t) for t in trigs)))
    out.append("inline constexpr FuncGenPattern kFuncGenPatterns[] = {")
    for i, pattern in enumerate(patterns):
        out.append('    { "%s", %d, %d, %s, kFgV%d, kFgT%d, %s, %s },'
                   % (pattern["name"], pattern["steps"], pattern["direction"],
                      "true" if pattern["smooth"] else "false", i, i,
                      write_float(pattern["stepBeats"]), write_float(pattern["rateHz"])))
    out.append("};")
    out.append("inline constexpr int kNumFuncGenPatterns = %d;" % len(patterns))
    out.append('}')
    return "\n".join(out) + "\n"


def template_xml(pattern):
    values = [0.0] * TOTAL
    for i, value in enumerate(pattern["values"]):
        values[i] = value
    triggers = [0] * TOTAL
    for i in pattern["gates"]:
        triggers[i] = 1
    out = ['<?xml version="1.0" ?>',
           '<template module="Function Generator" name="%s" >' % pattern["name"],
           '<bpm type="long" >', '1', '</bpm>',
           '<rate type="float" >', '4', '</rate>',
           '<sync type="long" >', '2', '</sync>',
           '<smooth type="long" >', '1' if pattern["smooth"] else '0', '</smooth>',
           '<direction type="long" >', str(pattern["direction"]), '</direction>',
           '<length type="long" >', str(pattern["steps"] - 1), '</length>']
    for i, value in enumerate(values):
        out += ['<value index="%d" type="float" >' % i, write_float(value), '</value>']
    for i, trigger in enumerate(triggers):
        out += ['<trigger index="%d" type="long" >' % i, str(trigger), '</trigger>']
    out.append('</template>')
    return "\n".join(out) + "\n"


def toc_xml(names):
    out = ['<?xml version="1.0" ?>', '<toc>']
    out += ['<item>%s</item>' % name for name in names]
    out.append('</toc>')
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("output")
    ap.add_argument("--header")
    args = ap.parse_args()
    os.makedirs(args.output, exist_ok=True)
    patterns = [build(spec) for spec in bank()]
    covered = set(rate_label(pattern["stepBeats"], 0.0) for pattern in patterns)
    missing = [name for _, name in DIVISIONS if name not in covered]
    if missing:
        raise SystemExit("no preset for rate(s): %s" % ", ".join(missing))
    for pattern in patterns:
        open(os.path.join(args.output, pattern["name"] + ".xml"), "w",
             encoding="utf-8", newline="\n").write(template_xml(pattern))
    open(os.path.join(args.output, "template.toc"), "w", encoding="utf-8",
         newline="\n").write(toc_xml([p["name"] for p in patterns]))
    doc = {"schema": SCHEMA, "percent_per_semitone": PERCENT_PER_SEMITONE, "grid": GRID,
           "four_bars_in_beats": FOUR_BARS, "half_axis_percent": HALF_AXIS_PERCENT,
           "patterns": patterns}
    json.dump(doc, open(os.path.join(args.output, "funcgen.json"), "w", encoding="utf-8"), indent=1)
    if args.header:
        open(args.header, "w", encoding="utf-8", newline="\n").write(header_text(patterns))
    print("bank %d presets exported to %s" % (len(patterns), args.output))
    for pattern in patterns:
        print("  %-18s %-6s %2d steps %6.2f beats  %-5s %-4s  +-%.4g%%" % (
            pattern["name"], pattern["rate"], pattern["steps"], pattern["cycleBeats"],
            {FWD: "fwd", REV: "rev", PEND: "pend", RAND: "rand", BROWN: "brown", ONCE: "once"}[pattern["direction"]],
            "glide" if pattern["smooth"] else "step", pattern["swingPercent"]))
    print("rates covered: %d of %d" % (len(covered), len(DIVISIONS)))


if __name__ == "__main__":
    main()
