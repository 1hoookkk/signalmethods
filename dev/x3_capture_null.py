"""Score EmulatorX3 captures of Talking Hedz against the TRENCH engine.

Both captures play the same dry sawtooth at the root key.  The X3 filter
response at time t is the capture's harmonic levels minus the dry file's,
harmonic by harmonic.  TRENCH responses come from the real trench_core.dll
(pyruntime.probe: packed-word interpolation at (morph, q), decode, biquads).

Static capture: nulls the interior point the panel was set to.
Sweep capture: for each frame, the morph value whose engine response best
matches the X3 frame -- the movement law as the X3 actually played it.
"""
import math
import pathlib
import sys

import numpy as np
from scipy.io import wavfile

ROOT = pathlib.Path(r"C:\Users\hooki\trench-native")
sys.path.insert(0, r"C:\Users\hooki\trench-x3-clean\pyruntime")
from ffi import probe  # noqa: E402

SR = 44100.0
BODY = (ROOT / "plugin/ref/presets/P2k_013_talking_hedz.bin").read_bytes()
DRY = pathlib.Path(r"C:\Users\hooki\Downloads\trench_capture\dry_saw_49hz_-12dBFS.wav")
CAPS = {
    "static": pathlib.Path(r"C:\Users\hooki\Downloads\hedznoenv.wav"),
    "sweep": pathlib.Path(r"C:\Users\hooki\Downloads\hedzenv.wav"),
}
F0 = 49.14
HMAX = 16000.0
HZ = np.asarray([k * F0 for k in range(1, int(HMAX / F0))])
W = 2 * np.pi * HZ / SR
Z1 = np.exp(-1j * W)
Z2 = Z1 * Z1


def mono(path):
    sr, x = wavfile.read(path)
    x = x.astype(float)
    if x.ndim > 1:
        x = x.mean(1)
    assert sr == SR
    return x


def harmonics_at(x, t0, n=8192):
    seg = x[int(t0 * SR): int(t0 * SR) + n] * np.hanning(n)
    spec = 20.0 * np.log10(np.abs(np.fft.rfft(seg)) + 1e-12)
    bin_hz = SR / n
    out = []
    for k in range(1, int(HMAX / F0)):
        c = int(round(k * F0 / bin_hz))
        w = spec[c - 2: c + 3]
        i = int(np.argmax(w))
        a, b, g = spec[c - 2 + i - 1], spec[c - 2 + i], spec[c - 2 + i + 1]
        d = 0.5 * (a - g) / (a - 2 * b + g) if (a - 2 * b + g) < 0 else 0.0
        lobe = abs(math.sin(math.pi * d) / (math.pi * d)) / abs(1 - d * d) if abs(d) > 1e-9 else 1.0
        out.append(b - 20 * math.log10(lobe))
    return np.asarray(out)


def engine_db(morph, q):
    c = probe(BODY, morph, q, SR)
    if c is None:
        return None
    H = np.ones_like(Z1)
    for b0, b1, b2, a1, a2 in c:
        H *= (b0 + b1 * Z1 + b2 * Z2) / (1 + a1 * Z1 + a2 * Z2)
    return 20 * np.log10(np.abs(H) + 1e-30)


def weights():
    erb = 24.7 * (4.37 * HZ / 1000 + 1)
    w = 1.0 / erb
    return w / w.sum()


WT = weights()


def score(target, model):
    d = target - model
    off = float(np.sum(WT * d))
    d = d - off
    return math.sqrt(float(np.sum(WT * d * d))), float(np.abs(d).max()), off


def main():
    dry = harmonics_at(mono(DRY), 6.0)
    caps = {k: mono(p) for k, p in CAPS.items()}

    print("== static capture vs engine ==")
    x3 = harmonics_at(caps["static"], 6.0) - dry
    best = None
    for m in np.linspace(0, 1, 101):
        for q in (0.0, 0.25, 0.5, 0.75, 1.0):
            e = engine_db(m, q)
            if e is None:
                continue
            r = score(x3, e)
            if best is None or r[0] < best[0]:
                best = (r[0], r[1], r[2], m, q)
    r = score(x3, engine_db(0.5, 0.5))
    print(f"   at (0.50, 0.50): rms {r[0]:5.2f} dB, worst {r[1]:5.1f} dB, level offset {r[2]:+6.2f} dB")
    print(f"   best over the square: rms {best[0]:5.2f} dB at morph {best[3]:.2f}, q {best[4]:.2f} (offset {best[2]:+6.2f} dB)")

    print("\n== sweep capture: X3's morph position per frame (Q held 0.5) ==")
    grid = np.linspace(0, 1, 201)
    eng = {m: e for m in grid if (e := engine_db(m, 0.5)) is not None}
    traj = []
    for t0 in np.arange(0.55, 11.5, 0.25):
        x = harmonics_at(caps["sweep"], t0) - dry
        m, r = min(((m, score(x, e)) for m, e in eng.items()), key=lambda t: t[1][0])
        traj.append((t0, m, r[0], r[2]))
        print(f"   {t0:6.2f} {m:6.3f} {r[0]:6.2f} {r[2]:+8.2f}")
    np.save(ROOT / "dev" / "x3_sweep_trajectory.npy", np.asarray(traj))


if __name__ == "__main__":
    main()
