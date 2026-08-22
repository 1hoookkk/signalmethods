"""Fit X3's effective morph trajectory on a step: render the engine's
per-sample path with candidate trajectories and keep the one that nulls the
capture best over the step region."""
import math

import numpy as np

from x3_step_null import CAP, DRY, SR, STEP_S, hn, mono, render, rms_db

lib = hn.bind()
dry = mono(DRY)
cap = mono(CAP)
n = min(len(dry), len(cap))
dry, cap = dry[:n], cap[:n]
t = np.arange(n) / SR


def region(x, a, b):
    return x[int(a * SR): int(b * SR)]


def null_db(r, a=1.99, b=2.20):
    return rms_db(region(cap, a, b) - region(r, a, b)) - rms_db(region(cap, a, b))


def instant(t0):
    m = np.zeros(n)
    m[t >= t0] = 1.0
    return m


def one_pole(t0, tau):
    m = np.zeros(n)
    k = t >= t0
    m[k] = 1.0 - np.exp(-(t[k] - t0) / tau)
    return m


def ramp(t0, length):
    m = np.clip((t - t0) / length, 0.0, 1.0)
    return m


GAIN = None


def score(name, morph):
    global GAIN
    r = render(lib, dry, morph, False)
    if GAIN is None:
        GAIN = rms_db(region(cap, 6, 11)) - rms_db(region(r, 6, 11))
    r *= 10 ** (GAIN / 20)
    d = null_db(r)
    print(f"   {name:<34} step-region null {d:6.1f} dB   (2.00-2.02: {null_db(r, 2.0, 2.02):6.1f}, 2.02-2.06: {null_db(r, 2.02, 2.06):6.1f}, 2.06-2.15: {null_db(r, 2.06, 2.15):6.1f})")
    return d, name


results = []
print("instant step at offset:")
for off_ms in (-4, -3, -2, -1, 0):
    results.append(score(f"instant @ {off_ms:+d} ms", instant(STEP_S + off_ms / 1000)))
print("one-pole from -2 ms, time constant:")
for tau_ms in (1, 2, 4, 8, 12, 16, 24, 32):
    results.append(score(f"one-pole tau {tau_ms} ms", one_pole(STEP_S - 0.002, tau_ms / 1000)))
print("linear ramp from -2 ms, length:")
for len_ms in (4, 8, 12, 16, 24, 32, 48, 64):
    results.append(score(f"ramp {len_ms} ms", ramp(STEP_S - 0.002, len_ms / 1000)))
best = min(results)
print(f"\nbest: {best[1]} at {best[0]:.1f} dB")
