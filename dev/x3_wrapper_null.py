"""Null the headless wrapper render (the real PluginProcessor, every control at
zero, MIX wet) against the X3 static capture, and against the engine-only
render, so whatever the wrapper adds on top of the filter is visible."""
import math
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from x3_step_null import DRY, SR, hn, mono, render, rms_db
from x3_capture_null import harmonics_at, WT

CAP = pathlib.Path(r"C:\Users\hooki\Downloads\hedznoenv.wav")
WRAP = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\hooki\Downloads\trench_capture\trench_wrapper_zero.wav")


def region(x, a, b):
    return x[int(a * SR): int(b * SR)]


cap = mono(CAP)
wrap = mono(WRAP)
dry = mono(DRY) / 32768.0
n = min(len(cap), len(wrap), len(dry))
cap, wrap, dry = cap[:n], wrap[:n], dry[:n]
lib = hn.bind()
engine = render(lib, dry, np.zeros(n), True)


def report(name, a, b):
    lvl = rms_db(region(a, 6, 11)) - rms_db(region(b, 6, 11))
    raw = rms_db(region(a, 6, 11) - region(b, 6, 11)) - rms_db(region(a, 6, 11))
    bg = b * 10 ** (lvl / 20)
    matched = rms_db(region(a, 6, 11) - region(bg, 6, 11)) - rms_db(region(a, 6, 11))
    d = harmonics_at(a, 6.0) - harmonics_at(b, 6.0)
    d = d - np.sum(WT * d)
    shape = math.sqrt(float(np.sum(WT * d * d)))
    lo = harmonics_at(a, 6.0)[:3] - harmonics_at(b, 6.0)[:3]
    print(f"{name:<34} level {lvl:+6.2f} dB   null raw {raw:6.1f}   gain-matched {matched:6.1f}   shape {shape:5.2f} dB   H1..3 {np.round(lo - lo.mean(), 2)}")


print("wrapper peak %.1f dBFS, X3 peak %.1f dBFS" % (20 * math.log10(np.abs(wrap).max() + 1e-12), 20 * math.log10(np.abs(cap).max() + 1e-12)))
report("X3 - wrapper (all zero)", cap, wrap)
report("X3 - engine filter only", cap, engine)
report("wrapper - engine filter only", wrap, engine)
