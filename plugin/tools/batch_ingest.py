"""batch_ingest — walk a folder, convert every WAV to a transfer function.

Two measurement lanes, because a cab IR and a 303 loop are not the same object:

  --kind ir      impulse response / sweep -> FFT magnitude, smoothed.
                 Use for: cabinet IRs, room IRs, pedal tone-stack captures.
  --kind tonal   pitched or sampled material -> averaged spectrum, then the
                 HARMONIC ENVELOPE (the filter shape the notes were played
                 through), not the notes themselves.
                 Use for: 303 loops, synth samples, instrument recordings.
  --kind auto    (default) picks by detecting whether the file has a stable
                 pitch. Pitched -> tonal, unpitched -> ir.

Usage:
  python tools/batch_ingest.py C:/path/to/303-samples --kind tonal
  python tools/batch_ingest.py C:/path/to/IRs --kind ir
  python tools/batch_ingest.py C:/path/to/rooms --kind ir --detilt

Output: recipes/tfs/<name>.tf.json — the standard 512-pt log grid TF.
Feed to recipe_pipeline.py with the `tf:` source spec.
"""
import json, wave, sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
FREQS = np.geomspace(30.0, 19200.0, 512)
OUT_DIR = ROOT / "recipes" / "tfs"

FLOOR_DB = -60.0

def normalise(db: np.ndarray) -> np.ndarray:
    return np.maximum(db - np.max(db), FLOOR_DB)

def _load_wav(wav_path: Path):
    with wave.open(str(wav_path), "rb") as w:
        sr = w.getframerate()
        n_ch = w.getnchannels()
        width = w.getsampwidth()
        raw = w.readframes(w.getnframes())

    if width == 1:
        x = (np.frombuffer(raw, dtype=np.uint8).astype(np.float64) - 128.0) / 128.0
    elif width == 2:
        x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        v = np.where(v & 0x800000, v - 0x1000000, v)
        x = v.astype(np.float64) / 8388608.0
    elif width == 4:
        x = np.frombuffer(raw, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f"unsupported sample width {width}")

    if n_ch > 1:
        x = x.reshape(-1, n_ch).mean(axis=1)
    return x, sr

