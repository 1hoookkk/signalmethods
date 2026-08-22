from __future__ import annotations

import ctypes
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, trim_to_p95, pack_and_certify  # noqa: E402

TABLES = Path("C:/Users/hooki/df2-workstation/filters/tables")
FS = 48_000.0
CEIL_DB = 66.0
Q_PUSH = 0.40
CANYON_TERRITORIES = (2500.0, 3150.0)

def r_from_bw(bw_hz: float) -> float:
    return math.exp(-math.pi * bw_hz / FS)

def rp_db(r: float) -> float:
    return -20.0 * math.log10(max(1.0 - r, 1e-12))

def r_from_rp(db: float) -> float:
    return 1.0 - 10.0 ** (-db / 20.0)

def push_q(r: float) -> float:
    return r_from_rp(rp_db(r) + Q_PUSH * max(0.0, CEIL_DB - rp_db(r)))

def vowel_lanes(v: dict, scale: float) -> list[list[float]]:
    f1, f2, f3 = v["f1"] * scale, v["f2"] * scale, v["f3"] * scale
    b1, b2, b3 = v["bw1"], v["bw2"], v["bw3"]
    f4, f5 = 3500.0 * scale, 4500.0 * scale
    canyon = min(max(math.sqrt(f3 * f4), CANYON_TERRITORIES[0]),
                 CANYON_TERRITORIES[1] * 4)
    rows = [
        [f1, r_from_bw(b1), f1 * 1.9, r_from_bw(b1 * 3.0), 1.0],
        [f2, r_from_bw(b2), f2 * 1.45, r_from_bw(b2 * 3.0), 1.0],
        [f3, r_from_bw(b3), f3 * 1.35, r_from_bw(b3 * 3.0), 1.0],
        [f4, r_from_bw(250.0), f4 * 1.3, r_from_bw(650.0), 1.0],
        [f5, r_from_bw(200.0), f5 * 1.25, r_from_bw(500.0), 1.0],
        [max(199.0, f1 * 0.45), r_from_rp(30.0), canyon, 1.0, 1.0],
    ]
    return rows

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    voice = "male"
    for i, a in enumerate(sys.argv):
        if a == "--voice":
            voice = sys.argv[i + 1]
    table = json.loads((TABLES / "vowel_formants.json").read_text("utf-8"))
    vowels = {e["key"]: e for e in table["vowels"]}
    scale = float(table["voice_scaling"].get(voice, 1.0))
    k0, k1 = args[0], args[1]
    v0, v1 = vowels[k0], vowels[k1]

    for k, v in ((k0, v0), (k1, v1)):
        f1 = v["f1"] * scale
        if not (199.0 <= f1 <= 580.0):
            print(f"note: {k} F1 {f1:.0f} Hz is outside the vocal-law S1 "
                  f"bounds [199, 580] - real speech, kept verbatim")

    m0 = vowel_lanes(v0, scale)
    m100 = vowel_lanes(v1, scale)
    corners = [m0, m100,
               [[p[0], push_q(p[1]), p[2], p[3], p[4]] for p in m0],
               [[p[0], push_q(p[1]), p[2], p[3], p[4]] for p in m100]]

    corner_words: list[int] = []
    for rows in corners:
        words, _ = trim_to_p95(rows, 0.0, FS)
        corner_words.extend(words)
    body, max_r = pack_and_certify(corner_words)

    out = ROOT / "bodies" / "candidates" / f"VOWEL_{k0}_to_{k1}_{voice}.body240"
    out.write_bytes(body)
    print(f"{out.name}: certified, hottest pole {max_r:.4f}")
    print(f"lanes: S1 F1 {v0['f1']*scale:.0f}->{v1['f1']*scale:.0f}  "
          f"S2 F2 {v0['f2']*scale:.0f}->{v1['f2']*scale:.0f}  "
          f"S3 F3 {v0['f3']*scale:.0f}->{v1['f3']*scale:.0f}  "
          f"S6 canyon unit-zero; Q100 = radii +40% toward {CEIL_DB:.0f} dB")

if __name__ == "__main__":
    main()
