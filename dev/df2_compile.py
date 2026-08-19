"""Offline 12th-order serial-cascade compiler: target magnitude -> 6 biquads.

Takes a magnitude response on a 512-bin logarithmic frequency grid at 44,100 Hz
and fits a 12th-order H(z) by Steiglitz-McBride iteration, then factors it into
six second-order sections and reports every root in polar coordinates.

SciPy has no invfreqz and no stmcb - both are MATLAB. The Steiglitz-McBride
iteration is implemented here on scipy.linalg.lstsq.

Magnitude alone does not determine a filter, so the target is completed to
minimum phase by the real-cepstrum method before fitting. That is the choice
that makes the problem well posed; a different phase choice gives a different
filter with the same magnitude.

Usage:
    python dev/df2_compile.py <target.json>   target_db: [512 floats]
    python dev/df2_compile.py --selftest      fit a known factory corner
"""
import sys, json, math
import numpy as np
from scipy.linalg import lstsq

SR = 44100.0
BINS = 512
LOW_HZ = 40.0
HIGH_HZ = 16000.0
ORDER = 12
SECTIONS = 6
POLE_CEILING_R = 0.99998


def log_grid(bins=BINS, low=LOW_HZ, high=HIGH_HZ):
    return np.geomspace(low, high, bins)


def minimum_phase(target_db, hz, uniform=8192):
    """Complex minimum-phase response sampled at hz, from magnitude alone."""
    w_uniform = np.linspace(0.0, math.pi, uniform)
    hz_uniform = w_uniform * SR / (2.0 * math.pi)
    lo, hi = hz[0], hz[-1]
    logf = np.log(np.clip(hz_uniform, lo, hi))
    mag_db = np.interp(logf, np.log(hz), target_db)

    log_mag = mag_db * (math.log(10.0) / 20.0)
    full = np.concatenate([log_mag, log_mag[-2:0:-1]])
    cepstrum = np.fft.ifft(full).real
    n = full.size
    folded = np.zeros(n)
    folded[0] = cepstrum[0]
    half = n // 2
    folded[1:half] = 2.0 * cepstrum[1:half]
    folded[half] = cepstrum[half]
    spectrum = np.exp(np.fft.fft(folded))[:uniform]

    w_target = 2.0 * math.pi * hz / SR
    real = np.interp(w_target, w_uniform, spectrum.real)
    imag = np.interp(w_target, w_uniform, spectrum.imag)
    return real + 1j * imag


def steiglitz_mcbride(h, w, order=ORDER, iterations=40):
    z = np.exp(-1j * w)
    powers = np.vstack([z ** k for k in range(order + 1)]).T
    a = np.zeros(order + 1)
    a[0] = 1.0
    b = None
    for _ in range(iterations):
        denom = powers @ a
        weight = 1.0 / np.maximum(np.abs(denom), 1e-9)
        left = powers * weight[:, None]
        right = -(powers[:, 1:] * (h * weight)[:, None])
        design = np.hstack([left, right])
        rhs = h * weight
        stacked = np.vstack([design.real, design.imag])
        target = np.concatenate([rhs.real, rhs.imag])
        solution, *_ = lstsq(stacked, target)
        b = solution[: order + 1]
        a_next = np.concatenate([[1.0], solution[order + 1:]])
        if np.allclose(a_next, a, atol=1e-12):
            a = a_next
            break
        a = a_next
    return b, a


def stabilize(roots, ceiling):
    out = []
    for r in roots:
        m = abs(r)
        if m > 1.0:
            r = r / (m * m) if m > 0 else r
            m = abs(r)
        if m > ceiling:
            r = r / m * ceiling
        out.append(r)
    return np.array(out)


def pair_roots(roots):
    """Conjugate pairs first, then leftover reals two at a time."""
    remaining = list(roots)
    pairs = []
    while remaining:
        r = remaining.pop(0)
        if abs(r.imag) > 1e-9:
            mate = min(range(len(remaining)),
                       key=lambda i: abs(remaining[i] - np.conj(r)))
            remaining.pop(mate)
            pairs.append(("conjugate", abs(r), abs(math.atan2(r.imag, r.real))))
        else:
            if remaining and abs(remaining[0].imag) <= 1e-9:
                mate = remaining.pop(0)
                pairs.append(("real", (abs(r), abs(mate)), None))
            else:
                pairs.append(("real", (abs(r), 0.0), None))
    return pairs


