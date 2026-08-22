from __future__ import annotations

import io
import sys
import wave
import zipfile
from pathlib import Path

import numpy as np

CELL = 16
BIT_OFFSET = 2

def demod(zip_path: Path) -> bytes:
    z = zipfile.ZipFile(zip_path)
    name = next(n for n in z.namelist() if n.endswith(".wav") and "MACOSX" not in n)
    w = wave.open(io.BytesIO(z.read(name)))
    assert w.getsampwidth() == 1 and w.getnchannels() == 1
    x = (np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8).astype(float) - 128)
    sgn = np.sign(x)
    sgn[sgn == 0] = 1
    zc = np.where(np.diff(sgn) != 0)[0]
    iv = np.diff(zc)
    first_space = zc[int(np.where(iv >= 7)[0][0])]
    n_cells = (len(x) - first_space) // CELL
    starts = first_space + CELL * np.arange(n_cells)
    counts = np.searchsorted(zc, starts + CELL) - np.searchsorted(zc, starts)
    assert set(np.unique(counts)) <= {2, 4}, "ambiguous FSK cells"
    bits = (counts >= 3).astype(np.uint8)
    b = bits[BIT_OFFSET:]
    n = len(b) // 8
    return np.packbits(b[:n * 8].reshape(-1, 8), axis=1, bitorder="little").tobytes()

def main():
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    data = demod(src)
    out.write_bytes(data)
    print(f"{out}  {len(data)} bytes  header {data[:10].hex()}")

if __name__ == "__main__":
    main()
