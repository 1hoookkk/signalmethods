from __future__ import annotations
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SR = 48_000.0
HOP = 32
GATE_HOPS = 16
VOICE_CEILING = 4_500.0
AIM_TAU_MS = 5.0
AIM_REF_TAU_S = 1.0
AIM_RANGE_DB = 10.0
ENV_FLOOR_DB = -80.0
DUR = 0.40
RNG = np.random.default_rng(20260807)

def biquad(b, a, x):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, xn in enumerate(x):
        yn = b[0] * xn + b[1] * x1 + b[2] * x2 - a[0] * y1 - a[1] * y2
        x2, x1 = x1, xn
        y2, y1 = y1, yn
        y[i] = yn
    return y

def butter_hp(hz):
    w = 2 * np.pi * hz / SR
    al = np.sin(w) / np.sqrt(2)
    a0 = 1 + al
    return ([(1 + np.cos(w)) / 2 / a0, -(1 + np.cos(w)) / a0, (1 + np.cos(w)) / 2 / a0],
            [-2 * np.cos(w) / a0, (1 - al) / a0])

def butter_lp(hz):
    w = 2 * np.pi * hz / SR
    al = np.sin(w) / np.sqrt(2)
    a0 = 1 + al
    return ([(1 - np.cos(w)) / 2 / a0, (1 - np.cos(w)) / a0, (1 - np.cos(w)) / 2 / a0],
            [-2 * np.cos(w) / a0, (1 - al) / a0])

def resonator(hz, bw):
    r = np.exp(-np.pi * bw / SR)
    theta = 2 * np.pi * hz / SR
    a1, a2 = -2 * r * np.cos(theta), r * r
    return ([1 + a1 + a2, 0.0, 0.0], [a1, a2])

def band_track(x, hp: bool):
    b, a = butter_hp(VOICE_CEILING) if hp else butter_lp(VOICE_CEILING)
    y = biquad(b, a, x)
    alpha = 1 - np.exp(-1.0 / SR / (AIM_TAU_MS / 1000))
    ref_alpha = 1 - np.exp(-(HOP / SR) / AIM_REF_TAU_S)
    e_x = e_dx = 0.0
    prev = 0.0
    primed = False
    ref = 0.0
    ex, hzs = [], []
    for s in range(0, len(y) - HOP, HOP):
        blk = y[s:s + HOP]
        sx = sdx = 0.0
        for v in blk:
            d = v - prev
            prev = v
            e_x += (v * v - e_x) * alpha
            e_dx += (d * d - e_dx) * alpha
            sx += v * v
            sdx += d * d
        if not primed:
            e_x, e_dx = sx / HOP, sdx / HOP
        hz = 0.0
        if e_x > 1e-12:
            ratio = max(e_dx / e_x, 0.0)
            hz = 2 * np.arcsin(min(np.sqrt(ratio) / 2, 1.0)) * SR / (2 * np.pi)
            hz = min(max(hz, VOICE_CEILING), 12_000.0)
        level = max(10 * np.log10(max(e_x, 1e-18)), ENV_FLOOR_DB) / AIM_RANGE_DB
        if not primed:
            primed, ref = True, level
        ref += (level - ref) * ref_alpha
        ex.append(min(max(level - ref, 0.0), 1.0))
        hzs.append(hz)
    return np.array(ex), np.array(hzs)

def features(x):
    hf, hz = band_track(x, hp=True)
    lf, _ = band_track(x, hp=False)
    n = min(len(hf), len(lf)) // GATE_HOPS
    out = []
    for f in range(n):
        s = slice(f * GATE_HOPS, (f + 1) * GATE_HOPS)
        m, sd = hz[s].mean(), hz[s].std()
        jitter = min(sd / m * 8.0, 1.0) if m > 1 else 0.0
        out.append([hf[s].mean(), lf[s].mean(), jitter])
    return out

DEFAULT_BW = {"F1": 60.0, "F2": 90.0, "F3": 150.0, "F4": 200.0}

