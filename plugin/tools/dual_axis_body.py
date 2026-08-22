from __future__ import annotations
import argparse
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from x3_morph_compiler import (  # noqa: E402
    PAD, PROBE_RATE, apply_gain_budget, load_lib, probe_body,
    compile_section, render_visual_compiler, BODY_DIR, OUT_DIR,
    BASE, SCALE,
)

FAMILY = 1

ZERO_OFF = -64
GAP = 48
SECTIONS = 6
SCALE_WORD = 0xe000

def _row(freq_byte: int, zero_off: int, pole_off: int) -> list[int]:
    freq = ((SCALE[FAMILY] * freq_byte) >> 7) + BASE[FAMILY]
    rad = ((freq * 0x7c) >> 8) + 0x76
    return [
        freq << 8,
        int(np.clip(rad + zero_off, 0, 255)) << 8,
        freq << 8,
        int(np.clip(rad + pole_off, 0, 255)) << 8,
        SCALE_WORD,
    ]

def build_body(f_lo: int, f_hi: int, sections: int = SECTIONS) -> bytes:
    corner_spec = [
        (f_lo, False),
        (f_lo, True),
        (f_hi, False),
        (f_hi, True),
    ]
    body = bytearray()
    for freq_byte, open_q in corner_spec:
        z, p = (ZERO_OFF, ZERO_OFF + GAP) if open_q else (0, 0)
        rows = [_row(freq_byte, z, p) for _ in range(sections)]
        while len(rows) < 6:
            rows.append(list(PAD))
        for row in rows:
            for w in row:
                body += struct.pack("<H", w)
    return bytes(body)

def response_db(body: bytes, morph: float, q: float, lib, freqs: np.ndarray) -> np.ndarray:
    rows = probe_body(body, morph, q, lib)
    z1 = np.exp(-1j * 2.0 * np.pi * freqs / PROBE_RATE)
    z2 = z1 * z1
    total = np.zeros_like(freqs)
    for b0, b1, b2, a1, a2 in rows:
        num = np.abs(b0 + b1 * z1 + b2 * z2)
        den = np.abs(1.0 + a1 * z1 + a2 * z2)
        total += 20.0 * np.log10(np.maximum(num / np.maximum(den, 1e-30), 1e-30))
    return total

def null_hz(body: bytes, morph: float, q: float, lib) -> tuple[float, float]:
    freqs = np.geomspace(20.0, 20_000.0, 8192)
    db = response_db(body, morph, q, lib, freqs)
    i = int(np.argmin(db))
    return float(freqs[i]), float(db[i])

def freq_map(lib) -> list[tuple[int, float]]:
    table = []
    for fb in range(0, 128):
        body = build_body(fb, fb, sections=1)
        hz, _ = null_hz(body, 1.0, 0.0, lib)
        table.append((fb, hz))
    return table

def byte_for_hz(table, target_hz: float) -> int:
    return min(table, key=lambda r: abs(np.log(r[1] / target_hz)))[0]

def plot_dual_axis(body: bytes, name: str, lib):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    freqs = np.geomspace(30.0, 20_000.0, 2048)
    steps = [0.0, 0.25, 0.5, 0.75, 1.0]
    colours = plt.cm.plasma(np.linspace(0.0, 0.85, len(steps)))

    fig, axes = plt.subplots(1, 2, figsize=(18, 6))
    for ax, axis in zip(axes, ("morph", "q")):
        for v, c in zip(steps, colours):
            m, q = (v, 0.0) if axis == "morph" else (1.0, v)
            label = f"{'Morph' if axis == 'morph' else 'Q'}={int(v * 100)}%"
            ax.plot(freqs, response_db(body, m, q, lib, freqs), color=c, lw=2, label=label)
        ax.set_title(f"{name} — {'MORPH opens the notch (Q=0)' if axis == 'morph' else 'Q picks the band (Morph=100)'}",
                     fontweight="bold")
        ax.set_xscale("log")
        ax.set_xlim(30, 20_000)
        ax.set_ylim(-60, 20)
        ax.set_xlabel("Hz")
        ax.set_ylabel("dB")
        ax.grid(alpha=0.3)
        ax.legend(loc="upper left")

    fig.tight_layout()
    out = OUT_DIR / f"{name.replace(' ', '_')}_dual_axis.png"
    fig.savefig(out, dpi=110)
    plt.close(fig)
    print(f"  Dual-axis:    {out}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", action="store_true")
    ap.add_argument("--build", action="store_true")
    args = ap.parse_args()

    lib = load_lib()
    table = freq_map(lib)

    if args.map:
        for fb, hz in table:
            if fb % 4 == 0:
                print(f"  byte {fb:3d} -> {hz:9.1f} Hz")
        print(f"\n  range: {table[0][1]:.1f} Hz .. {table[-1][1]:.1f} Hz")
        return

    if not args.build:
        ap.print_help()
        return

    lo_byte, hi_byte = table[0][0], table[-1][0]
    targets = [("Carve", lo_byte, hi_byte)]

    for name, f_lo, f_hi in targets:
        body = build_body(f_lo, f_hi)

        print(f"\n{name}")
        print(f"  bytes {f_lo} -> {f_hi}")
        for q in (0.0, 0.25, 0.5, 0.75, 1.0):
            hz, depth = null_hz(body, 1.0, q, lib)
            flat = null_hz(body, 0.0, q, lib)[1]
            m = q
            fr = np.geomspace(20.0, 20_000.0, 8192)
            db = response_db(body, 1.0, q, lib, fr)
            under = fr[db <= -10.0]
            oct_w = np.log2(under[-1] / under[0]) if len(under) > 1 else 0.0
            print(f"    Q {int(q*100):3d}%   null {hz:8.1f} Hz   "
                  f"Morph100 {depth:8.1f} dB   Morph0 {flat:5.1f} dB   "
                  f"-10dB width {oct_w:.3f} oct")

        out = BODY_DIR / f"DX_{name.lower().replace(' ', '_')}.body240"
        out.write_bytes(body)
        print(f"  Body: {out}")
        render_visual_compiler(body, name, lib)
        plot_dual_axis(body, name, lib)

if __name__ == "__main__":
    main()
