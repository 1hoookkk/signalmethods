from __future__ import annotations

import io
import sys
import wave
import zipfile
from pathlib import Path

import numpy as np

T = 4
SYNC = (11, 5, 3, 13)


def _runs(x):
    sgn = np.sign(x)
    sgn[sgn == 0] = 1
    edges = np.where(np.diff(sgn) != 0)[0] + 1
    lengths = np.diff(np.concatenate([[0], edges, [len(x)]]))
    return edges, lengths, sgn


def demod(zip_path: Path):
    z = zipfile.ZipFile(zip_path)
    name = next(n for n in z.namelist() if n.endswith(".wav") and "MACOSX" not in n)
    w = wave.open(io.BytesIO(z.read(name)))
    assert w.getsampwidth() == 1 and w.getnchannels() == 1
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).astype(float) - 128.0
    edges, lengths, sgn = _runs(x)

    hit = None
    for i in range(len(lengths) - 4):
        if tuple(lengths[i:i + 4]) == SYNC:
            hit = i
            break
    if hit is None:
        raise SystemExit("frame sync 11 5 3 13 not found")

    start_edge = edges[hit + 3]
    neg = [e for e in edges if e >= start_edge and sgn[e] < 0]
    iv = np.diff(np.array(neg))
    cells = np.rint(iv / T).astype(int)
    keep = (cells >= 2) & (cells <= 4)

    chan = []
    for k, ok in zip(cells, keep):
        if not ok:
            break
        chan.extend([0] * (k - 1))
        chan.append(1)
    chan = np.array(chan, dtype=np.uint8)

    best = None
    for phase in (0, 1):
        d = chan[phase::2]
        other = chan[1 - phase::2]
        n = len(other) // 8
        clk = np.packbits(other[:n * 8].reshape(-1, 8), bitorder="little")
        purity = float(np.mean(clk == 0x55)) if n else 0.0
        if best is None or purity > best[0]:
            best = (purity, phase, d)
    purity, phase, data = best
    n = len(data) // 8
    out = np.packbits(data[:n * 8].reshape(-1, 8), bitorder="little").tobytes()
    return out, dict(sync_run_index=hit, clock_phase=1 - phase,
                     clock_0x55_fraction=purity, channel_bits=len(chan))


def main():
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    data, info = demod(src)
    dst.write_bytes(data)
    counts = np.bincount(np.frombuffer(data, dtype=np.uint8), minlength=256)
    p = counts[counts > 0] / len(data)
    ent = float(-(p * np.log2(p)).sum())
    print(f"{dst}  {len(data)} bytes")
    print(f"  entropy {ent:.2f}  zeros {100 * counts[0] / len(data):.0f}%")
    for k, v in info.items():
        print(f"  {k} {v}")


if __name__ == "__main__":
    main()