def vowel_rows():
    rows, f0s = [], []
    for name in ("hillenbrand_1995", "peterson_barney_1952",
                 "etl_mokhtari_tanaka_2000"):
        d = json.loads((ROOT / "recipes/tables/academia" / f"{name}.json")
                       .read_text(encoding="utf-8", errors="replace"))
        for o in d["objects"]:
            fs = [(v["mode_or_formant"], v["frequency_hz"], v.get("bandwidth_hz"))
                  for v in o.get("formants", []) if v.get("frequency_hz")]
            if len(fs) < 3:
                continue
            if o.get("f0_hz"):
                f0s.append(o["f0_hz"])
            rows.append((o.get("f0_hz"), fs))
    rows = [(f0 if f0 else RNG.choice(f0s), fs) for f0, fs in rows]
    RNG.shuffle(rows)
    return rows

def vowels(limit=200):
    n = int(DUR * SR)
    out = []
    for f0, fs in vowel_rows()[:limit]:
        src = np.zeros(n)
        idx = np.arange(0, n, SR / f0).astype(int)
        src[idx[idx < n]] = 1.0
        y = src
        for name, hz, bw in fs:
            y = biquad(*resonator(hz, bw or DEFAULT_BW.get(name, 150.0)), y)
        out.append(y / (np.abs(y).max() + 1e-12))
    return out

def fricatives(limit=140):
    n = int(DUR * SR)
    out = []
    for _ in range(limit):
        hz = RNG.uniform(5_000, 9_000)
        bw = RNG.uniform(800, 2_500)
        b, a = resonator(hz, bw)
        y = biquad(b, a, RNG.normal(0, 1, n))
        out.append(y / (np.abs(y).max() + 1e-12))
    return out

def bright_tonal(limit=70):
    n = int(DUR * SR)
    t = np.arange(n) / SR
    out = []
    for _ in range(limit):
        f0 = RNG.uniform(150, 450)
        y = sum(np.sin(2 * np.pi * f0 * k * t) / k**0.5
                for k in range(1, int(16_000 / f0)))
        out.append(y / (np.abs(y).max() + 1e-12))
    return out

def full_band(limit=70):
    n = int(DUR * SR)
    out = []
    for _ in range(limit):
        y = RNG.normal(0, 1, n)
        env = np.exp(-np.arange(n) / (RNG.uniform(0.02, 0.10) * SR))
        out.append(y * env)
    return out

def main():
    print("synthesising...")
    pos = fricatives(200)
    neg = vowels() + bright_tonal(100) + full_band(100)
    print(f"  {len(pos)} fricative, {len(neg)} negative "
          f"({len(vowel_rows())} measured utterances available)")

    X, Y = [], []
    for sig, lab in [(pos, 1.0), (neg, 0.0)]:
        for s in sig:
            for f in features(s):
                X.append(f)
                Y.append(lab)
    X, Y = np.array(X), np.array(Y)
    print(f"  {len(X)} frames  ({int(Y.sum())} positive)")
    for i, nm in enumerate(("hf_excess", "lf_excess", "jitter")):
        print(f"    {nm:10s} pos mean {X[Y==1,i].mean():.3f}   neg mean {X[Y==0,i].mean():.3f}")

    w, b = np.zeros(3), 0.0
    wt = np.where(Y == 1, 1.0 / max(Y.sum(), 1), 1.0 / max((1 - Y).sum(), 1))
    wt = wt / wt.sum() * len(Y)
    for it in range(30_000):
        p = 1 / (1 + np.exp(-(X @ w + b)))
        g = (p - Y) * wt
        w -= 0.5 * (X.T @ g) / len(Y)
        b -= 0.5 * g.sum() / len(Y)
        if it % 10_000 == 0:
            loss = -(wt * (Y * np.log(p + 1e-12) + (1 - Y) * np.log(1 - p + 1e-12))).mean()
            print(f"    iter {it:6d}  loss {loss:.4f}")

    p = 1 / (1 + np.exp(-(X @ w + b)))
    acc = ((p > 0.5) == (Y > 0.5)).mean()
    tpr = ((p > 0.5) & (Y == 1)).sum() / max((Y == 1).sum(), 1)
    tnr = ((p <= 0.5) & (Y == 0)).sum() / max((Y == 0).sum(), 1)
    print(f"\n  accuracy {acc:.3f}   sibilance caught {tpr:.3f}   voice protected {tnr:.3f}")
    print(f"\nconst FITTED_W: [f64; NUM_FEATURES] = "
          f"[{w[0]:.4f}, {w[1]:.4f}, {w[2]:.4f}];")
    print(f"const FITTED_B: f64 = {b:.4f};")

if __name__ == "__main__":
    main()
