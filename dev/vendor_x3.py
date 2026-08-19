"""Vendor the 17 X3 runtime filters as 240-byte legacy bodies.

Each runtime block is [4 corners][N stages][5 words], little-endian, N given by
the file size (40/80/120 bytes = 1/2/3 stages). Padding unused stages with the
identity row makes it an ordinary 4-corner, 6-stage body that the rest of the
pipeline already reads.

The blocks are the compiled output of the CPhantom closed-form classes. Their
input tables verify byte-identical to EmulatorX.dll; for single-stage classes
the table and the block are the same bytes, for multi-stage the writer
transforms them, so the block is the truth.

Usage:  python dev/vendor_x3.py [--write]
"""
import sys, os, struct, json

SRC = r"C:\Users\hooki\trench-x3-clean\ref\x3_menu\runtime_blocks"
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "ref", "x3")
IDENTITY = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xE000)
CORNERS = 4
STAGES = 6

ROM = [
    ("2_pole_lowpass", "Smooth", 2, "LPF"),
    ("4_pole_lowpass", "Classic", 4, "LPF"),
    ("6_pole_lowpass", "Steeper", 6, "LPF"),
    ("2_pole_highpass", "Shallow", 2, "HPF"),
    ("4_pole_highpass", "Deeper", 4, "HPF"),
    ("2_pole_bandpass", "Band-pass1", 2, "BPF"),
    ("4_pole_bandpass", "Band-pass2", 4, "BPF"),
    ("contrary_bandpass", "ContraBand", 6, "BPF"),
    ("swept_eq_1_octave", "Swept1oct", 6, "EQ+"),
    ("swept_eq_2_1_octave", "Swept2~1oct", 6, "EQ+"),
    ("swept_eq_3_1_octave", "Swept3~1oct", 6, "EQ+"),
    ("phaser_1", "PhazeShift1", 6, "PHA"),
    ("phaser_2", "PhazeShift2", 6, "PHA"),
    ("bat_phaser", "BlissBatz", 6, "PHA"),
    ("flanger_lite", "FlangerLite", 6, "FLG"),
    ("vocal_ah_ay_ee", "Aah-Ay-Eeh", 6, "VOW"),
    ("vocal_oo_ah", "Ooh-To-Aah", 6, "VOW"),
]


def body_from_block(raw):
    stages = len(raw) // (CORNERS * 5 * 2)
    words = struct.unpack("<%dH" % (len(raw) // 2), raw)
    out = []
    for corner in range(CORNERS):
        for stage in range(STAGES):
            if stage < stages:
                base = (corner * stages + stage) * 5
                out.extend(words[base:base + 5])
            else:
                out.extend(IDENTITY)
    return struct.pack("<%dH" % len(out), *out), stages


def main(write):
    if write:
        os.makedirs(OUT, exist_ok=True)
    index = []
    for stem, name, order, kind in ROM:
        path = os.path.join(SRC, stem + "_44100.raw")
        if not os.path.exists(path):
            print("missing: %s" % stem)
            continue
        raw = open(path, "rb").read()
        body, stages = body_from_block(raw)
        assert len(body) == 240, len(body)
        target = os.path.join(OUT, "%s.bin" % stem)
        if write:
            open(target, "wb").write(body)
        index.append({"stem": stem, "name": name, "order": order, "type": kind,
                      "stages": stages})
        flag = "" if stages * 2 == order else "   (block holds %d stages, ROM order %d)" % (stages, order)
        print("%-22s %-13s %-4s %d stage%s -> 240 B%s"
              % (stem, name, kind, stages, "" if stages == 1 else "s", flag))
    if write:
        meta = {
            "schema": "trench-x3-generic-v1",
            "source": "EmulatorX.dll runtime blocks at 44,100 Hz, via trench-x3-clean/ref/x3_menu/runtime_blocks. Compiled output of the CPhantom closed-form classes; their input tables verify byte-identical to the DLL at the addresses in X3_MENU_MANIFEST.json.",
            "datum_hz": 44100.0,
            "corner_order": ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"],
            "identity_row": list(IDENTITY),
            "filters": index,
        }
        json.dump(meta, open(os.path.join(OUT, "index.json"), "w"), indent=1)
        print("\nwrote %d bodies + index.json to %s" % (len(index), OUT))
    else:
        print("\ndry run; pass --write to emit %d bodies" % len(index))


if __name__ == "__main__":
    main("--write" in sys.argv)
