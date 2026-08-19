"""Compile E-mu's shipped Morph Designer filter templates with E-mu's own arithmetic.

The 71 XML templates under Emulator X Family/Templates/Filter hold the six-record
designer grammar that FUN_1802c6590 compiles: per section a type (0 skips, 1..3
compile), an endpoint-A frequency/gain and an endpoint-B frequency/gain, each a
0..127 byte. This reimplements that compiler from the static extraction in
ghidra_extracts/morphdesigner_types.md, at sample-rate family 0 (44,100 Hz).

Morph Designer writes each endpoint twice, so the Q axis is collapsed: corner 0
and 2 are endpoint A, corners 1 and 3 are endpoint B.

Usage:
  python dev/compile_md_templates.py            report every template
  python dev/compile_md_templates.py --write    also emit ref/md_templates/
"""
import sys, os, glob, struct, json, xml.etree.ElementTree as ET

SRC = r"C:\Users\hooki\OneDrive\Documents\Creative Professional\Emulator X Family\Templates\Filter"
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "ref", "md_templates")

BASE = [18, 18, 4, 1]
SCALE = [220, 220, 200, 177]
PAD = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xE000)
STAGES = 6
FAMILY = 0


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def compile_section(kind, freq_byte, gain_byte, shift, family=FAMILY):
    """One designer record -> five packed words, per morphdesigner_types.md."""
    if kind not in (1, 2, 3):
        return None
    freq = ((SCALE[family] * freq_byte) >> 7) + BASE[family]
    signed = gain_byte - 256 if gain_byte > 127 else gain_byte
    gain = clamp(((signed - 0x40) >> 1) + shift, -32, 31)
    rad = ((freq * 0x7C) >> 8) + 0x76
    if kind == 1:
        return (freq << 8, clamp(rad + gain, 0, 255) << 8,
                freq << 8, clamp(rad - gain, 0, 255) << 8, 0xE000)
    if kind == 2:
        if family <= 1:
            return (0xEC00, 0xFF00, freq << 8,
                    clamp(rad - gain, 0, 255) << 8, ((freq + 0xF5) << 8) & 0xFFFF)
        return (0xE100, 0xF000, freq << 8,
                clamp(rad - gain, 0, 255) << 8, (freq << 8) & 0xFFFF)
    emitted = freq
    if family <= 1 and freq > 0xDB and gain < 0:
        emitted = (((freq - 0xDC) * (gain + 0x20)) >> 5) + 0xDC
    w0 = (BASE[family] << 8) & 0xFFFF
    w1 = ((((BASE[family] * 0x7C) >> 8) + 0x96) << 8) & 0xFFFF
    if family <= 1:
        w4 = ((freq - 18) * -12 - 8192) & 0xFFFF
    else:
        w4 = 0xE000
    return (w0, w1, (emitted << 8) & 0xFFFF, clamp(rad - gain, 0, 255) << 8, w4)


def read_template(path):
    root = ET.parse(path).getroot()
    f = root.find("filter")
    num = lambda tag: float(f.findtext(tag, "0") or 0)
    secs = []
    for s in f.findall("designer-section"):
        get = lambda t: int(s.findtext(t, "0") or 0)
        secs.append({
            "type": get("type"),
            "lo_freq": get("low-freq"), "lo_gain": get("low-gain"),
            "hi_freq": get("high-freq"), "hi_gain": get("high-gain"),
        })
    return root.get("name"), num("frequency"), num("gain"), secs


def body(secs, freq_ctl, gain_ctl):
    shift = -32 + int((freq_ctl + gain_ctl) * 63.0)
    ends = []
    for side in ("lo", "hi"):
        rows = []
        for s in secs:
            w = compile_section(s["type"], s["%s_freq" % side], s["%s_gain" % side], shift)
            rows.append(w)
        live = sum(1 for r in rows if r)
        rows = [r if r else PAD for r in rows][:STAGES]
        while len(rows) < STAGES:
            rows.append(PAD)
        ends.append((rows, live))
    a, b = ends
    corners = [a[0], b[0], a[0], b[0]]
    out = []
    for c in corners:
        for row in c:
            out.extend(row)
    return struct.pack("<%dH" % len(out), *out), a[1]


def main(write):
    paths = sorted(glob.glob(os.path.join(SRC, "*.xml")))
    if not paths:
        print("no templates at %s" % SRC)
        return
    if write:
        os.makedirs(OUT, exist_ok=True)
    index = []
    for p in paths:
        name, fctl, gctl, secs = read_template(p)
        raw, live = body(secs, fctl, gctl)
        kinds = [s["type"] for s in secs if s["type"] in (1, 2, 3)]
        stem = os.path.splitext(os.path.basename(p))[0]
        if write:
            open(os.path.join(OUT, stem + ".bin"), "wb").write(raw)
        index.append({"stem": stem, "name": name, "sections": live, "types": kinds})
        print("%-24s %d section%s  types %s" % (name, live, "" if live == 1 else "s", kinds or "-"))
    if write:
        json.dump({
            "schema": "trench-md-templates-v1",
            "source": "Emulator X Family/Templates/Filter, compiled by the FUN_1802c6590 arithmetic in ghidra_extracts/morphdesigner_types.md at sample-rate family 0 (44,100 Hz).",
            "datum_hz": 44100.0,
            "corner_order": ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"],
            "note": "Morph Designer collapses Q: corners 0 and 2 are endpoint A, 1 and 3 are endpoint B.",
            "templates": index,
        }, open(os.path.join(OUT, "index.json"), "w"), indent=1)
        print("\nwrote %d bodies + index.json to %s" % (len(index), OUT))
    else:
        print("\n%d templates; pass --write to emit bodies" % len(index))


if __name__ == "__main__":
    main("--write" in sys.argv)
