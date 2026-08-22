from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SR = 48_000
SECONDS = 8.0
LEVEL_DBFS = -18.0

def test_signal(rng: np.random.Generator) -> np.ndarray:
    n = int(SR * SECONDS)
    white = rng.standard_normal(n)
    spec = np.fft.rfft(white)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    spec[1:] /= np.sqrt(f[1:])
    x = np.fft.irfft(spec, n)
    x /= np.sqrt(np.mean(x * x))
    return (x * 10.0 ** (LEVEL_DBFS / 20.0)).astype(np.float32)

def response_curve(dry: np.ndarray, wet: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    from scipy.signal import welch, csd
    f, pxx = welch(dry, SR, nperseg=8192)
    _, pxy = csd(dry, wet, SR, nperseg=8192)
    h = np.abs(pxy) / np.maximum(pxx, 1e-20)
    keep = (f >= 20.0) & (f <= 20_000.0)
    return f[keep], 20.0 * np.log10(np.maximum(h[keep], 1e-9))

def main():
    args = [Path(a) for a in sys.argv[1:]]
    files = []
    for a in args:
        files += sorted(a.glob("**/*.nam")) if a.is_dir() else [a]
    if not files:
        print("no .nam files given")
        return

    import json

    from nam.models import init_from_nam

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from scipy.io import wavfile

    rng = np.random.default_rng(240)
    dry = test_signal(rng)

    import torch
    for f in files:
        d = json.loads(f.read_text(encoding="utf-8"))
        if d.get("architecture") == "SlimmableContainer":
            sub = max(d["config"]["submodels"], key=lambda s: s["max_value"])
            d = sub["model"]
        model = init_from_nam(d)
        model.eval()
        with torch.no_grad():
            wet = model(torch.from_numpy(dry)).numpy().astype(np.float32)
        if len(wet) < len(dry):
            pad = len(dry) - len(wet)
            wet = np.concatenate([np.zeros(pad, np.float32), wet])

        stem = f.stem.split(" ESR")[0].strip().lower().replace(" ", "_") \
                .replace("(", "").replace(")", "").replace("&", "and")
        outdir = ROOT / "renders" / "nam" / stem
        outdir.mkdir(parents=True, exist_ok=True)
        wavfile.write(outdir / "pink_dry.wav", SR, dry)
        wavfile.write(outdir / "pink_wet.wav", SR, wet)

        fr, db = response_curve(dry, wet)
        fig, ax = plt.subplots(figsize=(9, 4.5))
        ax.semilogx(fr, db, color="#c96a54", lw=1.6)
        ax.set_xlim(20, 20_000)
        ax.set_ylim(-60, 30)
        ax.grid(alpha=0.3, which="both")
        ax.set_xlabel("Hz")
        ax.set_ylabel("dB")
        ax.set_title(f"{f.stem}\nwet vs dry at {LEVEL_DBFS:.0f} dBFS pink noise", fontsize=9)
        fig.tight_layout()
        fig.savefig(outdir / "curve.png", dpi=110)
        plt.close(fig)
        print(f"{f.name} -> renders/nam/{stem}/  (peak {db.max():+.1f} dB, dip {db.min():+.1f} dB)")

main()
