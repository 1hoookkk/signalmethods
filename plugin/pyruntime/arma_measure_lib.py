from __future__ import annotations

import ctypes
from pathlib import Path

import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[1]
DATUM = 44_100.0

lib = ctypes.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))
lib.trench_fit_arma_endpoint.argtypes = [
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double),
    ctypes.c_size_t, ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_uint16), ctypes.POINTER(ctypes.c_double)]
lib.trench_fit_arma_endpoint.restype = ctypes.c_int
lib.trench_fit_arma_endpoint_pinned.argtypes = [
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double),
    ctypes.c_size_t, ctypes.POINTER(ctypes.c_double), ctypes.c_size_t,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_uint16), ctypes.POINTER(ctypes.c_double)]
lib.trench_fit_arma_endpoint_pinned.restype = ctypes.c_int
lib.trench_stage_words_from_roots_at.argtypes = [
    ctypes.POINTER(ctypes.c_double), ctypes.c_double,
    ctypes.POINTER(ctypes.c_uint16)]
lib.trench_stage_words_from_roots_at.restype = ctypes.c_int
lib.trench_pack_body_from_corner_words.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_size_t, ctypes.c_void_p]
lib.trench_pack_body_from_corner_words.restype = ctypes.c_int
lib.trench_certify_body.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint, ctypes.c_double,
    ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
lib.trench_certify_body.restype = ctypes.c_int


try:
    lib.trench_num_stages.restype = ctypes.c_uint32
    lib.trench_num_coeffs.restype = ctypes.c_uint32
    RT_STAGES = int(lib.trench_num_stages())
    RT_COEFFS = int(lib.trench_num_coeffs())
except AttributeError:
    RT_STAGES, RT_COEFFS = 6, 5
RT_DOUBLES = RT_STAGES * RT_COEFFS


def load_wav(path: Path) -> tuple[np.ndarray, float]:
    sr, raw = wavfile.read(path)
    x = raw.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if raw.dtype == np.int16:
        x /= 32768.0
    elif raw.dtype == np.int32:
        x /= 2.0 ** 31
    return x, float(sr)

def averaged_spectrum_db(x: np.ndarray, sr: float, nfft: int = 16384):
    hop = nfft // 4
    if len(x) < nfft:
        pad = np.zeros(nfft)
        pad[:len(x)] = x * np.hanning(len(x))
        frames = [pad]
    else:
        w = np.hanning(nfft)
        frames = [x[i:i + nfft] * w for i in range(0, len(x) - nfft + 1, hop)]
    power = np.mean([np.abs(np.fft.rfft(f)) ** 2 for f in frames], axis=0)
    freqs = np.fft.rfftfreq(nfft, 1.0 / sr)
    return freqs, 10.0 * np.log10(np.maximum(power, 1e-24))

def _comb_mean_db(freqs: np.ndarray, db: np.ndarray, f0: float,
                  n_harmonics: int = 12) -> float:
    bin_hz = freqs[1] - freqs[0]
    values = []
    for k in range(1, n_harmonics + 1):
        center = k * f0
        if center >= freqs[-1]:
            break
        lo = max(int((center - 0.2 * f0) / bin_hz), 1)
        hi = min(int((center + 0.2 * f0) / bin_hz) + 1, len(db) - 1)
        if hi <= lo:
            break
        values.append(db[lo:hi].max())
    return float(np.mean(values)) if values else -1e18

def estimate_f0(freqs: np.ndarray, db: np.ndarray, lo=50.0, hi=500.0,
                octave_correct: bool = True) -> float:
    candidates = freqs[(freqs >= lo) & (freqs <= hi)]
    best_f0, best_score = lo, -1e18
    for f0 in candidates:
        ks = np.arange(1, 9) * f0
        ks = ks[ks < freqs[-1]]
        idx = np.searchsorted(freqs, ks)
        score = db[np.clip(idx, 0, len(db) - 1)].sum() / len(ks)
        if score > best_score:
            best_score, best_f0 = score, f0
    f0 = float(best_f0)
    if octave_correct:
        while f0 / 2.0 >= 30.0 and (_comb_mean_db(freqs, db, f0 / 2.0)
                                    >= _comb_mean_db(freqs, db, f0) - 3.0):
            f0 /= 2.0
        while (_comb_mean_db(freqs, db, f0 * 2.0)
               >= _comb_mean_db(freqs, db, f0) + 3.0):
            f0 *= 2.0
    return f0

