import wave, struct, math, os
import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def read_wav(path):
    with wave.open(path, "r") as w:
        sr = w.getframerate()
        n = w.getnframes()
        ch = w.getnchannels()
        bps = w.getsampwidth()
        raw = w.readframes(n)
    samples = np.zeros(n)
    for i in range(n):
        samples[i] = struct.unpack_from("<h", raw, i * ch * bps)[0] / 32768.0
    return samples, sr

def estimate_f0(samples, sr, seg=8192):
    best_f0, best_score = 55, -999
    hann = np.hanning(seg)
    buf = samples[:seg] * hann
    spec = np.abs(np.fft.rfft(buf))
    psd = 20 * np.log10(np.maximum(spec, 1e-12))
    bin_hz = sr / seg
    for f0 in np.arange(40, 200, 0.25):
        s = 0
        count = 0
        h = f0
        while h < 4000 and count < 30:
            k = int(round(h / bin_hz))
            if k < len(psd):
                s += psd[k]
                count += 1
            h += f0
        if count > 0 and s / count > best_score:
            best_score = s / count
            best_f0 = f0
    return best_f0

def harmonic_amplitudes(samples, sr, f0, t_center, n_harmonics=40, seg=4096):
    start = max(0, int(t_center * sr) - seg // 2)
    if start + seg > len(samples):
        start = len(samples) - seg
    buf = samples[start:start + seg] * np.hanning(seg)
    spec = np.fft.rfft(buf)
    mag = np.abs(spec)
    bin_hz = sr / seg
    amps = np.zeros(n_harmonics)
    freqs = np.zeros(n_harmonics)
    for k in range(n_harmonics):
        h = (k + 1) * f0
        if h >= sr / 2:
            break
        center_bin = int(round(h / bin_hz))
        lo = max(0, center_bin - 2)
        hi = min(len(mag) - 1, center_bin + 3)
        best = lo + int(np.argmax(mag[lo:hi + 1]))
        amps[k] = float(mag[best])
        freqs[k] = best * bin_hz
    return freqs, amps

def analyze(path):
    samples, sr = read_wav(path)
    name = os.path.splitext(os.path.basename(path))[0]
    duration = len(samples) / sr
    f0 = estimate_f0(samples, sr)
    print(f"\n{'='*60}")
    print(f"{name}  f0={f0:.1f} Hz  duration={duration:.2f}s  sr={sr}")
    print(f"{'='*60}")

    n_harmonics = min(40, int(sr / 2 / f0) - 1)
    n_frames = 20
    times = np.linspace(0.1, duration - 0.1, n_frames)

    print(f"\nHarmonic-compensated filter response (c_k = 20*log10(a_k) + 20*log10(k))")
    print(f"{'t(s)':>6}  {'G(t)':>6}  {'fc_est':>7}  {'peak_dB':>7}  {'bw_est':>7}  harmonics_above_G")

    for t in times:
        freqs, amps = harmonic_amplitudes(samples, sr, f0, t, n_harmonics)
        valid = amps > 0
        if np.sum(valid) < 3:
            continue
        c = np.full(n_harmonics, -120.0)
        for k in range(n_harmonics):
            if amps[k] > 0:
                c[k] = 20 * np.log10(amps[k]) + 20 * np.log10(k + 1)

        valid_c = c[c > -100]
        if len(valid_c) < 3:
            continue
        g_t = float(np.median(valid_c[-max(3, len(valid_c) // 3):]))
        h_est = c - g_t

        peak_k = int(np.argmax(h_est[:n_harmonics]))
        peak_hz = (peak_k + 1) * f0
        peak_db = float(h_est[peak_k])

        above_g = sum(1 for k in range(n_harmonics) if h_est[k] > 3 and c[k] > -100)

        bw = 0
        if peak_db > 3:
            half = peak_db - 3
            lo_k, hi_k = peak_k, peak_k
            for k in range(peak_k, -1, -1):
                if h_est[k] < half:
                    lo_k = k
                    break
            for k in range(peak_k, n_harmonics):
                if h_est[k] < half:
                    hi_k = k
                    break
            bw = (hi_k - lo_k) * f0

        print(f"{t:6.2f}  {g_t:+6.0f}  {peak_hz:7.0f}  {peak_db:+7.1f}  {bw:7.0f}  {above_g}")

    print(f"\nFilter trajectory summary:")
    fc_traj = []
    for t in times:
        freqs, amps = harmonic_amplitudes(samples, sr, f0, t, n_harmonics)
        c = np.full(n_harmonics, -120.0)
        for k in range(n_harmonics):
            if amps[k] > 0:
                c[k] = 20 * np.log10(amps[k]) + 20 * np.log10(k + 1)
        valid_c = c[c > -100]
        if len(valid_c) < 3:
            continue
        g_t = float(np.median(valid_c[-max(3, len(valid_c) // 3):]))
        h_est = c - g_t
        peak_k = int(np.argmax(h_est[:n_harmonics]))
        fc_traj.append((t, (peak_k + 1) * f0, float(h_est[peak_k])))

    if fc_traj:
        fc_vals = [x[1] for x in fc_traj]
        res_vals = [x[2] for x in fc_traj]
        print(f"  cutoff range: {min(fc_vals):.0f} — {max(fc_vals):.0f} Hz")
        print(f"  resonance range: {min(res_vals):+.1f} — {max(res_vals):+.1f} dB")
        print(f"  f0-relative cutoff: {min(fc_vals)/f0:.1f}x — {max(fc_vals)/f0:.1f}x f0")

if __name__ == "__main__":
    for wav in sorted(os.listdir(os.path.join(REPO, "recipes", "recordings"))):
        if wav.startswith("303") and wav.endswith(".wav"):
            analyze(os.path.join(REPO, "recipes", "recordings", wav))