def sort_key(pair):
    kind, a, theta = pair
    return theta if kind == "conjugate" else -1.0


def response_db(b, a, w):
    z = np.exp(-1j * w)
    num = sum(b[k] * z ** k for k in range(len(b)))
    den = sum(a[k] * z ** k for k in range(len(a)))
    return 20.0 * np.log10(np.maximum(np.abs(num / den), 1e-12))


def compile_target(target_db, hz=None):
    hz = log_grid() if hz is None else np.asarray(hz, dtype=float)
    target_db = np.asarray(target_db, dtype=float)
    if target_db.size != hz.size:
        raise ValueError("target has %d bins, grid has %d" % (target_db.size, hz.size))
    w = 2.0 * math.pi * hz / SR

    h = minimum_phase(target_db, hz)
    b, a = steiglitz_mcbride(h, w)

    poles = stabilize(np.roots(a[::-1]), POLE_CEILING_R)
    zeros = stabilize(np.roots(b[::-1]), 1.0)
    a = np.poly(poles)[::-1].real.copy()
    b = np.poly(zeros)[::-1].real.copy()

    fitted = response_db(b, a, w)
    gain_db = float(np.mean(target_db - fitted))
    b = b * (10.0 ** (gain_db / 20.0))
    fitted = fitted + gain_db

    pole_pairs = sorted(pair_roots(poles), key=sort_key)
    zero_pairs = sorted(pair_roots(zeros), key=sort_key)
    while len(pole_pairs) < SECTIONS:
        pole_pairs.append(("conjugate", 0.0, 0.0))
    while len(zero_pairs) < SECTIONS:
        zero_pairs.append(("conjugate", 0.0, 0.0))

    residual = float(np.sqrt(np.mean((target_db - fitted) ** 2)))
    return {
        "sections": list(zip(pole_pairs[:SECTIONS], zero_pairs[:SECTIONS])),
        "gain_db": gain_db,
        "rms_db": residual,
        "max_db": float(np.max(np.abs(target_db - fitted))),
        "fitted_db": fitted,
        "hz": hz,
    }


def describe(pair):
    kind, a, theta = pair
    if kind == "real":
        return "real   a=%.6f b=%.6f" % a
    hz = theta * SR / (2.0 * math.pi)
    return "theta=%.6f rad (%8.1f Hz)  R=%.6f" % (theta, hz, a)


def report(result):
    print("6 sections, 44,100 Hz, ascending pole angle\n")
    for i, (pole, zero) in enumerate(result["sections"], 1):
        print("  S%d" % i)
        print("    pole  %s" % describe(pole))
        print("    zero  %s" % describe(zero))
    print("\n  cascade gain          %+.3f dB" % result["gain_db"])
    print("  aggregate residual    %.3f dB RMS" % result["rms_db"])
    print("  worst-bin residual    %.3f dB" % result["max_db"])


def selftest():
    sys.path.insert(0, __file__.rsplit("\\", 1)[0] + r"\cell_dictionary")
    import decode_lib as dl
    import glob
    path = glob.glob(r"..\ref\presets\P2k_013*.bin")
    path = path[0] if path else glob.glob(r"ref\presets\P2k_013*.bin")[0]
    corner = dl.decode_p2k_body(open(path, "rb").read(), SR)[0]
    hz = log_grid()
    live = [g for g in corner if not dl.stage_is_identity(g)]
    target = np.asarray(dl.corner_response_db(live, list(hz), SR))
    print("self-test target: TalkingHedz M0_Q0, %d live sections, %d bins\n"
          % (len(live), hz.size))
    report(compile_target(target, hz))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--selftest":
        selftest()
        return
    if len(sys.argv) < 2:
        print(__doc__)
        return
    doc = json.load(open(sys.argv[1]))
    target = doc["target_db"] if isinstance(doc, dict) else doc
    report(compile_target(target))


if __name__ == "__main__":
    main()
