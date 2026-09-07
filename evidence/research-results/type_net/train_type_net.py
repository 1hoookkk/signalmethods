import glob
import json
import math
import os
import struct
import sys

import numpy as np
import torch

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
BODIES = os.path.join(ROOT, "plugin", "presets", "p2k")
OUT = os.path.dirname(os.path.abspath(__file__))
DATUM_HZ = 44100.0

TYPES = {
    "megasweepz": "LPF", "early_rizer": "LPF", "millennium": "LPF", "klub_klassik": "LPF", "bassbox_303": "LPF",
    "dj_alkaline": "EQ+", "ace_of_bass": "EQ+", "tb_or_not_tb": "EQ+", "boland_bass": "EQ+", "bass_tracer": "EQ+", "rogue_hertz": "EQ+",
    "razor_blades": "EQ-", "radio_craze": "EQ-",
    "multi_q_vox": "VOW", "ooh_to_eee": "VOW", "talking_hedz": "VOW", "eeh_to_aah": "VOW", "ubu_orator": "VOW", "deep_bouche": "VOW",
    "freak_shifta": "PHA", "cruz_pusher": "PHA",
    "angelz_hairz": "FLG", "dream_weava": "FLG",
    "meaty_gizmo": "REZ", "dead_ringer": "REZ", "zoom_peaks": "REZ", "acid_ravage": "REZ", "bass_o_matic": "REZ", "lucifer_s_q": "REZ", "tooth_comb": "REZ",
    "ear_bender": "WAH",
    "fuzzi_face": "DST",
    "klang_kling": "SFX",
}
CLASSES = ["LPF", "EQ+", "EQ-", "VOW", "PHA", "FLG", "REZ", "WAH", "DST", "SFX"]

SCALE = [2.0 ** -(15 - e) for e in range(16)]


def decode_word(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    exponent = (u >> 12) & 0xF
    mantissa = float(u & 0xFFF)
    x = mantissa / 4096.0 if exponent == 0 else (mantissa + 4096.0) / 8192.0
    return x * SCALE[exponent]


def interpolate_word(a, b, fraction):
    return int(a) + int(float(int(b) - int(a)) * fraction)


def read_body240(path):
    raw = open(path, "rb").read()
    assert len(raw) == 240, path
    words = struct.unpack("<120H", raw)
    return [[list(words[(c * 6 + s) * 5:(c * 6 + s) * 5 + 5]) for s in range(6)] for c in range(4)]


def lerp_corner(body, morph, q):
    out = []
    for s in range(6):
        section = []
        for w in range(5):
            e0 = interpolate_word(body[0][s][w], body[1][s][w], morph)
            e1 = interpolate_word(body[2][s][w], body[3][s][w], morph)
            section.append(interpolate_word(e0, e1, q))
        out.append(section)
    return out


def pair_geometry(d_p, d_q):
    q = 1.0 - d_q
    p = 4.0 * d_p - 1.0 - q
    if q <= 0.0:
        return 0.0, 0.0
    radius = math.sqrt(q)
    cosine = max(-1.0, min(1.0, -p / (2.0 * radius)))
    return math.acos(cosine) / (2.0 * math.pi) * DATUM_HZ, radius


def features(corner):
    f = []
    for s in range(6):
        d = [decode_word(w) for w in corner[s]]
        zero_hz, zero_r = pair_geometry(d[0], d[1])
        pole_hz, pole_r = pair_geometry(d[2], d[3])
        gain = 4.0 * d[4]
        f += [
            math.log2(max(pole_hz, 20.0) / 20.0) / 10.0,
            pole_r,
            math.log2(max(zero_hz, 20.0) / 20.0) / 10.0,
            zero_r,
            math.log10(max(gain, 1e-4)) / 2.0,
        ]
    return f


def dataset(grid):
    xs, ys, names = [], [], []
    for path in sorted(glob.glob(os.path.join(BODIES, "*.body240"))):
        name = os.path.splitext(os.path.basename(path))[0]
        if name not in TYPES:
            continue
        body = read_body240(path)
        label = CLASSES.index(TYPES[name])
        for i in range(grid):
            for j in range(grid):
                xs.append(features(lerp_corner(body, i / (grid - 1), j / (grid - 1))))
                ys.append(label)
                names.append(name)
    return np.array(xs, dtype=np.float32), np.array(ys), np.array(names)


class Net(torch.nn.Module):
    def __init__(self, n_in, n_out):
        super().__init__()
        self.a = torch.nn.Linear(n_in, 32)
        self.b = torch.nn.Linear(32, 16)
        self.c = torch.nn.Linear(16, n_out)

    def forward(self, x):
        return self.c(torch.tanh(self.b(torch.tanh(self.a(x)))))


def train(x, y, epochs=400, seed=0):
    torch.manual_seed(seed)
    net = Net(x.shape[1], len(CLASSES))
    opt = torch.optim.Adam(net.parameters(), lr=3e-3, weight_decay=1e-4)
    xt, yt = torch.tensor(x), torch.tensor(y, dtype=torch.long)
    counts = np.bincount(y, minlength=len(CLASSES)).astype(np.float32)
    weight = torch.tensor(np.where(counts > 0, counts.sum() / np.maximum(counts, 1) / len(CLASSES), 0.0), dtype=torch.float32)
    loss_fn = torch.nn.CrossEntropyLoss(weight=weight)
    for _ in range(epochs):
        opt.zero_grad()
        loss = loss_fn(net(xt), yt)
        loss.backward()
        opt.step()
    return net


def predict(net, x):
    with torch.no_grad():
        return torch.softmax(net(torch.tensor(x)), dim=1).numpy()


def export_rtneural(net, path):
    layers = []
    for lin, act in ((net.a, "tanh"), (net.b, "tanh"), (net.c, "softmax")):
        w = lin.weight.detach().numpy().T.tolist()
        b = lin.bias.detach().numpy().tolist()
        layers.append({"type": "dense", "shape": [None, lin.out_features], "activation": act, "weights": [w, b]})
    json.dump({"in_shape": [None, net.a.in_features], "layers": layers}, open(path, "w"))


def main():
    grid = 9
    x, y, names = dataset(grid)
    bodies = sorted(set(names))
    report = []
    right = 0
    for held in bodies:
        train_mask = names != held
        net = train(x[train_mask], y[train_mask])
        p = predict(net, x[~train_mask]).mean(axis=0)
        guess = CLASSES[int(p.argmax())]
        truth = TYPES[held]
        right += guess == truth
        report.append((held, truth, guess, float(p.max()), float(p[CLASSES.index(truth)])))
    lines = ["body  truth  guess  p(guess)  p(truth)"]
    for held, truth, guess, pg, pt in report:
        lines.append(f"{held:14s} {truth:4s} {guess:4s} {pg:.2f} {pt:.2f}" + ("" if truth == guess else "   x"))
    lines.append(f"leave-one-body-out: {right}/{len(bodies)} bodies named right by their held-out square")
    final = train(x, y)
    p_all = predict(final, x).argmax(axis=1)
    lines.append(f"resubstitution: {(p_all == y).mean() * 100:.1f}% of {len(y)} chip-lerp corners ({grid}x{grid} per body)")
    export_rtneural(final, os.path.join(OUT, "type_net_rtneural.json"))
    text = "\n".join(lines)
    open(os.path.join(OUT, "type_net_report.txt"), "w").write(text + "\n")
    print(text)


if __name__ == "__main__":
    main()
