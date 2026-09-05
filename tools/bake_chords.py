import argparse
import csv
import io
import json
import math
import os
import wave

import numpy as np

SCHEMA = "trench-chords-v1"
ROWS = 6
DEFAULT_BANDWIDTHS = [60.0, 90.0, 120.0, 150.0, 200.0, 250.0]


def note_of(hz):
    return 69.0 + 12.0 * math.log2(max(1e-3, hz) / 440.0)


def width_of(hz, bandwidth_hz):
    return 12.0 * math.log2(1.0 + bandwidth_hz / max(1.0, hz))


def width_from_radius(hz, radius, rate):
    return width_of(hz, -math.log(max(1e-6, min(1.0, radius))) * rate / math.pi)


def voice(hz, bandwidth_hz):
    return {"note": round(note_of(hz), 3), "width": round(width_of(hz, bandwidth_hz), 3)}


def chord_from_formants(freqs, bandwidths=None, gain_db=0.0):
    stages = []
    for i in range(ROWS):
        if i < len(freqs) and freqs[i] and freqs[i] > 20.0:
            bw = bandwidths[i] if bandwidths and i < len(bandwidths) and bandwidths[i] else DEFAULT_BANDWIDTHS[i]
            stages.append({"pole": voice(freqs[i], bw), "zero": None, "gain_db": gain_db})
        else:
            stages.append({"pole": None, "zero": None, "gain_db": gain_db})
    return stages


def lpc(x, order):
    x = np.asarray(x, dtype=float)
    x = x - x.mean()
    if np.abs(x).max() > 0:
        x = x / np.abs(x).max()
    n = len(x)
    r = np.array([np.dot(x[:n - k], x[k:]) for k in range(order + 1)])
    if r[0] <= 0:
        return None
    a = np.zeros(order + 1)
    a[0] = 1.0
    err = r[0]
    for i in range(1, order + 1):
        acc = r[i] + np.dot(a[1:i], r[i - 1:0:-1])
        k = -acc / err
        a_new = a.copy()
        a_new[i] = k
        for j in range(1, i):
            a_new[j] = a[j] + k * a[i - j]
        a = a_new
        err *= (1.0 - k * k)
        if err <= 0:
            break
    return a


def poles_from_lpc(a, rate, keep=ROWS, low_hz=30.0):
    roots = np.roots(a)
    cands = []
    for z in roots:
        if z.imag <= 0:
            continue
        hz = math.atan2(z.imag, z.real) / (2.0 * math.pi) * rate
        r = abs(z)
        if hz < low_hz or hz > rate * 0.45 or r < 0.3:
            continue
        cands.append((r, hz))
    cands.sort(reverse=True)
    picked = sorted(cands[:keep], key=lambda c: c[1])
    return [(hz, r) for r, hz in picked]


def chord_from_signal(x, rate, mode):
    if mode == "speech":
        target = 11025.0
        if rate > target:
            step = int(round(rate / target))
            x = x[::step]
            rate = rate / step
        a = lpc(x, 12)
    else:
        a = lpc(x, 24)
    if a is None:
        return None
    poles = poles_from_lpc(a, rate)
    stages = []
    for i in range(ROWS):
        if i < len(poles):
            hz, r = poles[i]
            stages.append({"pole": {"note": round(note_of(hz), 3), "width": 0.25}, "zero": None, "gain_db": 0.0})
        else:
            stages.append({"pole": None, "zero": None, "gain_db": 0.0})
    return stages


def read_wav(path):
    w = wave.open(path)
    rate, channels, width, frames = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
    raw = w.readframes(frames)
    if width == 1:
        x = np.frombuffer(raw, dtype=np.uint8).astype(float) - 128.0
    elif width == 2:
        x = np.frombuffer(raw, dtype=np.int16).astype(float)
    elif width == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        x = ((b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)) ^ 0x800000) - 0x800000
        x = x.astype(float)
    else:
        x = np.frombuffer(raw, dtype=np.int32).astype(float)
    if channels > 1:
        x = x.reshape(-1, channels)[:, 0]
    return x, float(rate)


