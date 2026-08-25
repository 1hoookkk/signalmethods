"""Map the 120 DSP-mailbox payload words (row of 32 at param 0x2800+k) onto
the P2K 240-byte body layout: 4 corners x 6 stages x 5 u16 words, corner-major
then stage-major then word-major (dev/cell_dictionary/decode_lib.py L121-136).

Row law observed on the emulator (emu_realbody7.txt): the OS uploads the 120
words linearly, one per DSP param 0x2800+k, each written as
  index=0x2800+k & 0x1f -> 0x60000e,  data -> 0x600000+((0x2800+k)>>8&0x7e)
so the 120 words fall into 4 rows of 32/32/32/24 at param bases
0x2800 / 0x2820 / 0x2840 / 0x2860, all pinned to the same data port 0x600048.

Word n -> (corner n//30, stage (n%30)//6, slot n%5).  Slot meaning (decode_lib):
  w0,w1 = zero pair (hi/lo minifloat halves), w2,w3 = pole pair, w4 = scale*4.
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent / "cell_dictionary"))
from decode_lib import parse_p2k_bytes, P2K_CORNER_LABELS

BODY = pathlib.Path(r"C:\Users\hooki\trench-native\ref\presets\P2k_013_talking_hedz.bin")
PARAM_BASE = 0x2800
ROW = 32
SLOT = ["zero_hi", "zero_lo", "pole_hi", "pole_lo", "scale_x4"]

corners = parse_p2k_bytes(BODY.read_bytes())
flat = [w for c in corners for s in c for w in s]
assert len(flat) == 120

print(f"body = {BODY.name}  ({len(flat)} words)")
print("corner labels: " + ", ".join(P2K_CORNER_LABELS))
print()
print(f"{'param':>8} {'row':>3} {'n':>3} {'corner':>10} {'stage':>5} {'slot':>9} {'word':>6}")
for n, w in enumerate(flat):
    corner = n // 30
    stage = (n % 30) // 6
    slot = n % 5
    param = PARAM_BASE + n
    row = n // ROW
    print(f"0x{param:04X} {row:3d} {n:3d} {P2K_CORNER_LABELS[corner]:>10} {stage:5d} {SLOT[slot]:>9} 0x{w:04X}")

print()
# Summarize the per-row span of corners so the bank map is evident
print("bank summary:")
for row, lo in enumerate(range(0, 120, ROW)):
    hi = min(lo + ROW, 120)
    cs = sorted({n // 30 for n in range(lo, hi)})
    print(f"  row {row}  words {lo:3d}..{hi-1:3d}  param 0x{PARAM_BASE+lo:04X}"
          f"  corners: {', '.join(P2K_CORNER_LABELS[i] for i in cs)}")