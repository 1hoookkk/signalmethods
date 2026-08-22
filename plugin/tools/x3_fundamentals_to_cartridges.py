"""X3 fixed-class fundamentals -> 4-bank runtime preset cartridges.

Supersedes x3_fundamentals_to_bodies.py, which packed the 44.1k blocks only
and shipped them as single-rate .body240 files. Those play sharp at 48k and an
octave out at 96k.

THE XSTREAM LAW
---------------
The Emulator X3 stores FOUR pre-compiled coefficient banks per filter
(44.1k / 48k / 96k / 192k). They are distinct designs, not rate-converted
copies — proven by cross-rate re-encoding mismatch
(scratchpad/rate_bank_redundancy.py). The engine selects the nearest bank and
plays it verbatim; it never migrates coefficients. We do the same:

    X3F factory presets  -> 4-bank .x3preset.json,  datum_rate = 0, verbatim
    TRENCH originals     -> .body240,               datum_rate > 0, Hz-anchored

The Hz-anchored path exists because a body we author from a measurement has no
factory bank set to select from. E-mu never needed it; they shipped the banks.

CORNER ORDER
------------
Blocks are M0/Q0, M100/Q0, M0/Q100, M100/Q100 — the body240 corner order with
the X3's second control (Res / Body) on the Q axis. No transposition needed.

PROOF PER BANK
--------------
  codec null   our FFI decode vs the df2 reference decode, at that bank's own
               rate, on a grid ending at that rate's Nyquist. Must be < 0.01 dB.
  certify      33x33 stability sweep on the identity-padded bank.

Stability is a property of the words themselves — the minifloat codec decodes
straight to biquad coefficients — so one certify per bank covers every rate it
could be played at.

Usage:
  python tools/x3_fundamentals_to_cartridges.py
  python tools/x3_fundamentals_to_cartridges.py --out bodies/candidates
"""
from __future__ import annotations

import argparse
import ctypes
import json
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
sys.path.insert(0, str(ROOT))
from arma_measure_lib import lib  # noqa: E402
from joint_fit import cascade_db, certify, pack, probe  # noqa: E402
from pyruntime.packed_interp import kernel_to_biquad, words_to_coeffs  # noqa: E402

BLOCKS = ROOT / "ref" / "x3_menu" / "runtime_blocks"
OUT_DEFAULT = ROOT / "bodies" / "candidates"

X3_RATES = [44_100, 48_000, 96_000, 192_000]
NULL_TOLERANCE_DB = 0.01

FUND = [
    ("2_pole_lowpass", 1), ("4_pole_lowpass", 2), ("6_pole_lowpass", 3),
    ("2_pole_highpass", 1), ("4_pole_highpass", 2),
    ("2_pole_bandpass", 1), ("4_pole_bandpass", 2), ("contrary_bandpass", 1),
    ("swept_eq_1_octave", 1), ("swept_eq_2_1_octave", 1),
    ("swept_eq_3_1_octave", 1),
    ("phaser_1", 2), ("phaser_2", 2), ("bat_phaser", 2),
    ("flanger_lite", 3), ("vocal_ah_ay_ee", 3), ("vocal_oo_ah", 3),
]

CONTROLS = {
    "vocal_ah_ay_ee": ("Freq", "Body"),
    "vocal_oo_ah": ("Freq", "Body"),
}
DEFAULT_CONTROLS = ("Freq", "Res")

PRETTY = {
    "2_pole_lowpass": "2 Pole Lowpass",
    "4_pole_lowpass": "4 Pole Lowpass",
    "6_pole_lowpass": "6 Pole Lowpass",
    "2_pole_highpass": "2 Pole Highpass",
    "4_pole_highpass": "4 Pole Highpass",
    "2_pole_bandpass": "2 Pole Bandpass",
    "4_pole_bandpass": "4 Pole Bandpass",
    "contrary_bandpass": "Contrary Bandpass",
    "swept_eq_1_octave": "Swept EQ 1 Oct",
    "swept_eq_2_1_octave": "Swept EQ 2>1 Oct",
    "swept_eq_3_1_octave": "Swept EQ 3>1 Oct",
    "phaser_1": "Phaser 1",
    "phaser_2": "Phaser 2",
    "bat_phaser": "Bat Phaser",
    "flanger_lite": "Flanger Lite",
    "vocal_ah_ay_ee": "Vocal Ah-Ay-Ee",
    "vocal_oo_ah": "Vocal Oo-Ah",
}

def identity_row(rate: float):
    row = (ctypes.c_uint16 * 5)()
    rc = lib.trench_stage_words_from_roots_at(
        (ctypes.c_double * 5)(1000.0, 0.0, 1000.0, 0.0, 1.0), rate, row)
    assert rc == 0, f"identity row encode failed at {rate}"
    return list(row)

def read_block(stem: str, rate: int, active_stages: int):
    path = BLOCKS / f"{stem}_{rate}.raw"
    raw = path.read_bytes()
    w = list(struct.unpack(f"<{len(raw) // 2}H", raw))
    per = active_stages * 5
    if len(w) != 4 * per:
        raise ValueError(
            f"{path.name}: {len(w)} words, expected {4 * per} "
            f"(4 corners x {active_stages} stages x 5)")
    return [[w[c * per + s * 5: c * per + s * 5 + 5] for s in range(active_stages)]
            for c in range(4)]

def df2_corner_db(stages, freqs, rate: float):
    z = np.exp(-1j * 2 * np.pi * freqs / rate)
    total = np.zeros_like(freqs)
    for s in stages:
        b0, b1, b2, a1, a2 = kernel_to_biquad(words_to_coeffs(tuple(s)))
        total += 20 * np.log10(
            np.abs(b0 + b1 * z + b2 * z * z)
            / np.abs(1 + a1 * z + a2 * z * z) + 1e-12)
    return total

