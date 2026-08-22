import pathlib
import subprocess
import sys

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from x3_capture_null import CAPS, DRY, ROOT, SR, engine_db, harmonics_at, mono, score

RENDER = ROOT / "plugin/build-juce9/TRENCH_RenderNull_artefacts/Release/TRENCH_RenderNull.exe"
BODY = ROOT / "plugin/ref/presets/P2k_013_talking_hedz.bin"
OUT = ROOT / "dev/x3_env_render.wav"
RAMP = 4.003
X3_VOICE_DB = 20 * np.log10(1.6107)
HOP = 0.05


def rms_track(x):
    n = int(HOP * SR)
    frames = len(x) // n
    seg = x[: frames * n].reshape(frames, n)
    return 20 * np.log10(np.sqrt(np.mean(seg * seg, 1)) + 1e-12)


def main():
    extra = sys.argv[1:]
    subprocess.run([str(RENDER), str(DRY), str(BODY), str(OUT), "q=0.5", f"morphRamp={RAMP}", *extra], check=True)
    x3 = mono(CAPS["sweep"])
    ours = mono(OUT)
    n = min(len(x3), len(ours))
    a, b = rms_track(x3[:n]), rms_track(ours[:n])
    t = np.arange(len(a)) * HOP
    keep = (t > 0.3) & (t < n / SR - 0.3) & (a > -60.0)
    d = (a - b)[keep]
    print(f"render peak {np.abs(ours).max():.3f}  X3 peak {np.abs(x3).max():.3f}")
    print(f"X3 - ours level, 50 ms RMS: median {np.median(d):+.2f} dB, spread p5..p95 {np.percentile(d, 5):+.2f}..{np.percentile(d, 95):+.2f}, max dev from median {np.abs(d - np.median(d)).max():.2f} dB")
    print(f"fixed voice gain in the render: {X3_VOICE_DB:+.2f} dB (kX3VoiceGain)")
    print("time  X3dB  oursdB  diff")
    for ti, ai, bi in zip(t[keep][::10], a[keep][::10], b[keep][::10]):
        print(f"{ti:5.2f} {ai:6.1f} {bi:6.1f} {ai - bi:+6.2f}")
    dry = harmonics_at(mono(DRY), 6.0)
    print("\nshape rms per frame, X3 vs our render (harmonic response minus dry)")
    for t0 in np.arange(0.55, min(11.5, n / SR - 0.3), 0.5):
        xa = harmonics_at(x3, t0) - dry
        xb = harmonics_at(ours, t0) - dry
        r = score(xa, xb)
        print(f"   {t0:5.2f}  rms {r[0]:5.2f} dB  worst {r[1]:5.1f}  level {r[2]:+6.2f}")


if __name__ == "__main__":
    main()
