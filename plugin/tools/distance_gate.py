from __future__ import annotations
import math, os, struct, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from pyruntime.packed_interp import words_to_coeffs, kernel_to_biquad

ATLAS = r"C:\Users\hooki\df2\dev\tmp\rom_extract_verify_20260611_001341\presets"
ATLAS_SR = 39062.5
GRID = np.geomspace(20.0, 18000.0, 700)
CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
DAYLIGHT_DB = 3.0

def corner_responses(raw: bytes, sr: float) -> list[np.ndarray]:
    w = struct.unpack("<120H", raw)
    out = []
    for ci in range(4):
        z = np.exp(-1j * 2.0 * np.pi * GRID / sr)
        h = np.ones_like(z)
        for s in range(6):
            b0, b1, b2, a1, a2 = kernel_to_biquad(
                words_to_coeffs(w[ci * 30 + s * 5: ci * 30 + s * 5 + 5]))
            h = h * (b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z)
        db = 20.0 * np.log10(np.abs(h) + 1e-12)
        out.append(db - np.median(db))
    return out

def main():
    path = sys.argv[1]
    datum = float(sys.argv[2]) if len(sys.argv) > 2 else 44100.0
    cand = corner_responses(open(path, "rb").read(), datum)

    rows = []
    for fn in sorted(f for f in os.listdir(ATLAS) if f.endswith(".bin")):
        rom = corner_responses(open(os.path.join(ATLAS, fn), "rb").read(), ATLAS_SR)
        per = [float(np.sqrt(((cand[i] - rom[i]) ** 2).mean())) for i in range(4)]
        rows.append((fn[8:-4], per))

    print(f"candidate: {os.path.basename(path)} (datum {datum:g} Hz) vs 33 ROM bodies\n")
    overall = []
    for ci, label in enumerate(CORNERS):
        ranked = sorted(rows, key=lambda r: r[1][ci])
        name, per = ranked[0]
        print(f"{label:10s} nearest {name:16s} {per[ci]:6.2f} dB RMS   "
              f"(2nd: {ranked[1][0]} {ranked[1][1][ci]:.2f})")
        overall.append(per[ci])
    body_ranked = sorted(rows, key=lambda r: sum(r[1]) / 4.0)
    name, per = body_ranked[0]
    mean = sum(per) / 4.0
    print(f"\nwhole-body nearest: {name}  {mean:.2f} dB RMS mean over corners")
    verdict = "DAYLIGHT — passes" if mean >= DAYLIGHT_DB else "TOO CLOSE — fails"
    print(f"GATE ({DAYLIGHT_DB:.1f} dB): {verdict}")

if __name__ == "__main__":
    main()
