"""Native (7-section x 8-corner, 560-byte) container law survey.

Mirrors the questions already settled for the 6-section P2K container: which
words the factory actually writes, whether they lie on the same exponent-indexed
lattice, what the radius ceilings are, and whether any word is locked verbatim.
Datum is the Morpheus 39,062.5 Hz; nothing here is pooled with P2K.
"""
import collections, math, pathlib, struct, sys
import numpy as np
sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
import trench_native_research as core

SR = core.kMorpheusDatumHz
IDENT = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
LOW = [0xEE, 0xED, 0xF5, 0xF9, 0xFB, 0xFC, 0xFC, 0xFC,
       0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD]


def lattice_word(byte):
    return (byte << 8) | LOW[byte >> 4]


LATTICE = {lattice_word(b) for b in range(256)}

bodies = sorted(pathlib.Path(r"C:\Users\hooki\trench-authoring\ref\morpheus\bodies").glob("*.body"))
rows = []
for p in bodies:
    b = p.read_bytes()
    if len(b) != 560:
        continue
    w = struct.unpack("<280H", b)
    for c in range(8):
        for s in range(7):
            row = tuple(w[c * 35 + s * 5:c * 35 + s * 5 + 5])
            rows.append((p.stem, c, s, row))

live = [r for r in rows if r[3] != IDENT]
print(f"bodies {len(bodies)}   section rows {len(rows)}   non-identity rows {len(live)}"
      f"  ({100*len(live)/len(rows):.1f}%)\n")

print("== 1. lattice membership, per word slot ==")
print("   slot           on-lattice        distinct values")
for slot, label in enumerate(["0 zero mag", "1 zero rsq", "2 pole mag", "3 pole rsq", "4 scale"]):
    vals = [r[3][slot] for r in live]
    on = sum(1 for v in vals if v in LATTICE)
    print(f"   {label:<14} {100*on/len(vals):6.2f}%  {on:6d}/{len(vals)}   {len(set(vals)):6d}")

print("\n== 2. magnitude byte ceiling (words 0 and 2) ==")
for slot in (0, 2):
    bytes_ = [r[3][slot] >> 8 for r in live if r[3][slot] in LATTICE]
    if bytes_:
        print(f"   slot {slot}: max byte {max(bytes_)}  min {min(bytes_)}")

print("\n== 3. radius ceilings (conjugate roots only) ==")
for slot, label in ((2, "pole"), (0, "zero")):
    radii = []
    for _, _, _, row in live:
        g = core.geometry(list(row), SR)
        d = g["pole"] if slot == 2 else g["zero"]
        if d["kind"] == "conjugate":
            radii.append(d["radius"])
    radii = np.asarray(radii)
    print(f"   {label:<5} n={len(radii):6d}  max {radii.max():.9f}  "
          f"p99.9 {np.quantile(radii, 0.999):.9f}  median {np.median(radii):.6f}")

print("\n== 4. is any word locked verbatim, per section index? ==")
print("   sec  slot  most common value   share    distinct")
for s in range(7):
    sec = [r for r in live if r[2] == s]
    if not sec:
        continue
    for slot in range(5):
        vals = [r[3][slot] for r in sec]
        value, n = collections.Counter(vals).most_common(1)[0]
        share = n / len(vals)
        if share > 0.30:
            print(f"   {s+1:>3}  {slot:>4}  0x{value:04X}          {100*share:6.1f}%   "
                  f"{len(set(vals)):6d}")

print("\n== 5. scale word (slot 4) ==")
scales = [r[3][4] for r in live]
top = collections.Counter(scales).most_common(6)
print(f"   distinct {len(set(scales))}   most common:",
      "  ".join(f"0x{v:04X}x{n}" for v, n in top))
dec = np.asarray([4.0 * core.decode_word(v) for v in scales])
print(f"   decoded 4*d: min {dec.min():.6f}  median {np.median(dec):.6f}  max {dec.max():.6f}")
print(f"   in dB      : min {20*math.log10(max(dec.min(),1e-12)):.2f}  "
      f"max {20*math.log10(dec.max()):.2f}")

print("\n== 6. real-axis and degenerate pairs ==")
kinds = collections.Counter()
for _, _, _, row in live:
    g = core.geometry(list(row), SR)
    kinds[("pole", g["pole"]["kind"])] += 1
    kinds[("zero", g["zero"]["kind"])] += 1
for k, n in sorted(kinds.items()):
    print(f"   {k[0]:<5} {k[1]:<12} {n:6d}  ({100*n/len(live):5.1f}%)")
