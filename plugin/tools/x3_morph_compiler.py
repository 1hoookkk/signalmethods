r"""X3 Morph Designer compiler — XML templates → body240 + visual compiler plots.

Parses <designer-section> blocks from Emulator X filter template XML,
compiles to packed u16 corner words via the type 1-3 grammar, and renders
the cumulative signal-so-far response — the same view E-mu's internal
visual compiler gave their designers.

Grammar source: FUN_1802c6590 (Morph Designer compiler), documented in
ref/ghidra_extracts/morphdesigner_types.md.

Usage:
  python tools/x3_morph_compiler.py "Bass Shaper"          # compile + plot one
  python tools/x3_morph_compiler.py --all                    # compile all 71
  python tools/x3_morph_compiler.py --plot-only "Twin Peaks" # re-plot existing
"""
from __future__ import annotations
import argparse
import ctypes as C
import os
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DLL = ROOT / "target" / "release" / "trench_core.dll"
XML_DIR = ROOT / "ref" / "x3_morph_designer" / "templates"
OUT_DIR = ROOT / "evidence" / "morph_compiler"
BODY_DIR = ROOT / "bodies" / "candidates"

BASE = [18, 18, 4, 1]
SCALE = [220, 220, 200, 177]
RATE_FAMILY = 1

PAD = [0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000]

def parse_template(path: Path) -> dict:
    tree = ET.parse(path)
    root = tree.getroot()

    name = root.get("name", path.stem)

    sections = []
    filter_el = root.find("filter")
    if filter_el is None:
        return {"name": name, "sections": sections}

    for ds in filter_el.findall("designer-section"):
        def _int(tag, default=0):
            el = ds.find(tag)
            if el is not None and el.text is not None:
                return int(el.text.strip())
            return default

        sec = {
            "type": _int("type"),
            "low_freq": _int("low-freq"),
            "low_gain": _int("low-gain"),
            "high_freq": _int("high-freq"),
            "high_gain": _int("high-gain"),
        }
        sections.append(sec)

    def _float(tag, default=0.0):
        el = filter_el.find(tag)
        if el is not None and el.text is not None:
            return float(el.text.strip())
        return default

    return {
        "name": name,
        "frequency": _float("frequency"),
        "gain": _float("gain"),
        "sections": sections,
    }

def compile_section(sec: dict, endpoint: str, shift: int = -32) -> list[int]:
    t = sec["type"]
    if t not in (1, 2, 3):
        return list(PAD)

    family = RATE_FAMILY
    freq_byte = sec["low_freq"] if endpoint == "low" else sec["high_freq"]
    gain_byte = sec["low_gain"] if endpoint == "low" else sec["high_gain"]

    freq = ((SCALE[family] * freq_byte) >> 7) + BASE[family]
    gain = int(np.clip(((gain_byte - 0x40) >> 1) + shift, -32, 31))
    rad = ((freq * 0x7c) >> 8) + 0x76

    w = [0, 0, 0, 0, 0]

    if t == 1:
        w[0] = freq << 8
        w[1] = np.clip(rad + gain, 0, 255) << 8
        w[2] = freq << 8
        w[3] = np.clip(rad - gain, 0, 255) << 8
        w[4] = 0xe000
    elif t == 2:
        if family <= 1:
            w[0] = 0xec00
            w[1] = 0xff00
        else:
            w[0] = 0xe100
            w[1] = 0xf000
        w[2] = freq << 8
        w[3] = np.clip(rad - gain, 0, 255) << 8
        w[4] = (freq + 0xf5) << 8 if family <= 1 else freq << 8
    elif t == 3:
        w[0] = BASE[family] << 8
        w[1] = (((BASE[family] * 0x7c) >> 8) + 0x96) << 8

        emitted_freq = freq
        if family <= 1 and freq > 0xdb and gain < 0:
            emitted_freq = (((freq - 0xdc) * (gain + 0x20)) >> 5) + 0xdc

        w[2] = emitted_freq << 8
        w[3] = np.clip(rad - gain, 0, 255) << 8
        if family <= 1:
            w[4] = ((freq - 18) * -12 - 8192) & 0xffff
        else:
            w[4] = 0xe000

    return [int(x) & 0xffff for x in w]

GAIN_CEILING_DB = 40.0
CORNER_SPREAD_LIMIT_DB = 44.0
PROBE_RATE = 48_000.0

_MINIFLOAT_TABLE = None

def _minifloat_table():
    global _MINIFLOAT_TABLE
    if _MINIFLOAT_TABLE is None:
        sys.path.insert(0, str(ROOT))
        from pyruntime.packed_interp import decode as _dec
        _MINIFLOAT_TABLE = np.array([_dec(w) for w in range(65536)])
    return _MINIFLOAT_TABLE

def _nearest_scale_word(target: float) -> int:
    return int(np.argmin(np.abs(_minifloat_table() - target)))

