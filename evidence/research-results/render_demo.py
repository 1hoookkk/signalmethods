import argparse, importlib.util, os, struct, zipfile
import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
spec = importlib.util.spec_from_file_location("va", os.path.join(HERE, "vowel_bodies_anatomy.py"))
va = importlib.util.module_from_spec(spec)
src = open(os.path.join(HERE, "vowel_bodies_anatomy.py"), encoding="utf-8").read().replace("if __name__", "if False and __name__")
exec(compile(src, "va", "exec"), va.__dict__)
IDENT = tuple(va.IDENT)


def load_body(path):
    raw = open(path, "rb").read()
    if len(raw) < 240:
        raise SystemExit("body too short: " + path)
    return np.array(struct.unpack("<120H", raw[:240]), dtype=np.int64).reshape(4, 6, 5)


def bank_body(name):
    z = zipfile.ZipFile(os.path.join(ROOT, "evidence", "factory-data", "p2k", "bodies", "p2k.zip"))
    for n in z.namelist():
        if n.endswith(name + ".bin"):
            return np.array(struct.unpack("<120H", z.read(n)[:240]), dtype=np.int64).reshape(4, 6, 5)
    raise SystemExit("no bank body " + name)


def corner_words(words, m, q):
    m = min(max(m, 0.0), 1.0); q = min(max(q, 0.0), 1.0)
    lo = words[0] + (words[1] - words[0]) * m
    hi = words[2] + (words[3] - words[2]) * m
    return np.floor(lo + (hi - lo) * q).astype(np.int64)


def coefficients(section_words):
    if tuple(int(w) for w in section_words) == IDENT:
        return None
    b = va.biquad([int(w) for w in section_words])
    return b


def run_cascade(x, words, fs, morph_of_t, q_of_t, tick=32):
    y = np.zeros_like(x)
    states = [[0.0, 0.0] for _ in range(6)]
    n = len(x)
    for start in range(0, n, tick):
        stop = min(n, start + tick)
        t = start / fs
        cw = corner_words(words, morph_of_t(t), q_of_t(t))
        secs = [coefficients(cw[s]) for s in range(6)]
        seg = x[start:stop].astype(np.float64)
        for s, c in enumerate(secs):
            if c is None:
                continue
            b0, b1, b2, a1, a2 = c[0], c[1], c[2], c[3], c[4]
            z1, z2 = states[s]
            out = np.empty_like(seg)
            for i in range(len(seg)):
                v = seg[i] - a1 * z1 - a2 * z2
                out[i] = b0 * v + b1 * z1 + b2 * z2
                z2 = z1; z1 = v
            states[s] = [z1, z2]
            seg = out
        y[start:stop] = seg
    return y


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--body")
    ap.add_argument("--bank")
    ap.add_argument("--seconds", type=float, default=6.0)
    ap.add_argument("--morph", default="0:0,1:1")
    ap.add_argument("--q", default="0:0")
    ap.add_argument("--gain", type=float, default=1.0)
    ap.add_argument("--rate", type=int, default=44100)
    args = ap.parse_args()
    x, fs = sf.read(args.inp, always_2d=True)
    x = x.mean(axis=1)
    if fs != args.rate:
        t = np.arange(0, len(x) / fs, 1 / args.rate)
        x = np.interp(t, np.arange(len(x)) / fs, x); fs = args.rate
    x = x[:int(args.seconds * fs)]
    words = load_body(args.body) if args.body else bank_body(args.bank)

    def curve(spec):
        pts = sorted((float(a), float(b)) for a, b in (p.split(":") for p in spec.split(",")))
        def f(t):
            if t <= pts[0][0]:
                return pts[0][1]
            for (t0, v0), (t1, v1) in zip(pts, pts[1:]):
                if t <= t1:
                    return v0 + (v1 - v0) * (t - t0) / max(1e-9, t1 - t0)
            return pts[-1][1]
        return f

    y = run_cascade(x, words, fs, curve(args.morph), curve(args.q))
    peak = np.max(np.abs(y)) + 1e-9
    y = y / peak * 0.891 * args.gain
    sf.write(args.out, y.astype(np.float32), fs, subtype="PCM_16")
    print(args.out, round(len(y) / fs, 2), "s, peak gain applied", round(20 * np.log10(0.891 / peak), 1), "dB")


if __name__ == "__main__":
    main()
