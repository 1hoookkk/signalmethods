from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OCTAVES = 10.0
SR = 39_062.5
REF_HZ = SR / 2048.0
LANE_COL = ["#c96a54", "#4a7c9b", "#7b9e6b", "#9b7bb0",
            "#d9a83c", "#5aa9a0"]
LANE_NAMES = [f"S{n}" for n in range(1, 7)]

def theta_prime(hz):
    theta = 2.0 * math.pi * max(hz, REF_HZ) / SR
    return math.pi * (OCTAVES + math.log2(theta / math.pi)) / OCTAVES

def resonance_db(r, for_zero):
    if for_zero:
        return 20.0 * math.log10(1.0 / max(1.0 - min(r, 0.999999), 1e-9))
    return 20.0 * math.log10(1.0 / max(1.0 - min(r, 0.999999), 1e-9))

def st(a, b):
    return 12.0 * math.log2(a / b)

def load(name):
    f = Path("dossiers/characters") / f"P2k_{name}.json"
    if not f.exists():
        f = Path("dossiers/characters") / f"{name}.json"
    if not f.exists():
        raise SystemExit(f"no dossier {name}")
    return json.loads(f.read_text())

def main():
    max_st = 3.0
    names = []
    for a in sys.argv[1:]:
        if a.startswith("--max-st="):
            max_st = float(a.split("=")[1])
        else:
            names.append(a)
    if not names:
        names = ["003_Millennium", "022_DeepBouche"]

    fig, axs = plt.subplots(len(names), 1, figsize=(11, 4.2 * len(names)),
                            subplot_kw={"projection": "polar"})
    if len(names) == 1:
        axs = [axs]

    for row, name in zip(axs, names):
        d = load(name)
        row.set_title(f"{d['x3_type']}  {d['name']}  -  poles (fill) / zeros (hollow), "
                      f"landing on a foreign pole < {max_st:.1f} st shown as an edge",
                      fontsize=10, pad=16)
        seen = set()
        for ci, (cn, rows) in enumerate(d["corners"].items()):
            pts = []
            for i, r in enumerate(rows):
                pts.append((i, r["pole"]["hz"], r["pole"]["radius"], False))
                pts.append((i, r["zero"]["hz"], r["zero"]["radius"], True))
            poles = [(li, p["pole"]["hz"]) for li, p in enumerate(rows)]
            for lane, hz, r, isz in pts:
                if hz <= 0:
                    continue
                th = theta_prime(hz)
                rr = resonance_db(r, isz)
                row.scatter(th, rr, s=46 if not isz else 40,
                            facecolors="none" if isz else LANE_COL[lane],
                            edgecolors=LANE_COL[lane],
                            alpha=0.9, zorder=4)
                if isz:
                    best = None
                    for v_lane, v_hz in poles:
                        if v_lane == lane or v_hz <= 0:
                            continue
                        dist = abs(st(hz, v_hz))
                        if best is None or dist < best[0]:
                            best = (dist, v_lane, v_hz)
                    if best and best[0] < max_st:
                        dd, v_lane, v_hz = best
                        sep = 12 * math.log2(hz / v_hz)
                        th_v = theta_prime(v_hz)
                        rr_v = resonance_db(
                            rows[v_lane]["pole"]["radius"], False)
                        row.plot([th, th_v], [rr, rr_v],
                                 color=LANE_COL[lane], lw=0.7, alpha=0.45)
                        key = (lane + 1, v_lane + 1)
                        if key not in seen:
                            seen.add(key)
                            row.annotate(
                                f"S{lane+1}->S{v_lane+1} {sep:+.2f} st",
                                (th, rr),
                                textcoords="offset points", xytext=(4, 4),
                                fontsize=7, color=LANE_COL[lane])
        row.set_ylim(0, 90)
        row.set_thetamin(0)
        row.set_thetamax(180)
        row.set_yticks([20, 40, 60, 80])
        row.set_yticklabels(["20 dB", "40", "60", "80"], fontsize=7)
        row.set_xticks([math.pi * k / OCTAVES for k in range(0, 11, 2)])
        row.set_xticklabels([f"{REF_HZ * 2 ** k:.0f}"
                             for k in range(0, 11, 2)], fontsize=7)
        row.grid(alpha=0.3)

    fig.suptitle("ARMAdillo plane, ROM ground truth - a zero's nearest foreign pole",
                 fontsize=12, y=0.995)
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    out = Path("plots/roma_zero_armadillo.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, dpi=140)
    print(f"wrote {out}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