def _corner_peak_db(body: bytes, morph: float, q: float, lib) -> float:
    rows = probe_body(body, morph, q, lib)
    freqs = np.geomspace(30.0, min(19_200.0, PROBE_RATE * 0.45), 1024)
    z1 = np.exp(-1j * 2.0 * np.pi * freqs / PROBE_RATE)
    z2 = z1 * z1
    total = np.zeros_like(freqs)
    for b0, b1, b2, a1, a2 in rows:
        num = np.abs(b0 + b1 * z1 + b2 * z2)
        den = np.abs(1.0 + a1 * z1 + a2 * z2)
        total += 20.0 * np.log10(np.maximum(num / np.maximum(den, 1e-30), 1e-30))
    return float(total.max())

def apply_gain_budget(body: bytes, lib, ceiling_db: float = GAIN_CEILING_DB):
    words = list(struct.unpack("<120H", body))
    table = _minifloat_table()
    corners = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0), (1.0, 1.0)]
    names = ["M0Q0", "M100Q0", "M0Q100", "M100Q100"]
    report = []

    for ci, ((m, q), cname) in enumerate(zip(corners, names)):
        before = _corner_peak_db(body, m, q, lib)
        if before <= ceiling_db:
            report.append((cname, before, before))
            continue
        per_section = 10.0 ** (-(before - ceiling_db) / 20.0 / 6)
        for si in range(6):
            idx = ci * 6 * 5 + si * 5 + 4
            words[idx] = _nearest_scale_word(table[words[idx]] * per_section)
        body = struct.pack("<120H", *words)
        report.append((cname, before, _corner_peak_db(body, m, q, lib)))

    return struct.pack("<120H", *words), report

def lint_corner_spread(report, name: str) -> str | None:
    peaks = [after for _, _, after in report]
    spread = max(peaks) - min(peaks)
    if spread > CORNER_SPREAD_LIMIT_DB:
        return (f"corner spread {spread:.1f} dB exceeds E-mu's {CORNER_SPREAD_LIMIT_DB:.0f} dB "
                f"(peaks {min(peaks):.1f}..{max(peaks):.1f}) — the Q axis is "
                f"carrying level, not just resonance")
    return None

def compile_template(template: dict, lib=None,
                     budget: bool = True) -> tuple[bytes, str]:
    name = template["name"]
    sections = template["sections"]

    corners = [[], [], [], []]

    shift = -32 + int(np.trunc((template["frequency"] + template["gain"]) * 63.0))

    for si, sec in enumerate(sections):
        low_words = compile_section(sec, "low", shift)
        high_words = compile_section(sec, "high", shift)
        corners[0].append(low_words)
        corners[1].append(high_words)
        corners[2].append(low_words)
        corners[3].append(high_words)

    for c in corners:
        while len(c) < 6:
            c.append(list(PAD))

    body = bytearray()
    for c in corners:
        for stage in c:
            for w in stage:
                body += struct.pack("<H", w)
    body = bytes(body)

    if budget and lib is not None:
        body, report = apply_gain_budget(body, lib)
        for cname, before, after in report:
            if before != after:
                print(f"    budget {cname:9s}{before:8.1f} -> {after:6.1f} dB")
        warning = lint_corner_spread(report, name)
        if warning:
            print(f"    LINT {name}: {warning}")

    return body, name

def probe_body(body: bytes, morph: float, q: float, lib) -> np.ndarray:
    out = (C.c_double * RT_DOUBLES)()
    mr = C.c_double()
    um = C.c_uint32()
    nm = C.c_uint32()
    rc = lib.trench_packed_probe(
        body, len(body), C.c_double(morph), C.c_double(q),
        out, C.byref(mr), C.byref(um), C.byref(nm),
    )
    if rc != 0:
        raise RuntimeError(f"probe rc={rc}")
    return np.array(out).reshape(6, 5)

def stage_response(b0, b1, b2, a1, a2, freqs, sr):
    z = np.exp(-1j * 2 * np.pi * freqs / sr)
    num = b0 + b1 * z + b2 * z * z
    den = 1.0 + a1 * z + a2 * z * z
    return 20 * np.log10(np.abs(num) / (np.abs(den) + 1e-12))

