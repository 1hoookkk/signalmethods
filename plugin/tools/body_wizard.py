from __future__ import annotations

import sys
import ctypes
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vowel_morph as V
import voice_lead as VL

BANNER = r"""
   ______  ____  ____  __  __   _       __ ____ ____  ___    ____  ____
  /_  __/ / __ \/ __ \/ / / /  | |     / //  _//_  / /   |  / __ \/ __ \
   / /   / /_/ / /_/ / /_/ /   | | /| / / / /   / / / /| | / /_/ / / / /
  / /   / _, _/ _, _/ __  /    | |/ |/ /_/ /   / /_/ ___ |/ _, _/ /_/ /
 /_/   /_/ |_/_/ |_/_/ /_/     |__/|__//___/  /___/_/  |_/_/ |_/_____/

        six voices  x  four corners  -  the machine fills the middle
"""

MENU = """
  What are we building?

    1  VOWEL MORPH      two mouths, Klatt physics (IY IH EH AE AA AO UH UW ER)
    2  CHORD            one progression, voice-led (Am -> F)
    3  CALL & RESPONSE  Q0 row sings A->B, Q100 row answers C->D
    4  PHASER           bat wings: notches sweep, peaks bloom with Q
    5  LADDER           resonant lowpass sweep, Q screams
    6  WAH              six-lane vocal wah
"""

def ascii_plot(db_rows, labels, width=64, height=14):
    lo, hi = -60.0, 30.0
    marks = "ox*+#"
    grid = [[" "] * width for _ in range(height)]
    zero_row = int((hi - 0.0) / (hi - lo) * (height - 1))
    for x in range(width):
        grid[zero_row][x] = "-"
    n = len(db_rows[0])
    for li, db in enumerate(db_rows):
        for x in range(width):
            v = db[int(x * (n - 1) / (width - 1))]
            y = int((hi - max(lo, min(hi, v))) / (hi - lo) * (height - 1))
            grid[y][x] = marks[li % len(marks)]
    lines = ["  +" + "-" * width + "+"]
    for y, row in enumerate(grid):
        edge = "+30" if y == 0 else (" 0 " if y == zero_row else ("-60" if y == height - 1 else "   "))
        lines.append(f"  |{''.join(row)}| {edge}")
    lines.append("  +" + "-" * width + "+")
    lines.append("   20 Hz" + " " * (width - 14) + "20 kHz")
    lines.append("   " + "   ".join(f"{marks[i]} {lab}" for i, lab in enumerate(labels)))
    return "\n".join(lines)

def show_body(body):
    print("\n  MORPH walk (Q0):")
    print(ascii_plot([V.probe_db(body, m, 0.0) for m in (0.0, 0.5, 1.0)],
                     ["M0", "M50", "M100"]))
    print("\n  Q walk (M50):")
    print(ascii_plot([V.probe_db(body, 0.5, q) for q in (0.0, 1.0)],
                     ["Q0", "Q100"]))

def compile_rows(rows_for_corner, name):
    words = []
    for c in range(4):
        rows = rows_for_corner(c)
        active = sum(1 for r in rows if r)
        peak = V.response_db([r for r in rows if r]).max()
        sc = 10 ** (-peak / 20 / active)
        for r in rows:
            words += V.IDENT if r is None else V.stage_words(*r, sc)
    wbuf = (ctypes.c_uint16 * 120)(*words)
    body = (ctypes.c_uint8 * 240)()
    if V.lib.trench_pack_body_from_corner_words(wbuf, 120, body) != 0:
        print("  the packer refused - nothing written"); return None
    p = ctypes.c_int(); mr = ctypes.c_double(); fm = ctypes.c_double(); fq = ctypes.c_double()
    V.lib.trench_certify_body(body, 240, 17, 1.0, ctypes.byref(p), ctypes.byref(mr),
                              ctypes.byref(fm), ctypes.byref(fq))
    if p.value != 1:
        print(f"  CERT FAIL at morph {fm.value:.2f} q {fq.value:.2f} - not written"); return None
    V.OUT.mkdir(parents=True, exist_ok=True)
    out = V.OUT / f"{name}.body240"
    out.write_bytes(bytes(body))
    print(f"\n  CERTIFIED (hottest pole {mr.value:.4f})  ->  {out}")
    show_body(body)
    return out