def estimate_f0_autocorr(x: np.ndarray, sr: float, lo=50.0, hi=400.0):
    s = x - np.mean(x)
    n = min(len(s), int(sr))
    s = s[:n]
    ac = np.correlate(s, s, 'full')[n - 1:]
    ac /= max(ac[0], 1e-24)
    la, lb = int(sr / hi), int(sr / lo)
    lag = la + int(np.argmax(ac[la:lb]))
    return sr / lag, float(ac[lag])

def harmonic_envelope(x: np.ndarray, sr: float, fmax: float = 16_000.0,
                      snr_stop_db: float = 6.0):
    freqs, db = averaged_spectrum_db(x, sr)
    f0_ac, voiced = estimate_f0_autocorr(x, sr)
    if voiced > 0.4:
        f0 = estimate_f0(freqs, db, lo=0.9 * f0_ac, hi=1.12 * f0_ac,
                         octave_correct=False)
        resolves = 4.0 * sr / len(x) < 0.6 * f0
    else:
        f0 = estimate_f0(freqs, db, octave_correct=False)
        resolves = 4.0 * sr / len(x) < 0.6 * f0
        if resolves:
            f0 = estimate_f0(freqs, db)
            resolves = 4.0 * sr / len(x) < 0.6 * f0
    bin_hz = freqs[1] - freqs[0]
    cand = []
    k = 1
    while k * f0 < min(fmax, sr * 0.45):
        center = k * f0
        lo = int(max((center - 0.45 * f0) / bin_hz, 1))
        hi = int(min((center + 0.45 * f0) / bin_hz, len(db) - 1))
        if hi <= lo:
            break
        window = db[lo:hi]
        peak = lo + int(np.argmax(window))
        f_lo = int((center + 0.25 * f0) / bin_hz)
        f_hi = min(int((center + 0.75 * f0) / bin_hz), len(db) - 1)
        floor = np.median(db[f_lo:f_hi]) if f_hi > f_lo else -1e18
        weak = resolves and db[peak] - floor < snr_stop_db
        cand.append((freqs[peak], db[peak], weak))
        k += 1
    last_strong = -1
    top = max((c[1] for c in cand), default=0.0)
    for i, (_, d, weak) in enumerate(cand):
        if not weak and d > top - 70.0:
            last_strong = i
    out_f = [c[0] for c in cand[:last_strong + 1]]
    out_db = [c[1] for c in cand[:last_strong + 1]]
    return f0, np.array(out_f), np.array(out_db)

def log_grid_target(harm_f: np.ndarray, harm_db: np.ndarray,
                    n: int = 512) -> tuple[np.ndarray, np.ndarray]:
    smooth = harm_db.copy()
    if len(smooth) >= 3:
        smooth[1:-1] = (harm_db[:-2] + harm_db[1:-1] + harm_db[2:]) / 3.0
    lo, hi = harm_f[0], harm_f[-1]
    grid = np.geomspace(lo, hi, n)
    dbs = np.interp(np.log(grid), np.log(harm_f), smooth)
    offset = dbs.mean()
    return grid, dbs - offset, offset

def formant_peaks(freqs: np.ndarray, dbs: np.ndarray, max_peaks: int = 5,
                  min_prominence_db: float = 4.0) -> list[float]:
    from scipy.signal import find_peaks
    idx, props = find_peaks(dbs, prominence=min_prominence_db)
    order = np.argsort(props["prominences"])[::-1][:max_peaks]
    return sorted(float(freqs[i]) for i in idx[order])

def fit_arma(freqs: np.ndarray, dbs: np.ndarray, rate: float = DATUM,
             pinned_hz: list[float] | None = None):
    n = len(freqs)
    roots = (ctypes.c_double * RT_DOUBLES)()
    words = (ctypes.c_uint16 * 30)()
    metrics = (ctypes.c_double * 4)()
    if pinned_hz:
        k = len(pinned_hz)
        rc = lib.trench_fit_arma_endpoint_pinned(
            (ctypes.c_double * n)(*freqs), (ctypes.c_double * n)(*dbs), n,
            (ctypes.c_double * k)(*pinned_hz), k, rate, roots, words, metrics)
    else:
        rc = lib.trench_fit_arma_endpoint(
            (ctypes.c_double * n)(*freqs), (ctypes.c_double * n)(*dbs), n,
            rate, roots, words, metrics)
    assert rc == 0, f"arma fit rc={rc}"
    return ([list(roots[s * 5:s * 5 + 5]) for s in range(6)],
            [list(words[s * 5:s * 5 + 5]) for s in range(6)], list(metrics))

