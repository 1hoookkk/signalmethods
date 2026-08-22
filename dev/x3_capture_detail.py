"""Finer look at the X3 captures: the static point's residual landscape, the
sweep's first second at 50 ms, and a straight-line fit of the morph ramp."""
import numpy as np

from x3_capture_null import CAPS, DRY, engine_db, harmonics_at, mono, score

dry = harmonics_at(mono(DRY), 6.0)
static = harmonics_at(mono(CAPS["static"]), 6.0) - dry

print("static capture: rms vs engine morph at q = 0.5")
for m in (0.0, 0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5):
    print(f"   morph {m:4.2f}: {score(static, engine_db(m, 0.5))[0]:5.2f} dB")
qs = [(round(q, 2), round(score(static, engine_db(0.0, q))[0], 2)) for q in np.linspace(0, 1, 21)]
print("static: best q at morph 0 ->", min(qs, key=lambda t: t[1]), " (q=0:", qs[0][1], " q=1:", qs[-1][1], ")")

sweep = mono(CAPS["sweep"])
grid = np.linspace(0, 1, 201)
eng = {m: e for m in grid if (e := engine_db(m, 0.5)) is not None}
print("\nsweep, 4096-point frames every 50 ms:")
pts = []
for t0 in np.arange(0.50, 4.6, 0.05):
    x = harmonics_at(sweep, t0, n=4096) - dry
    m, r = min(((m, score(x, e)[0]) for m, e in eng.items()), key=lambda t: t[1])
    pts.append((t0 + 4096 / 44100 / 2, m, r))
for t0, m, r in pts[::4]:
    print(f"   t={t0:5.2f}  morph {m:5.3f}  rms {r:4.2f}")
ramp = [(t, m) for t, m, _ in pts if 0.02 < m < 0.98]
t = np.asarray([p[0] for p in ramp])
m = np.asarray([p[1] for p in ramp])
slope, intercept = np.polyfit(t, m, 1)
resid = m - (slope * t + intercept)
print(f"\nlinear fit over the moving part: morph = {slope:.4f} * t + {intercept:+.4f}")
print(f"   morph 0 at t = {-intercept / slope:.3f} s, morph 1 at t = {(1 - intercept) / slope:.3f} s "
      f"-> ramp length {1 / slope:.3f} s; residual rms {np.sqrt(np.mean(resid ** 2)):.4f}")