def verify_bank(corners, active_stages: int, rate: int):
    ident = identity_row(float(rate))
    words = []
    for c in range(4):
        for s in range(6):
            words += corners[c][s] if s < active_stages else ident
    body = pack(words)

    top = min(16_000.0, rate * 0.45)
    freqs = np.geomspace(40.0, top, 384)

    worst = 0.0
    for (m, q), c in zip([(0, 0), (1, 0), (0, 1), (1, 1)], range(4)):
        rows, _ = probe(body, float(m), float(q), float(rate))
        ours = cascade_db(rows, freqs, float(rate))
        theirs = df2_corner_db(corners[c], freqs, float(rate))
        worst = max(worst, float(np.abs(ours - theirs).max()))

    ok, max_r = certify(body)
    return body, worst, ok, max_r

def build(stem: str, active_stages: int):
    banks, report = {}, []
    for rate in X3_RATES:
        path = BLOCKS / f"{stem}_{rate}.raw"
        if not path.exists():
            report.append((rate, None, None, None, "MISSING"))
            continue
        try:
            corners = read_block(stem, rate, active_stages)
        except ValueError as e:
            report.append((rate, None, None, None, f"BAD: {e}"))
            continue

        _, worst, ok, max_r = verify_bank(corners, active_stages, rate)
        null_ok = worst < NULL_TOLERANCE_DB
        status = "ok" if (ok and null_ok) else (
            "certify FAIL" if not ok else f"null {worst:.4f}dB")
        report.append((rate, worst, ok, max_r, status))

        if ok and null_ok:
            flat = []
            for c in range(4):
                for s in range(active_stages):
                    flat += corners[c][s]
            banks[str(rate)] = flat

    if not banks:
        return None, report

    x, q = CONTROLS.get(stem, DEFAULT_CONTROLS)
    cart = {
        "format": "trench-x3-runtime-preset-v1",
        "name": PRETTY.get(stem, stem),
        "stem": stem,
        "active_stages": active_stages,
        "datum_rate": 0,
        "controls": {"x": x, "q": q},
        "corner_order": ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"],
        "banks": banks,
        "provenance": {
            "source": "ref/x3_menu/runtime_blocks (verbatim EmulatorX.dll bytes)",
            "codec": "minifloat u16, same codec as body240",
            "law": "xStream: four distinct per-rate designs, nearest-bank "
                   "selection, no recompilation",
            "verified": "codec null vs df2 reference decode < 0.01 dB per bank; "
                        "33x33 stability certify per bank",
        },
    }
    return cart, report

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=str(OUT_DEFAULT),
                    help="output directory for .x3preset.json files")
    ap.add_argument("--dry-run", action="store_true",
                    help="verify only, write nothing")
    ap.add_argument("--bake", action="store_true",
                    help="also copy the roster's RUNTIME cartridges into "
                         "plugin/presets/bodies/ as X3F_<stem>.json for "
                         "BinaryData (see plugin/CMakeLists.txt for why the "
                         "name is shortened)")
    args = ap.parse_args()

    out_dir = Path(args.out)
    if not args.dry_run:
        out_dir.mkdir(parents=True, exist_ok=True)

    if not BLOCKS.is_dir():
        print(f"runtime blocks not found: {BLOCKS}")
        sys.exit(1)

    print(f"blocks: {BLOCKS}")
    print(f"{'filter':22s} {'44.1k':>12s} {'48k':>12s} {'96k':>12s} {'192k':>12s}  banks")
    print("-" * 82)

    written = 0
    total_banks = 0
    incomplete = []

    for stem, active_stages in FUND:
        cart, report = build(stem, active_stages)
        cells = []
        for rate, worst, ok, max_r, status in report:
            if status == "ok":
                cells.append(f"{worst:.4f}dB")
            elif status == "MISSING":
                cells.append("--")
            else:
                cells.append(status[:12])
        n = len(cart["banks"]) if cart else 0
        total_banks += n
        flag = "" if n == 4 else "  <- incomplete"
        if n != 4:
            incomplete.append((stem, n))
        print(f"{stem:22s} " + " ".join(f"{c:>12s}" for c in cells) +
              f"  {n}/4{flag}")

        if cart and not args.dry_run:
            (out_dir / f"X3F_{stem}.x3preset.json").write_text(
                json.dumps(cart, indent=1))
            written += 1

    print("-" * 82)
    print(f"{total_banks}/{len(FUND) * 4} banks verified")
    if incomplete:
        for stem, n in incomplete:
            print(f"  incomplete: {stem} ({n}/4)")
    if args.dry_run:
        print("dry run — nothing written")
        return

    print(f"{written} cartridges -> {out_dir}")

    if args.bake:
        import re
        import shutil
        roster = ROOT / "plugin" / "presets" / "PresetRoster.inc"
        dst = ROOT / "plugin" / "presets" / "bodies"
        pat = re.compile(r'TRENCH_PRESET\(".+?",\s*"(.+?)",\s*"RUNTIME"\)')
        stems = [m.group(1) for m in
                 (pat.match(l) for l in roster.read_text().splitlines())
                 if m]
        baked = 0
        for stem in stems:
            src = out_dir / f"{stem}.x3preset.json"
            if src.exists():
                shutil.copyfile(src, dst / f"{stem}.json")
                baked += 1
            else:
                print(f"  roster wants {stem} but {src.name} was not built")
        print(f"{baked}/{len(stems)} RUNTIME cartridges baked -> {dst}")

if __name__ == "__main__":
    main()
