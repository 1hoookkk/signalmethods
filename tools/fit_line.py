from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.optimize import least_squares, minimize
from scipy.signal import find_peaks, resample_poly, welch

FS = 44_100.0
NSEC = 6
FLO = 40.0
FHI = 16_000.0
RMIN = 0.05
ROOT_LO = 80.0
RMAX = 0.999


def load_mono(path: Path) -> np.ndarray:
    rate, data = wavfile.read(path)
    x = data.astype(float)
    if x.ndim == 2:
        x = x.mean(axis=1)
    if np.issubdtype(data.dtype, np.integer):
        x /= float(np.iinfo(data.dtype).max)
    if rate != FS:
        g = math.gcd(int(rate), int(FS))
        x = resample_poly(x, int(FS) // g, int(rate) // g)
    return x - x.mean()


def reference_line(x: np.ndarray, fraction: int = 6, ppo: int = 48, impulse: bool = False):
    if impulse or x.size <= 2.0 * FS:
        nfft = 1 << int(np.ceil(np.log2(max(x.size, 2))))
        spectrum = np.fft.rfft(x, n=nfft)
        f = np.fft.rfftfreq(nfft, d=1.0 / FS)
        power = np.abs(spectrum) ** 2
    else:
        f, power = welch(x, fs=FS, window="hann", nperseg=16_384, noverlap=8_192,
                         nfft=65_536, detrend="constant", scaling="density")
    good = (f > 0) & (power > 0)
    f, power = f[good], power[good]
    n = int(np.floor(ppo * np.log2(20_000.0 / 20.0)))
    grid = 20.0 * 2.0 ** (np.arange(n + 1) / ppo)
    sigma = (1.0 / fraction) / (2.0 * np.sqrt(2.0 * np.log(2.0)))
    log_f = np.log2(f)
    smooth = np.empty_like(grid)
    for i, fc in enumerate(grid):
        weight = np.exp(-0.5 * ((log_f - np.log2(fc)) / sigma) ** 2)
        smooth[i] = np.sum(weight * power) / np.sum(weight)
    db = 10.0 * np.log10(np.maximum(smooth, np.finfo(float).tiny))
    band = (grid >= FLO) & (grid <= FHI)
    return grid, db - np.median(db[band])


def pair_db(f, fc, radius) -> np.ndarray:
    w = 2.0 * np.pi * np.asarray(f, dtype=float) / FS
    theta = 2.0 * np.pi * np.asarray(fc, dtype=float) / FS
    z1 = np.exp(-1j * w)
    q = 1.0 - 2.0 * radius * np.cos(theta) * z1 + radius ** 2 * z1 ** 2
    return 20.0 * np.log10(np.maximum(np.abs(q), np.finfo(float).tiny))


def unpack(params: np.ndarray):
    fp = 2.0 ** params[1:1 + NSEC]
    rp = params[1 + NSEC:1 + 2 * NSEC]
    fz = 2.0 ** params[1 + 2 * NSEC:1 + 3 * NSEC]
    rz = params[1 + 3 * NSEC:1 + 4 * NSEC]
    return params[0], fp, rp, fz, rz


def model_db(params: np.ndarray, f: np.ndarray) -> np.ndarray:
    gain, fp, rp, fz, rz = unpack(params)
    y = np.full_like(f, gain, dtype=float)
    for k in range(NSEC):
        y += pair_db(f, fz[k], rz[k]) - pair_db(f, fp[k], rp[k])
    return y


def seeds(f: np.ndarray, target: np.ndarray, invert: bool) -> np.ndarray:
    curve = -target if invert else target
    peaks, props = find_peaks(curve, prominence=0.5)
    chosen = f[peaks[np.argsort(props["prominences"])[::-1][:NSEC]]] if len(peaks) else np.array([])
    if len(chosen) < NSEC:
        chosen = np.concatenate([chosen, np.geomspace(FLO, FHI, NSEC)])[:NSEC]
    return np.sort(np.clip(chosen, ROOT_LO, FHI))


def fit_body(f: np.ndarray, target: np.ndarray):
    band = (f >= FLO) & (f <= FHI)
    f, t = f[band], target[band]
    x0 = np.concatenate([[0.0], np.log2(seeds(f, t, False)), np.full(NSEC, 0.9),
                         np.log2(seeds(f, t, True)), np.full(NSEC, 0.9)])
    lo = np.concatenate([[-60.0], np.full(NSEC, np.log2(ROOT_LO)), np.full(NSEC, RMIN),
                         np.full(NSEC, np.log2(ROOT_LO)), np.full(NSEC, RMIN)])
    hi = np.concatenate([[60.0], np.full(NSEC, np.log2(FHI)), np.full(NSEC, RMAX),
                         np.full(NSEC, np.log2(FHI)), np.full(NSEC, RMAX)])
    lsq = least_squares(lambda p: model_db(p, f) - t, x0, bounds=(lo, hi),
                        loss="soft_l1", f_scale=1.0, max_nfev=20_000, x_scale="jac")

    def high_p(p: np.ndarray, pnorm: float = 16.0) -> float:
        e = np.abs(model_db(p, f) - t)
        scale = max(float(e.max()), 1e-9)
        return scale * np.mean((e / scale) ** pnorm) ** (1.0 / pnorm)

    refined = minimize(high_p, lsq.x, method="L-BFGS-B", bounds=list(zip(lo, hi)),
                       options={"maxiter": 10_000, "ftol": 1e-12})
    params = refined.x if refined.success else lsq.x
    residual = model_db(params, f) - t
    return params, float(np.max(np.abs(residual))), float(np.sqrt(np.mean(residual ** 2)))


def bandwidth_hz(radius: np.ndarray) -> np.ndarray:
    return np.maximum(-np.log(np.minimum(radius, RMAX)) * FS / np.pi, 0.03)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("wav", type=Path)
    parser.add_argument("name")
    parser.add_argument("--out", type=Path, default=None)
    parser.add_argument("--ir", action="store_true")
    args = parser.parse_args()
    x = load_mono(args.wav)
    f, line = reference_line(x, impulse=args.ir)
    params, max_err, rms_err = fit_body(f, line)
    gain, fp, rp, fz, rz = unpack(params)
    order = np.argsort(fp)
    fp, rp, fz, rz = fp[order], rp[order], fz[order], rz[order]
    row_gain = np.full(NSEC, gain / NSEC)
    rows = np.column_stack([fp, bandwidth_hz(rp), fz, bandwidth_hz(rz), row_gain])
    out = args.out or args.wav.with_name(f"{args.name}.fbw")
    newline = chr(10)
    with open(out, "w", encoding="utf-8") as handle:
        handle.write(f"# {args.name}" + newline)
        for row in rows:
            handle.write("%.4f %.4f %.4f %.4f %.2f" % tuple(row) + newline)
    print("    pole Hz   pole BW   zero Hz   zero BW   gain dB")
    for row in rows:
        print("%10.1f %9.1f %10.1f %9.1f %8.2f" % tuple(row))
    print(f"gain {gain:+.2f} dB   max residual {max_err:.2f} dB   rms {rms_err:.2f} dB   -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