def _welch_psd(x, sr, nperseg=4096):
    nperseg = min(nperseg, len(x))
    if nperseg < 64:
        nperseg = len(x)
    hop = max(1, nperseg // 2)
    win = np.hanning(nperseg)
    n_frames = max(1, (len(x) - nperseg) // hop + 1)
    psd = np.zeros(nperseg // 2 + 1)
    for i in range(n_frames):
        seg = x[i * hop: i * hop + nperseg]
        if len(seg) < nperseg:
            seg = np.pad(seg, (0, nperseg - len(seg)))
        psd += np.abs(np.fft.rfft(seg * win)) ** 2
    psd /= n_frames
    return np.fft.rfftfreq(nperseg, 1.0 / sr), psd

def _detect_f0(x, sr, fmin=40.0, fmax=1200.0):
    n = min(len(x), int(sr * 0.5))
    if n < 256:
        return None, 0.0
    mid = len(x) // 2
    seg = x[max(0, mid - n // 2): mid + n // 2].astype(np.float64)
    seg = seg - seg.mean()
    if not np.any(seg):
        return None, 0.0

    corr = np.correlate(seg, seg, mode="full")[len(seg) - 1:]
    if corr[0] <= 0:
        return None, 0.0
    corr = corr / corr[0]

    lag_min = max(1, int(sr / fmax))
    lag_max = min(int(sr / fmin), len(corr) - 1)
    if lag_max <= lag_min:
        return None, 0.0

    peak = lag_min + int(np.argmax(corr[lag_min:lag_max]))
    clarity = float(corr[peak])
    if clarity <= 0.0:
        return None, 0.0
    return sr / peak, clarity

def _smooth_frac_octave(db, smooth_oct):
    lo = np.log2(np.maximum(FREQS, 1.0))
    out = np.empty_like(db)
    half = smooth_oct / 2
    for i, c in enumerate(lo):
        out[i] = np.mean(db[np.abs(lo - c) <= half])
    return out

def _detilt(db):
    lo = np.log2(np.maximum(FREQS, 1.0))
    trend = np.empty_like(db)
    for i, c in enumerate(lo):
        trend[i] = np.mean(db[np.abs(lo - c) <= 1.0])
    return db - trend

def ir_to_tf(x, sr, smooth_oct=1 / 6, detilt=False):
    n = int(2 ** np.ceil(np.log2(max(len(x), 2))))
    H = np.abs(np.fft.rfft(x, n))
    f = np.fft.rfftfreq(n, 1.0 / sr)
    mag = np.interp(FREQS, f, H)

    lo = np.log2(np.maximum(FREQS, 1.0))
    sm = np.empty_like(mag)
    for i, c in enumerate(lo):
        sel = np.abs(lo - c) <= smooth_oct / 2
        sm[i] = np.sqrt(np.mean(mag[sel] ** 2))

    db = 20.0 * np.log10(np.maximum(sm, 1e-9))
    if detilt:
        db = _detilt(db)
    return normalise(db)

def tonal_to_tf(x, sr, f0=None, smooth_oct=1 / 3, detilt=False):
    f, psd = _welch_psd(x, sr)
    psd_db = 10.0 * np.log10(np.maximum(psd, 1e-18))

    if f0 is None:
        f0, _ = _detect_f0(x, sr)

    if f0 and f0 > 20.0:
        nyq = sr * 0.5
        harmonics = np.arange(f0, min(nyq * 0.95, 20000.0), f0)
        if len(harmonics) >= 4:
            h_db = np.empty(len(harmonics))
            for i, h in enumerate(harmonics):
                sel = (f >= h * 0.94) & (f <= h * 1.06)
                h_db[i] = psd_db[sel].max() if sel.any() else -120.0
            db = np.interp(np.log(FREQS), np.log(harmonics), h_db,
                           left=h_db[0], right=h_db[-1])
            db = _smooth_frac_octave(db, smooth_oct)
            if detilt:
                db = _detilt(db)
            return normalise(db), f0

    db = np.interp(FREQS, f, psd_db)
    db = _smooth_frac_octave(db, smooth_oct)
    if detilt:
        db = _detilt(db)
    return normalise(db), f0

def wav_to_tf(wav_path: Path, kind="auto", detilt=False) -> dict:
    x, sr = _load_wav(wav_path)
    peak = np.max(np.abs(x))
    if peak > 0:
        x = x / peak

    f0, clarity = _detect_f0(x, sr)
    resolved = kind
    if kind == "auto":
        resolved = "tonal" if (f0 and clarity >= 0.30) else "ir"

    if resolved == "tonal":
        db, f0_used = tonal_to_tf(x, sr, f0=f0, detilt=detilt)
    else:
        db = ir_to_tf(x, sr, detilt=detilt)
        f0_used = None

    lows = db[FREQS < 200]
    mids = db[(FREQS >= 400) & (FREQS < 3500)]
    highs = db[FREQS >= 3500]

    return {
        "source": str(wav_path),
        "kind": resolved,
        "requested_kind": kind,
        "sample_rate": sr,
        "duration_s": round(len(x) / sr, 3),
        "detilt": detilt,
        "f0_hz": round(f0_used, 1) if f0_used else None,
        "pitch_clarity": round(clarity, 3),
        "freqs_hz": FREQS.tolist(),
        "mag_db": [round(float(v), 3) for v in db],
        "summary": {
            "peak_db": round(float(np.max(db)), 1),
            "peak_hz": round(float(FREQS[int(np.argmax(db))]), 0),
            "low_db": round(float(np.mean(lows)), 1),
            "mid_db": round(float(np.mean(mids)), 1),
            "high_db": round(float(np.mean(highs)), 1),
            "range_db": round(float(np.max(db) - np.min(db)), 1),
        },
    }

def main():
    argv = sys.argv[1:]
    kind = "auto"
    detilt = "--detilt" in argv
    if "--kind" in argv:
        i = argv.index("--kind")
        if i + 1 < len(argv):
            kind = argv[i + 1]
    if kind not in ("auto", "ir", "tonal"):
        print(f"unknown --kind {kind!r} (use: auto, ir, tonal)")
        sys.exit(1)

    positional, skip = [], False
    for a in argv:
        if skip:
            skip = False
            continue
        if a == "--kind":
            skip = True
            continue
        if a.startswith("--"):
            continue
        positional.append(a)

    if not positional:
        print(__doc__)
        sys.exit(1)

    folder = Path(positional[0])
    if not folder.is_dir():
        print(f"not a directory: {folder}")
        sys.exit(1)

    wavs = sorted(folder.rglob("*.wav"))
    if not wavs:
        zips = list(folder.rglob("*.zip"))
        print(f"no WAVs in {folder}" +
              (f" — {len(zips)} zip file(s) present, extract first" if zips else ""))
        sys.exit(1)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    print(f"{len(wavs)} WAVs in {folder}   kind={kind}" + ("  detilt" if detilt else ""))
    print()

    ok = skipped = 0
    for wav in wavs:
        try:
            tf = wav_to_tf(wav, kind=kind, detilt=detilt)
            with open(OUT_DIR / (wav.stem + ".tf.json"), "w") as fh:
                json.dump(tf, fh)
            s = tf["summary"]
            f0 = f" f0={tf['f0_hz']:.0f}" if tf["f0_hz"] else ""
            print(f"  {wav.name[:44]:44s} [{tf['kind']:5s}]{f0:>10s}  "
                  f"peak {s['peak_db']:+5.0f} @ {s['peak_hz']:>5.0f} Hz  "
                  f"lo {s['low_db']:+4.0f} mid {s['mid_db']:+4.0f} hi {s['high_db']:+4.0f}")
            ok += 1
        except Exception as e:
            print(f"  SKIP {wav.name}: {e}")
            skipped += 1

    print()
    print(f"{ok} transfer functions -> {OUT_DIR}")
    if skipped:
        print(f"{skipped} skipped")

if __name__ == "__main__":
    main()
