"""How much does the interior change if you interpolate DECODED coefficients
instead of PACKED words?  Corners are identical either way by construction; only
the interior can differ.  A large difference makes this a candidate for the
plugin-vs-EmulatorX3 morph mismatch."""
import pathlib
import struct
import sys

import numpy as np

sys.path.insert(0, r"C:\Users\hooki\trench-native\out\build\windows-msvc-release\native\research")
import trench_native_research as core

SR = core.kP2kDatumHz
GRID = [20.0 * (0.49 * SR / 20.0) ** (i / 255.0) for i in range(256)]
PRESETS = pathlib.Path(r"C:\Users\hooki\trench-native\ref\presets")


def lerp_word(a, b, f):
    d32 = int(float(b - a) * f)
    d16 = d32 & 0xFFFF
    d16 = d16 - 65536 if d16 >= 32768 else d16
    return (a + d16) & 0xFFFF


def corners_240(raw):
    w = struct.unpack("<120H", raw)
    return [[list(w[c * 30 + s * 5: c * 30 + s * 5 + 5]) for s in range(6)] for c in range(4)]


def packed_then_decode(cs, m, q):
    out = []
    for s in range(6):
        row = []
        for k in range(5):
            e0 = lerp_word(cs[0][s][k], cs[1][s][k], m)
            e1 = lerp_word(cs[2][s][k], cs[3][s][k], m)
            row.append(lerp_word(e0, e1, q))
        out.append(row)
    return np.asarray(core.cascade_db([v for r in out for v in r], GRID, SR))


def decode_then_lerp(cs, m, q):
    def coeffs(rows):
        out = []
        for s in range(6):
            d = [core.decode_word(v) for v in rows[s]]
            c0 = 4 * d[0] + d[1]
            c1 = d[1]
            c2 = 4 * d[2] + d[3]
            c3 = d[3]
            c4 = 4 * d[4]
            out.append([c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3])
        return np.asarray(out)

    A, B, C, D = (coeffs(cs[i]) for i in range(4))
    E0 = A + (B - A) * m
    E1 = C + (D - C) * m
    K = E0 + (E1 - E0) * q
    z1 = np.exp(-1j * 2 * np.pi * np.asarray(GRID) / SR)
    z2 = z1 * z1
    total = np.zeros(len(GRID))
    for s in range(6):
        b0, b1, b2, a1, a2 = K[s]
        num = b0 + b1 * z1 + b2 * z2
        den = 1.0 + a1 * z1 + a2 * z2
        total += 20 * np.log10(np.maximum(np.abs(num / den), 1e-30))
    return total


print(f'{"preset":<28}{"m,q":>9}{"rms dB":>9}{"max dB":>9}')
rows = []
for p in sorted(PRESETS.glob("P2k_*.bin")):
    cs = corners_240(p.read_bytes())
    for (m, q) in ((0.5, 0.0), (0.5, 0.5), (0.25, 0.75), (0.75, 0.25)):
        a = packed_then_decode(cs, m, q)
        b = decode_then_lerp(cs, m, q)
        d = a - b
        ok = np.isfinite(d)
        if not ok.any():
            continue
        d = d - d[ok].mean()
        rms = float(np.sqrt((d[ok] ** 2).mean()))
        mx = float(np.abs(d[ok]).max())
        rows.append((rms, mx))
        if (m, q) == (0.5, 0.5) and len(rows) < 40:
            print(f"{p.stem:<28}{'0.5,0.5':>9}{rms:9.2f}{mx:9.2f}")
r = np.asarray(rows)
print()
print(f"across {len(rows)} interior points: median rms {np.median(r[:, 0]):.2f} dB, "
      f"p90 rms {np.percentile(r[:, 0], 90):.2f} dB, worst max {r[:, 1].max():.1f} dB")
print("corners are bit-identical either way; this is purely the interior")