def roots_response_db(roots6, freqs: np.ndarray, rate: float = DATUM):
    z = np.exp(-1j * 2.0 * np.pi * freqs / rate)
    h = np.ones_like(z, dtype=complex)
    for phz, pr, zhz, zr, sc in roots6:
        wz, wp = 2 * np.pi * zhz / rate, 2 * np.pi * phz / rate
        h *= (sc * (1 - 2 * zr * np.cos(wz) * z + zr * zr * z * z)
              / (1 - 2 * pr * np.cos(wp) * z + pr * pr * z * z))
    return 20.0 * np.log10(np.maximum(np.abs(h), 1e-15))

def cascade_peak_db(roots6, rate: float = DATUM) -> float:
    freqs = np.geomspace(20.0, rate / 2.0, 4096)
    return float(roots_response_db(roots6, freqs, rate).max())

def trim_gain_budget(roots6, rate: float = DATUM):
    peak_db = cascade_peak_db(roots6, rate)
    per_section = 10.0 ** (-peak_db / 20.0 / 6.0)
    trimmed = [[p[0], p[1], p[2], p[3], p[4] * per_section] for p in roots6]
    words = []
    for p in trimmed:
        pz = list(p)
        if pz[0] < 1.0 and pz[1] < 0.3:
            words.extend([0xdfff, 0xffff, 0xdfff, 0xffff, 0xdfff])
            continue
        if pz[2] < 20.0: pz[2] = 20.0
        if pz[2] > rate * 0.48: pz[2] = rate * 0.48
        row = (ctypes.c_uint16 * 5)()
        rc = lib.trench_stage_words_from_roots_at(
            (ctypes.c_double * 5)(*pz), rate, row)
        if rc != 0:
            pz[2] = rate * 0.45; pz[3] = 0.9
            rc = lib.trench_stage_words_from_roots_at(
                (ctypes.c_double * 5)(*pz), rate, row)
        assert rc == 0, f"re-encode refused: {p} -> {pz}"
        words.extend(row)
    return words, trimmed, peak_db

_MINIFLOAT_TABLE = None

def _nearest_scale_word(target: float) -> int:
    global _MINIFLOAT_TABLE
    if _MINIFLOAT_TABLE is None:
        import sys
        sys.path.insert(0, str(ROOT))
        from pyruntime.packed_interp import decode as _dec
        _MINIFLOAT_TABLE = np.array([_dec(w) for w in range(65536)])
    return int(np.argmin(np.abs(_MINIFLOAT_TABLE - target)))

def dc_anchor_body(body: bytes, rate: float = DATUM) -> bytes:
    out = ctypes.create_string_buffer(240)
    rc = lib.trench_body_dc_anchor(body, len(body), out)
    if rc != 0:
        raise ValueError(f"dc anchor refused (rc={rc}): degenerate cascade "
                         "(a corner's DC gain is zero or non-finite)")
    return out.raw

def pack_and_certify(corner_words: list[int]) -> tuple[bytes, float]:
    body = ctypes.create_string_buffer(240)
    assert lib.trench_pack_body_from_corner_words(
        (ctypes.c_uint16 * 120)(*corner_words), 120, body) == 0
    p, mr = ctypes.c_int(), ctypes.c_double()
    fm, fq = ctypes.c_double(), ctypes.c_double()
    assert lib.trench_certify_body(body, 240, 33, 1.0, ctypes.byref(p),
                                   ctypes.byref(mr), ctypes.byref(fm),
                                   ctypes.byref(fq)) == 0
    assert p.value == 1, f"UNSTABLE at morph {fm.value} q {fq.value}"
    return body.raw, mr.value

def trim_to_p95(roots6, p95_target_db: float, rate: float = DATUM):
    freqs = np.geomspace(20.0, 18000.0, 700)
    db = roots_response_db(roots6, freqs, rate)
    offset_db = p95_target_db - float(np.percentile(db, 95))
    per_section = 10.0 ** (offset_db / 20.0 / 6.0)
    trimmed = [[p[0], p[1], p[2], p[3], p[4] * per_section] for p in roots6]
    words = []
    for p in trimmed:
        pz = list(p)
        if pz[0] < 1.0 and pz[1] < 0.3:
            words.extend([0xdfff, 0xffff, 0xdfff, 0xffff, 0xdfff])
            continue
        if pz[2] < 20.0: pz[2] = 20.0
        if pz[2] > rate * 0.48: pz[2] = rate * 0.48
        row = (ctypes.c_uint16 * 5)()
        rc = lib.trench_stage_words_from_roots_at(
            (ctypes.c_double * 5)(*pz), rate, row)
        if rc != 0:
            pz[2] = rate * 0.45; pz[3] = 0.9
            rc = lib.trench_stage_words_from_roots_at(
                (ctypes.c_double * 5)(*pz), rate, row)
        assert rc == 0, f"re-encode refused: {p} -> {pz}"
        words.extend(row)
    return words, trimmed
