"""Unpack the 120 TalkingHedz DSP words through the ARMAdillo k1/k2 law and
audit every biquad stage for stability.

k1/k2 from the unpacked geometry:
  conjugate pair at (hz, r):  k1 = -2 r cos(TAU hz/sr);  k2 = r^2
  real pair (a,b):            k1 = -(a+b);              k2 = a*b
  degenerate:                 k1 = 0; k2 = 0
Stage coefficients (biquad):
  b0 = scale, b1 = scale*zp, b2 = scale*zq, a1 = k1(pole), a2 = k2(pole)

Stability checks per stage:
  - denominator roots: z = exp(a1/2 * [i*acos...] ) -> all |root| < 1  <=>  k2<1
  - conventional IIR: |a1| < 1+k2, |a2| < 1, |k2| < 1
  - minifloat input in [0,1] so r in [0,1] -> r^2 <= 1 (edge = 1.0 marginal)

Runs over P2k_013_talking_hedz.bin (240B) via decode_lib law.
"""
import pathlib
import sys
import math

sys.path.insert(0, str(pathlib.Path(__file__).parent / "cell_dictionary"))
from decode_lib import parse_p2k_bytes, minifloat_decode, pair_coefficients_at, stage_biquad

SR = 44100.0
BODY = pathlib.Path(r"C:\Users\hooki\trench-native\ref\presets\P2k_013_talking_hedz.bin")
corners = parse_p2k_bytes(BODY.read_bytes())

print(f"body = {BODY.name}  datum = {SR:g} Hz")
print(f"{'corner':>10} {'stage':>5} {'word':>18} {'b0':>8} {'b1':>8} {'b2':>8} {'a1':>8} {'a2':>8}  stable")
n_bad = 0
for ci, corner in enumerate(corners):
    for si, words in enumerate(corner):
        geom = None
        zmin = minifloat_decode(words[0]); zmin2 = minifloat_decode(words[1])
        pmin = minifloat_decode(words[2]); pmin2 = minifloat_decode(words[3])
        sc = minifloat_decode(words[4])
        # reconstruct pole/zero via pair_geometry_at-like (use decode_lib)
        from decode_lib import pair_geometry_at
        zero = pair_geometry_at(zmin, zmin2, SR)
        pole = pair_geometry_at(pmin, pmin2, SR)
        b0, b1, b2, a1, a2 = stage_biquad(__import__('decode_lib').StageGeometry(pole=pole, zero=zero, scale=4.0*sc), SR)
        k1, k2 = a1, a2
        conv = math.isnan(b0) or math.isnan(b1) or math.isnan(b2) or math.isnan(a1) or math.isnan(a2)
        stable = (abs(a2) < 1.0) and (abs(a1) < 1.0 + abs(a2))
        ok = stable and not conv
        if not ok:
            n_bad += 1
        print(f"{ci:3d}      {si}   0x{words[0]:04X}.{words[1]:04X}.{words[2]:04X}.{words[3]:04X}.{words[4]:04X} "
              f"{b0:8.3f} {b1:8.3f} {b2:8.3f} {a1:8.3f} {a2:8.3f}  {'OK' if ok else '***'}")
print()
print(f"stages checked: {4*6}   unstable/nan: {n_bad}")
print("RESULT:", "ALL 24 STAGES STABLE" if n_bad == 0 else "UNSTABLE STAGES PRESENT")