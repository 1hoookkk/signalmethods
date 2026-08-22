#!/usr/bin/env python3
from __future__ import annotations

import ctypes
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, RT_DOUBLES  # noqa: E402

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int

GRID = np.geomspace(30.0, 18_000.0, 256)
HEIGHT = 26
WIDTH = 96
DB_LO, DB_HI = -60.0, 30.0
BLOCKS = "  ▁▂▃▄▅▆▇█"

def response_db(body: bytes, m: float, q: float, rate: float) -> np.ndarray:
    z1 = np.exp(-1j * 2.0 * np.pi * GRID / rate)
    z2 = z1 * z1
    c = (ctypes.c_double * RT_DOUBLES)()
    mr = ctypes.c_double(); un = ctypes.c_uint32(); nf = ctypes.c_uint32()
    buf = ctypes.create_string_buffer(body, 240)
    if lib.trench_packed_probe_at(buf, 240, m, q, rate, c, ctypes.byref(mr),
                                  ctypes.byref(un), ctypes.byref(nf)) != 0 \
       or un.value or nf.value:
        return np.full_like(GRID, -60.0)
    cc = np.ctypeslib.as_array(c).reshape(6, 5)
    out = np.zeros_like(GRID)
    for b0, b1, b2, a1, a2 in cc:
        h = (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)
        out += 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))
    return out

def hz_ticks() -> dict[int, str]:
    out = {}
    for hz in (30, 100, 300, 1000, 3000, 10_000, 18_000):
        frac = np.log(hz / GRID[0]) / np.log(GRID[-1] / GRID[0])
        out[round(frac * (WIDTH - 1))] = f"{hz:g}"
    return out

def frame(body: bytes, m: float, q: float, rate: float, name: str) -> str:
    db = response_db(body, m, q, rate)
    rows = [[" "] * WIDTH for _ in range(HEIGHT)]
    xs = np.linspace(0, WIDTH - 1, len(GRID)).astype(int)
    rows_at = np.clip(((db - DB_LO) / (DB_HI - DB_LO)) * (HEIGHT - 1), 0, HEIGHT - 1)
    rows_at = (HEIGHT - 1) - rows_at
    for x, r in zip(xs, rows_at):
        rows[int(r)][x] = "█"
    for x in range(WIDTH):
        col = rows_at[xs == x]
        if len(col) == 0:
            continue
        top = int(col.min())
        rows[top][x] = "▀"
    for r in (0, HEIGHT // 2, HEIGHT - 1):
        for x in range(WIDTH):
            if rows[r][x] == " ":
                rows[r][x] = "·"
    for x, lab in hz_ticks().items():
        for i, ch in enumerate(lab):
            if x + i < WIDTH:
                rows[HEIGHT - 1][x + i] = ch
    lines = ["".join(r) for r in rows]
    lines[0] = "+30 " + lines[0][4:]
    lines[HEIGHT // 2] = "  0 " + lines[HEIGHT // 2][4:]
    lines[HEIGHT - 1] = "-60 " + lines[HEIGHT - 1][4:]
    for i in range(2, len(db) - 2):
        if db[i] > db[i - 1] and db[i] > db[i + 1] and db[i] - db[i + 8: i + 16].min() > 3.0:
            x = int(np.linspace(0, WIDTH - 1, len(GRID))[i])
            r = int((HEIGHT - 1) - ((db[i] - DB_LO) / (DB_HI - DB_LO)) * (HEIGHT - 1))
            if 0 <= r - 1 < HEIGHT:
                rows[r - 1][x] = "*"
    lines = ["".join(r) for r in rows]

    P = WIDTH + 8
    top = "┌" + "─" * (P - 2) + "┐"
    title = "│  TRENCH · MORPHING FILTER · PATCH 001" + " " * (P - 2 - 38) + "│"
    bezel_top = "│  " + "┌" + "─" * (WIDTH + 2) + "┐" + " " * (P - 2 - (WIDTH + 6)) + "│"
    out = [top, title, bezel_top]
    for ln in lines:
        out.append("│  │ " + ln + " │" + " " * (P - 2 - (WIDTH + 6)) + "│")
    out.append("│  " + "└" + "─" * (WIDTH + 2) + "┘" + " " * (P - 2 - (WIDTH + 6)) + "│")
    def bar(label, val):
        w = int(round(val * WIDTH))
        return "│  " + f"{label:<6}" + "█" * w + "·" * (WIDTH - w) + f" {int(val*100):3d}%" \
            + " " * (P - 2 - (6 + WIDTH + 6)) + "│"
    out.append(bar("MORPH", m))
    out.append(bar("Q", q))
    wheel = "│  MOD WHEEL "
    pos = int(round(m * (WIDTH - 4)))
    wheel += "─" * pos + "●" + "─" * (WIDTH - 4 - pos)
    wheel += " " * (P - 2 - (11 + WIDTH)) + "│"
    out.append(wheel)
    out.append("└" + "─" * (P - 2) + "┘")
    return "\n".join(out)

def main():
    argv = sys.argv[1:]
    save = None
    if "--save" in argv:
        save = Path(argv[argv.index("--save") + 1])
        argv = [a for i, a in enumerate(argv) if i != argv.index("--save") and i != argv.index("--save") + 1]
    body_path = Path(argv[0]).resolve()
    rate = float(argv[1]) if len(argv) > 1 else 48_000.0
    body = body_path.read_bytes()
    assert len(body) == 240, f"{body_path.name}: {len(body)} bytes, want 240"
    name = body_path.stem

    frames = []
    for i in range(13):
        m = i / 12.0
        fr = frame(body, m, 0.0, rate, name)
        frames.append(fr)
        if save is not None:
            save.mkdir(parents=True, exist_ok=True)
            (save / f"{name}_m{int(m*100):03d}.txt").write_text(fr + "\n")
        print("\033[H\033[2J", end="")
        print(fr)
        print(f"{name}  ·  {rate:.0f} Hz  ·  wheel position {int(m*100)}%")
        import time
        time.sleep(0.25)
    if save is not None:
        print(f"saved {len(frames)} frames to {save}/")

if __name__ == "__main__":
    main()
