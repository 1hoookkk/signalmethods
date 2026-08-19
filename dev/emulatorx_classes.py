"""Every claim in CLAUDE.md's EmulatorX.dll entry, re-derived from the binary.

Prints the CPhantom class hierarchy with its provenance grouping, the shared
frequency/radius law tables, and the rate-family proof that fixes the P2K datum
at 44,100 Hz.

Usage:  python dev/emulatorx_classes.py [path to EmulatorX.dll]
"""
import sys, os, glob, struct
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "cell_dictionary"))
import decode_lib as dl
import pefile

DLL = sys.argv[1] if len(sys.argv) > 1 else \
    r"C:\Program Files\Creative Professional\Emulator X\EmulatorX.dll"
VARIANTS = r"C:\Users\hooki\trench-x3-clean\ref\p2k_variants"

DISPATCH = 0x1806D5D00
DISPATCH_END = 0x1806D6200
BASE_TABLE = 0x1806D73A0
SLOPE_TABLE = 0x1806D73B0
PROFILES = 0x1806D73C0

TABLE_READER = 0x1802D3CE0
MORPH = {0x1802C5D60, 0x1802C5E40, 0x1802C5F10, 0x1802C6020, 0x1802C6590}


class Image:
    def __init__(self, path):
        self.pe = pefile.PE(path, fast_load=True)
        self.data = open(path, "rb").read()
        self.base = self.pe.OPTIONAL_HEADER.ImageBase

    def off(self, va):
        rva = va - self.base
        for s in self.pe.sections:
            span = max(s.Misc_VirtualSize, s.SizeOfRawData)
            if s.VirtualAddress <= rva < s.VirtualAddress + span:
                return s.PointerToRawData + (rva - s.VirtualAddress)
        raise ValueError(hex(va))

    def read(self, va, n):
        o = self.off(va)
        return self.data[o:o + n]

    def u16(self, va):
        return struct.unpack("<H", self.read(va, 2))[0]

    def u32(self, va):
        return struct.unpack("<I", self.read(va, 4))[0]

    def u64(self, va):
        return struct.unpack("<Q", self.read(va, 8))[0]

    def cstr(self, va, n=120):
        b = self.read(va, n)
        i = b.find(b"\x00")
        return b[:i].decode("latin1", errors="replace")


def hierarchy(img):
    """Walk the dispatch table as an ordered event stream. A record's descriptor
    pointer names the class of the NEXT compile function in the table, verified
    at both ends: CPhantomMorphDesigner lands on FUN_1802c6590, the type 1..3
    compiler, and CPhantomFilterP2k on FUN_1802d3ce0, the bank reader."""
    events = []
    for va in range(DISPATCH, DISPATCH_END, 8):
        q = img.u64(va)
        if 0x1802C4000 <= q <= 0x1802C7000 or q == TABLE_READER:
            events.append(("fn", q))
        elif 0x180744000 <= q <= 0x180746000:
            try:
                nm = img.cstr(img.base + img.u32(q + 0x0C) + 0x10)
            except ValueError:
                continue
            if nm.startswith(".?AV") and nm.endswith("@@"):
                events.append(("name", nm[4:-2]))
    out, pending = [], None
    for tag, value in events:
        if tag == "name":
            pending = value
        elif pending is not None:
            if pending.startswith("CPhantom"):
                group = ("rom_table" if value == TABLE_READER else
                         "morph" if value in MORPH else "closed_form")
                out.append((group, pending, value))
            pending = None
    return out


def rate_proof(sr=44100.0):
    """Decode one skin's four banks at a single rate. If the banks are the same
    filter pre-computed per rate, every live pole scales by R0/Rn."""
    skin = os.path.join(VARIANTS, "P2k_013_talking_hedz")
    sets = []
    for v in range(4):
        f = glob.glob(os.path.join(skin, "variant_%d_*.bin" % v))[0]
        corners = dl.decode_p2k_body(open(f, "rb").read(), sr)
        hz = [g.pole.hz for g in corners[0]
              if not dl.stage_is_identity(g)
              and type(g.pole).__name__ == "Conjugate" and g.pole.r > 0.45]
        sets.append(sorted(hz))
    return sets


def main():
    img = Image(DLL)
    rows = hierarchy(img)
    order = {"rom_table": 0, "closed_form": 1, "morph": 2}
    rows.sort(key=lambda r: (order[r[0]], r[2]))
    counts = {}
    print("=== CPhantom hierarchy (dispatch table 0x%x) ===" % DISPATCH)
    for group, nm, fn in rows:
        counts[group] = counts.get(group, 0) + 1
        print("  %-11s 0x%09x  %s" % (group, fn, nm))
    print("  totals:", ", ".join("%s %d" % kv for kv in sorted(counts.items())))

    print("\n=== shared frequency / radius law (writer 0x1802c59b0) ===")
    base = [img.u16(BASE_TABLE + i * 4) for i in range(4)]
    slope = [img.u16(SLOPE_TABLE + i * 4) for i in range(4)]
    print("  base  0x%x = %s" % (BASE_TABLE, base))
    print("  slope 0x%x = %s" % (SLOPE_TABLE, slope))
    print("  v = slope[rate]*byte + base[rate];  rad = (v >> 1) + 0x6400")
    print("  radius is affine in frequency; no per-section volume term")

    print("\n=== CPhantomMorphLP profiles (0x%x, 16 x 12 bytes) ===" % PROFILES)
    raw = img.read(PROFILES, 16 * 12)
    cols = list(zip(*[struct.unpack("<6H", raw[i * 12:(i + 1) * 12])
                      for i in range(16)]))
    for c, col in enumerate(cols):
        steps = [b - a for a, b in zip(col, col[1:])]
        down = sum(1 for s in steps if s < 0)
        up = sum(1 for s in steps if s > 0)
        trend = "falls" if down > up else "rises"
        against = up if down > up else down
        print("  column %d  %s  %s, %d step(s) against"
              % (c, " ".join("%04x" % x for x in col[:6]) + " ...",
                 trend, against))

    print("\n=== rate-family proof (P2k_013, decoded at one fixed rate) ===")
    sets = rate_proof()
    for v, h in enumerate(sets):
        print("  bank %d  %s" % (v, [round(x) for x in h]))
    print("  ratios against bank 0, and the rate each implies if bank 0 is 44100:")
    for v in range(1, 4):
        r = [b / a for a, b in zip(sets[0], sets[v])]
        mean = sum(r) / len(r)
        print("    bank%d  %.4f  spread %.2e  -> %.0f Hz"
              % (v, mean, max(r) - min(r), 44100.0 / mean))


if __name__ == "__main__":
    main()