ARCH = {
    "4": ("PHASER", [
        dict(f0=700.0, f1=4200.0, pQ0=0.95, pQ1=0.94, zQ0=0.975, zQ1=0.995),
        dict(f0=1400.0, f1=8400.0, pQ0=0.95, pQ1=0.94, zQ0=0.975, zQ1=0.995),
        dict(f0=990.0, f1=5940.0, pQ0=0.94, pQ1=0.985, zQ0=0.94, zQ1=0.80),
        None, None, None]),
    "5": ("LADDER", [
        dict(f0=150.0, f1=6000.0, pQ0=0.90, pQ1=0.90, zf0=600.0, zf1=17777.8, zQ0=0.93, zQ1=0.93),
        dict(f0=150.0, f1=6000.0, pQ0=0.85, pQ1=0.99, zf0=600.0, zf1=17777.8, zQ0=0.90, zQ1=0.90),
        None, None, None, None]),
    "6": ("WAH", [
        dict(f0=120.0, f1=180.0, pQ0=0.90, pQ1=0.90, zf0=55.0, zf1=80.0, zQ0=0.97, zQ1=0.97),
        dict(f0=350.0, f1=2200.0, pQ0=0.95, pQ1=0.988, zQ0=0.85, zQ1=0.85),
        dict(f0=380.0, f1=2400.0, pQ0=0.92, pQ1=0.96, zQ0=0.84, zQ1=0.84),
        dict(f0=850.0, f1=3500.0, pQ0=0.85, pQ1=0.90, zQ0=0.80, zQ1=0.80),
        dict(f0=1200.0, f1=4800.0, pQ0=0.90, pQ1=0.90, zf0=1200.0, zf1=4800.0, zQ0=0.965, zQ1=0.985),
        dict(f0=200.0, f1=200.0, pQ0=0.90, pQ1=0.90, zQ0=0.87, zQ1=0.895)]),
}

def arch_rows(lanes, c):
    m100, q100 = c in (1, 3), c >= 2
    rows = []
    for ln in lanes:
        if ln is None:
            rows.append(None); continue
        pHz = ln["f1"] if m100 else ln["f0"]
        pR = ln["pQ1"] if q100 else ln["pQ0"]
        zHz = ln.get("zf1" if m100 else "zf0", 0.0) or pHz
        zR = ln["zQ1"] if q100 else ln["zQ0"]
        rows.append((pHz, pR, zHz, zR))
    return rows

def main():
    print(BANNER)
    print(MENU)
    choice = input("  > ").strip()
    if choice == "1":
        print("  Vowels:", " ".join(V.KLATT))
        a = input("  M0 mouth   > ").strip().upper()
        b = input("  M100 mouth > ").strip().upper()
        if a not in V.KLATT or b not in V.KLATT:
            print("  unknown vowel"); return
        compile_rows(lambda c: V.corner_rows(a, b, c), f"VOWEL_{a}_to_{b}")
    elif choice == "2":
        a = input("  first chord (e.g. Am)  > ").strip()
        b = input("  second chord (e.g. F)  > ").strip()
        compile_rows(lambda c: VL.corner_rows(a, b, c),
                     f"CHORD_{a}_to_{b}".replace("#", "s"))
    elif choice == "3":
        a = input("  call, from  > ").strip()
        b = input("  call, to    > ").strip()
        cc = input("  answer, from> ").strip()
        d = input("  answer, to  > ").strip()
        compile_rows(lambda c: VL.corner_rows(a, b, c, cc, d),
                     f"CALL_{a}{b}_RESP_{cc}{d}".replace("#", "s"))
    elif choice in ARCH:
        name, lanes = ARCH[choice]
        compile_rows(lambda c: arch_rows(lanes, c), f"ARCH_{name}")
    else:
        print("  that's not on the menu")

if __name__ == "__main__":
    main()
    if sys.stdin.isatty():
        input("\n  [enter] to close ")
