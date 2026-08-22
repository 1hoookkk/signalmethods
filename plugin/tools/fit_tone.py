from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "pyruntime"))
from arma_measure_lib import load_wav   # noqa: E402

DRY = Path(sys.argv[1]); WET = Path(sys.argv[2]); OUT = Path(sys.argv[3])
EPOCHS = int(sys.argv[4]) if len(sys.argv) > 4 else 60
H = 16

x, srx = load_wav(DRY)
y, sry = load_wav(WET)
assert srx == sry, "rate mismatch"
n = min(len(x), len(y))
x, y = x[:n], y[:n]
norm = max(np.abs(x).max(), 1e-9)
x, y = x / norm, y / norm

rng = np.random.default_rng(7)
W1 = rng.normal(0, 0.5, (1, H)); b1 = np.zeros(H)
W2 = rng.normal(0, 0.5 / np.sqrt(H), (H, H)); b2 = np.zeros(H)
W3 = rng.normal(0, 0.5 / np.sqrt(H), (H, 1)); b3 = np.zeros(1)
params = [W1, b1, W2, b2, W3, b3]
m = [np.zeros_like(p) for p in params]
v = [np.zeros_like(p) for p in params]
lr, beta1, beta2, eps = 3e-3, 0.9, 0.999, 1e-8

idx = rng.permutation(n)[: min(n, 400_000)]
xs, ys = x[idx], y[idx]
BATCH = 8192
step = 0
base = float(np.mean((xs - ys) ** 2))
for epoch in range(EPOCHS):
    order = rng.permutation(len(xs))
    for s in range(0, len(xs), BATCH):
        bi = order[s:s + BATCH]
        xb = xs[bi][:, None]; yb = ys[bi][:, None]
        a1 = np.tanh(xb @ W1 + b1)
        a2 = np.tanh(a1 @ W2 + b2)
        out = a2 @ W3 + b3
        d = 2.0 * (out - yb) / len(bi)
        gW3 = a2.T @ d; gb3 = d.sum(0)
        d2 = (d @ W3.T) * (1 - a2 ** 2)
        gW2 = a1.T @ d2; gb2 = d2.sum(0)
        d1 = (d2 @ W2.T) * (1 - a1 ** 2)
        gW1 = xb.T @ d1; gb1 = d1.sum(0)
        step += 1
        for p, g, mi, vi in zip(params, [gW1, gb1, gW2, gb2, gW3, gb3], m, v):
            mi *= beta1; mi += (1 - beta1) * g
            vi *= beta2; vi += (1 - beta2) * g * g
            p -= lr * (mi / (1 - beta1 ** step)) / (np.sqrt(vi / (1 - beta2 ** step)) + eps)

def forward(xa):
    return (np.tanh(np.tanh(xa[:, None] @ W1 + b1) @ W2 + b2) @ W3 + b3)[:, 0]

fit = float(np.mean((forward(xs) - ys) ** 2))
print(f"identity error {10*np.log10(base):.1f} dB -> model error {10*np.log10(fit):.1f} dB "
      f"({len(xs)} samples, {EPOCHS} epochs)")

model = {"in_shape": [None, 1], "layers": [
    {"type": "dense", "shape": [None, H], "activation": "tanh",
     "weights": [W1.tolist(), b1.tolist()]},
    {"type": "dense", "shape": [None, H], "activation": "tanh",
     "weights": [W2.tolist(), b2.tolist()]},
    {"type": "dense", "shape": [None, 1], "activation": "",
     "weights": [W3.tolist(), b3.tolist()]},
]}
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(model))
d = json.loads(OUT.read_text())
Wa, ba = [np.array(w) for w in d["layers"][0]["weights"]]
Wb, bb = [np.array(w) for w in d["layers"][1]["weights"]]
Wc, bc = [np.array(w) for w in d["layers"][2]["weights"]]
probe = np.linspace(-1, 1, 101)
ref = forward(probe)
re = (np.tanh(np.tanh(probe[:, None] @ Wa + ba) @ Wb + bb) @ Wc + bc)[:, 0]
assert np.max(np.abs(ref - re)) < 1e-12
print(f"exported + reload parity OK -> {OUT}")