def minimum_phase_ir(freqs_hz, mags_db, rate=48000.0, n=4096):
    grid = np.fft.rfftfreq(n, 1.0 / rate)
    lf = np.log(np.maximum(1.0, grid))
    src = np.log(np.maximum(1.0, np.asarray(freqs_hz, dtype=float)))
    mag_db = np.interp(lf, src, np.asarray(mags_db, dtype=float), left=mags_db[0], right=mags_db[-1])
    log_mag = mag_db / 20.0 * math.log(10.0)
    full = np.concatenate([log_mag, log_mag[-2:0:-1]])
    cep = np.fft.ifft(full).real
    fold = np.zeros_like(cep)
    fold[0] = cep[0]
    fold[1:n // 2] = 2.0 * cep[1:n // 2]
    fold[n // 2] = cep[n // 2]
    spec = np.exp(np.fft.fft(fold))
    ir = np.fft.ifft(spec).real
    return ir[: n // 2], rate


def bake_formants(args):
    text = open(args.input, encoding="utf-8", newline="").read().replace("\r\n", "\n").replace("\r", "\n")
    rows = list(csv.DictReader(io.StringIO(text)))
    fcols = args.f_cols.split(",")
    bcols = args.b_cols.split(",") if args.b_cols else None
    chords = []
    if args.group_mean:
        groups = {}
        for row in rows:
            name = row[args.name_col]
            key = (name[0], name[-2:])
            freqs = [float(row[c]) if row.get(c) not in (None, "", "0") else 0.0 for c in fcols]
            groups.setdefault(key, []).append(freqs)
        speaker = {"m": "men", "w": "women", "b": "boys", "g": "girls"}
        for (who, vowel), rowsf in sorted(groups.items(), key=lambda kv: (kv[0][1], kv[0][0])):
            means = []
            for i in range(len(fcols)):
                vals = [r[i] for r in rowsf if r[i] > 0]
                means.append(float(np.exp(np.mean(np.log(vals)))) if vals else 0.0)
            chords.append({"name": "vowel %s %s" % (vowel, speaker.get(who, who)), "source": os.path.basename(args.input), "stages": chord_from_formants(means)})
        return chords
    for row in rows:
        freqs = [float(row[c]) if row.get(c) not in (None, "", "0") else 0.0 for c in fcols]
        bands = [float(row[c]) for c in bcols] if bcols else None
        if not any(freqs):
            continue
        chords.append({"name": row[args.name_col], "source": os.path.basename(args.input), "stages": chord_from_formants(freqs, bands)})
    return chords


def bake_ir(args):
    x, rate = read_wav(args.input)
    stages = chord_from_signal(x, rate, args.mode)
    return [{"name": args.name or os.path.splitext(os.path.basename(args.input))[0], "source": os.path.basename(args.input), "stages": stages}] if stages else []


def bake_sofa(args):
    import h5py
    f = h5py.File(args.input, "r")
    ir = f["Data.IR"]
    rate = float(np.array(f["Data.SamplingRate"]).ravel()[0])
    pos = np.array(f["SourcePosition"])
    chords = []
    seen = set()
    for m in range(ir.shape[0]):
        az, el = float(pos[m, 0]), float(pos[m, 1])
        if abs(el - args.elevation) > 0.5:
            continue
        key = int(round(az / args.step))
        if key in seen or abs(az - key * args.step) > 0.5:
            continue
        seen.add(key)
        stages = chord_from_signal(np.array(ir[m, args.receiver, :], dtype=float), rate, "bells")
        if stages:
            chords.append({"name": "%s ear az %d" % ("left" if args.receiver == 0 else "right", int(round(az))), "source": os.path.basename(args.input), "stages": stages})
    return chords


def bake_magnitude(args):
    text = open(args.input, encoding="utf-8", newline="").read().replace("\r\n", "\n").replace("\r", "\n")
    rows = [r for r in csv.reader(io.StringIO(text)) if len(r) >= 2]
    pts = []
    for r in rows:
        try:
            pts.append((float(r[0]), float(r[1])))
        except ValueError:
            continue
    pts.sort()
    ir, rate = minimum_phase_ir([p[0] for p in pts], [p[1] for p in pts])
    stages = chord_from_signal(ir, rate, "bells")
    return [{"name": args.name or os.path.splitext(os.path.basename(args.input))[0], "source": os.path.basename(args.input), "stages": stages}] if stages else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("kind", choices=["formants", "ir", "sofa", "magnitude"])
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--floor", default="INSTRUMENTS")
    ap.add_argument("--name")
    ap.add_argument("--name-col", default="ID")
    ap.add_argument("--f-cols", default="F1,F2,F3,F4")
    ap.add_argument("--b-cols")
    ap.add_argument("--mode", choices=["speech", "bells"], default="bells")
    ap.add_argument("--receiver", type=int, default=0)
    ap.add_argument("--elevation", type=float, default=0.0)
    ap.add_argument("--step", type=float, default=30.0)
    ap.add_argument("--group-mean", action="store_true")
    args = ap.parse_args()
    chords = {"formants": bake_formants, "ir": bake_ir, "sofa": bake_sofa, "magnitude": bake_magnitude}[args.kind](args)
    doc = {"schema": SCHEMA, "floor": args.floor, "source": os.path.basename(args.input), "chords": chords}
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    json.dump(doc, open(args.output, "w", encoding="utf-8"), indent=1)
    print("wrote %s  %d chords  floor %s" % (args.output, len(chords), args.floor))
    for c in chords[:3]:
        print("  %-24s %s" % (c["name"], "  ".join("%.1f/%.1f" % (s["pole"]["note"], s["pole"]["width"]) if s["pole"] else "-" for s in c["stages"])))


if __name__ == "__main__":
    main()
