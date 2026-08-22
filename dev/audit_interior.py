import math
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "plugin" / "pyruntime"))
from ffi import probe, roots_from_coeffs  # noqa: E402

SR = 44100.0
GRID = np.geomspace(20.0, 0.49 * SR, 256)
W = 2 * np.pi * GRID / SR
Z1 = np.exp(-1j * W)
Z2 = Z1 * Z1
ERB = 24.7 * (4.37 * GRID / 1000 + 1)
WT = (1.0 / ERB) / (1.0 / ERB).sum()
MORPHS = np.linspace(0, 1, 21)
QS = np.linspace(0, 1, 5)


def response_db(body, m, q):
    c = probe(body, m, q, SR)
    if c is None:
        return None, None
    H = np.ones_like(Z1)
    rmax = 0.0
    for b0, b1, b2, a1, a2 in c:
        H *= (b0 + b1 * Z1 + b2 * Z2) / (1 + a1 * Z1 + a2 * Z2)
        rmax = max(rmax, roots_from_coeffs((b0, b1, b2, a1, a2), False, SR)[1])
    return 20 * np.log10(np.abs(H) + 1e-30), rmax


def wdist(a, b):
    d = a - b
    d = d - np.sum(WT * d)
    return math.sqrt(float(np.sum(WT * d * d)))


def main():
    body = pathlib.Path(sys.argv[1]).read_bytes()
    assert len(body) == 240
    table = {}
    refused = []
    for q in QS:
        for m in MORPHS:
            r, rmax = response_db(body, float(m), float(q))
            if r is None:
                refused.append((m, q))
            else:
                table[(round(m, 3), round(q, 3))] = (r, rmax)
    print(f"{sys.argv[1]}: {len(table)} interior points probed, {len(refused)} refused")
    for q in QS:
        row = [table.get((round(m, 3), round(q, 3))) for m in MORPHS]
        steps = [wdist(row[i][0], row[i + 1][0]) for i in range(len(row) - 1) if row[i] and row[i + 1]]
        span = wdist(row[0][0], row[-1][0]) if row[0] and row[-1] else float("nan")
        worst = max(range(len(steps)), key=lambda i: steps[i]) if steps else -1
        rmax = max(r[1] for r in row if r)
        print(f"  q {q:4.2f}: corner-to-corner {span:6.2f} dB, max step {max(steps):5.2f} dB at morph {MORPHS[worst]:.2f}->{MORPHS[worst + 1]:.2f}, "
              f"mean step {np.mean(steps):5.2f}, pole r max {rmax:.5f}")
    for m in (0.0, 1.0):
        col = [table.get((round(m, 3), round(q, 3))) for q in QS]
        steps = [wdist(col[i][0], col[i + 1][0]) for i in range(len(col) - 1) if col[i] and col[i + 1]]
        print(f"  morph {m:.0f}: q travel {sum(steps):6.2f} dB, max q step {max(steps):5.2f}")


if __name__ == "__main__":
    main()
