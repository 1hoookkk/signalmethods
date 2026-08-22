"""Is a Morpheus section actually doing anything?  Byte-difference from the
identity row is not the test -- a pole and zero can sit on top of each other and
cancel.  Measure each section's own response range instead, at the Morpheus
datum, through the real packed law."""
import json, pathlib, struct, collections, sys
import numpy as np
sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
import trench_native_research as core

SR = core.kMorpheusDatumHz
IDENT = (0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF)
grid = [20.0 * (0.49 * SR / 20.0) ** (i / 255.0) for i in range(256)]

names = json.load(open(r"C:\WINDOWS\TEMP\claude\C--Users-hooki-trench-native\c1331e8e-ca6d-4249-8dd3-5d4540d80e20\scratchpad\up_filters.json"))
bodies = pathlib.Path(r"C:\Users\hooki\trench-authoring\ref\morpheus\bodies")
by_num = {}
for p in bodies.glob("*.body"):
    try: by_num[int(p.name.split("_")[0])] = p
    except ValueError: pass

THRESH = float(sys.argv[1]) if len(sys.argv) > 1 else 0.5
rows = []
for num, name in sorted((int(k), v) for k, v in names.items()):
    p = by_num.get(num)
    if p is None: continue
    b = p.read_bytes()
    if len(b) != 560: continue
    w = struct.unpack("<280H", b)
    square = name.rstrip().endswith(".4") or name.rstrip().endswith(" 4")
    byte_live = 0
    active = 0
    spans = []
    for s in range(7):
        rows_s = [tuple(w[c * 35 + s * 5:c * 35 + s * 5 + 5]) for c in range(8)]
        if any(r != IDENT for r in rows_s):
            byte_live += 1
        best = 0.0
        for r in rows_s:
            db = np.asarray(core.section_db(list(r), grid, SR))
            if np.all(np.isfinite(db)):
                best = max(best, float(db.max() - db.min()))
        spans.append(best)
        if best > THRESH:
            active += 1
    rows.append((num, name, square, byte_live, active, spans))

print(f"threshold {THRESH} dB of section-own response range, {len(rows)} bodies\n")
for tag, want in ((".4 square", True), ("cube", False)):
    sub = [r for r in rows if r[2] == want]
    print(f"{tag}  n={len(sub)}")
    print("   sections differing from identity row:",
          dict(sorted(collections.Counter(r[3] for r in sub).items())))
    print("   sections actually contributing      :",
          dict(sorted(collections.Counter(r[4] for r in sub).items())))
    print()

print("bodies where byte-live exceeds actually-contributing (inert but non-identity rows):")
n = 0
for num, name, sq, bl, ac, spans in rows:
    if bl > ac:
        n += 1
        if n <= 12:
            print(f"  F{num:03d} {name:<22s} {'.4' if sq else 'cube':<5s} byte {bl} active {ac}  "
                  f"spans {' '.join(f'{s:.2f}' for s in spans)}")
print(f"  ... {n} total")

print("\nseventh-section span distribution (max over corners):")
for tag, want in ((".4", True), ("cube", False)):
    v = np.asarray([r[5][6] for r in rows if r[2] == want])
    print(f"  {tag:<5s} median {np.median(v):7.2f} dB   below 0.5 dB: {(v<0.5).sum()}/{len(v)}"
          f"   below 3 dB: {(v<3).sum()}/{len(v)}")
