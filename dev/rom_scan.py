"""Are the filter bodies in the Audity OS images?  Three independent tests."""
import pathlib, struct, collections
import numpy as np

OS = pathlib.Path(r"C:\Users\hooki\Downloads\emu_re_artifacts\audity2000\os")
ROMS = pathlib.Path(r"C:\Users\hooki\Downloads\emu_re_artifacts\audity2000\roms")
PRESETS = pathlib.Path(r"C:\Users\hooki\trench-native\ref\presets")

LOW = [0xEE,0xED,0xF5,0xF9,0xFB,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFC,0xFD]
LATTICE = {(b << 8) | LOW[b >> 4] for b in range(256)}
IDENT = bytes([0xFF,0xDF,0xFF,0xFF,0xFF,0xDF,0xFF,0xFF,0xFF,0xDF])   # LE identity row

bodies = {p.name: p.read_bytes() for p in sorted(PRESETS.glob("P2k_*.bin"))}
print(f"known bodies: {len(bodies)}, each {len(next(iter(bodies.values())))} bytes\n")

targets = sorted(OS.glob("*.bin")) + sorted(ROMS.glob("*.bin"))
for path in targets:
    data = path.read_bytes()
    print(f"== {path.name}  ({len(data)} bytes) ==")

    # 1. exact body match
    hits = [n for n, b in bodies.items() if b in data]
    print(f"   exact 240-byte body matches : {len(hits)}" + (f"  {hits[:4]}" if hits else ""))

    # 2. identity row (appears in every legacy body's 7th-section pad and in md templates)
    n_ident = data.count(IDENT)
    print(f"   identity rows (FFDF FFFF..)  : {n_ident}")

    # 3. density of lattice words -- packed filter data is dense in these
    words = np.frombuffer(data[:len(data) & ~1], dtype="<u2")
    on = np.isin(words, np.fromiter(LATTICE, dtype="<u2"))
    print(f"   lattice-word share overall   : {100*on.mean():.3f}%")
    # sliding window of 120 words = one body
    if on.size > 120:
        c = np.convolve(on.astype(np.int32), np.ones(120, dtype=np.int32), mode="valid")
        best = int(c.max()); at = int(c.argmax())
        dense = int((c >= 96).sum())
        print(f"   densest 120-word window      : {best}/120 lattice words at byte 0x{2*at:06X}")
        print(f"   windows >=80% lattice        : {dense}")
    print()