def render_visual_compiler(body: bytes, name: str, lib):
    SR = 48000.0
    freqs = np.geomspace(30, 20000, 800)

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    corners = [(0, 0, "M0 Q0"), (1, 0, "M100 Q0"),
               (0, 1, "M0 Q100"), (1, 1, "M100 Q100")]

    for (m, q, label), ax in zip(corners, axes.flat):
        rows = probe_body(body, float(m), float(q), lib)

        cumulative = np.zeros_like(freqs)
        colors = plt.cm.viridis(np.linspace(0.15, 0.95, 6))
        for si in range(6):
            b0, b1, b2, a1, a2 = rows[si]
            stage_db = stage_response(b0, b1, b2, a1, a2, freqs, SR)
            cumulative += stage_db
            alpha = 0.55 if si < 5 else 1.0
            lw = 1.0 if si < 5 else 1.8
            ax.plot(freqs, cumulative, color=colors[si], lw=lw, alpha=alpha,
                    label=f"S{si + 1}" if si == 5 else None)

        ax.set_title(label, fontsize=11, fontweight="bold")
        ax.set_xscale("log")
        ax.set_ylim(-60, 30)
        ax.set_xlim(30, 20000)
        ax.grid(True, alpha=0.3)
        if label.endswith("Q0"):
            ax.set_ylabel("dB")
        if "M100" in label:
            ax.legend(fontsize=7, loc="lower left")

    fig.suptitle(f"{name} — Cumulative Signal-So-Far (E-mu Visual Compiler View)",
                 fontsize=13, fontweight="bold", y=0.98)
    fig.tight_layout(rect=[0, 0, 1, 0.95])

    out1 = OUT_DIR / f"{name.replace(' ', '_')}_corners.png"
    out1.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out1, dpi=150)
    plt.close(fig)
    print(f"  Corner sheet: {out1}")

    fig, ax = plt.subplots(figsize=(12, 6))
    morphs = [0.0, 0.25, 0.5, 0.75, 1.0]
    colors = plt.cm.plasma(np.linspace(0.1, 0.9, 5))

    for mi, m in enumerate(morphs):
        rows = probe_body(body, float(m), 0.0, lib)
        total = np.zeros_like(freqs)
        for si in range(6):
            b0, b1, b2, a1, a2 = rows[si]
            total += stage_response(b0, b1, b2, a1, a2, freqs, SR)
        ax.plot(freqs, total, color=colors[mi], lw=1.5,
                label=f"Morph={int(m * 100)}%")

    ax.set_xscale("log")
    ax.set_ylim(-60, 30)
    ax.set_xlim(30, 20000)
    ax.set_title(f"{name} — Morph Ride (Q=0)", fontsize=12, fontweight="bold")
    ax.set_xlabel("Hz")
    ax.set_ylabel("dB")
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()

    out2 = OUT_DIR / f"{name.replace(' ', '_')}_ride.png"
    fig.savefig(out2, dpi=150)
    plt.close(fig)
    print(f"  Morph ride:   {out2}")

def load_lib():
    lib = C.CDLL(str(DLL))
    try:
        lib.trench_num_stages.restype = C.c_uint32
        lib.trench_num_coeffs.restype = C.c_uint32
        RT_DOUBLES = int(lib.trench_num_stages()) * int(lib.trench_num_coeffs())
    except AttributeError:
        RT_DOUBLES = 30
    globals()['RT_DOUBLES'] = RT_DOUBLES
    lib.trench_packed_probe.restype = C.c_int32
    lib.trench_packed_probe.argtypes = [
        C.c_char_p, C.c_size_t, C.c_double, C.c_double,
        C.POINTER(C.c_double), C.POINTER(C.c_double),
        C.POINTER(C.c_uint32), C.POINTER(C.c_uint32),
    ]
    return lib

def compile_and_plot(xml_path: Path, lib, save_body: bool = True):
    name = xml_path.stem
    print(f"\n{name}")
    template = parse_template(xml_path)

    valid = sum(1 for s in template["sections"] if s["type"] in (1, 2, 3))
    print(f"  Sections: {len(template['sections'])} ({valid} active types 1-3)")

    if valid == 0:
        print(f"  SKIP: no compilable sections")
        return

    body, _ = compile_template(template, lib)

    if save_body:
        stem = name.lower().replace(" ", "_").replace("-", "_")
        body_path = BODY_DIR / f"MD_{stem}.body240"
        body_path.parent.mkdir(parents=True, exist_ok=True)
        body_path.write_bytes(body)
        print(f"  Body: {body_path}")

    render_visual_compiler(body, name, lib)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("name", nargs="?", help="Template name (partial match)")
    ap.add_argument("--all", action="store_true", help="Compile all 71 templates")
    ap.add_argument("--plot-only", metavar="NAME", help="Re-plot from existing body")
    args = ap.parse_args()

    lib = load_lib()

    if args.plot_only:
        stem = args.plot_only.lower().replace(" ", "_").replace("-", "_")
        body_path = BODY_DIR / f"MD_{stem}.body240"
        if not body_path.exists():
            print(f"Body not found: {body_path}")
            sys.exit(1)
        render_visual_compiler(body_path.read_bytes(), args.plot_only, lib)
        return

    if args.all:
        for xml_path in sorted(XML_DIR.glob("*.xml")):
            try:
                compile_and_plot(xml_path, lib)
            except Exception as e:
                print(f"  ERROR: {e}")
        return

    if args.name:
        matches = list(XML_DIR.glob(f"*{args.name}*.xml"))
        if not matches:
            print(f"No template matching '{args.name}'")
            sys.exit(1)
        for xml_path in matches:
            compile_and_plot(xml_path, lib)
        return

    print("Available templates:")
    for xml_path in sorted(XML_DIR.glob("*.xml")):
        print(f"  {xml_path.stem}")

if __name__ == "__main__":
    main()
