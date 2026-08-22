"""TRENCH taste linter — validate a body240 against the filter axioms.

Runs before any candidate body is allowed to ship. Every violation is a
build-stopper, not a warning. Axioms are from docs/FILTER_AXIOMS.md.

Usage:
  python tools/taste_linter.py path/to/body.body240
  python tools/taste_linter.py --all presets_ship_v1/bodies/
"""
from __future__ import annotations
import argparse
import ctypes as C
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DLL = ROOT / "target" / "release" / "trench_core.dll"

NUM_STAGES = 6
NUM_COEFFS = 5
PAD = (0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000)

ANCHOR_MAX_HZ = 400.0
MOUTH_MAX_HZ = 3500.0
RAZOR_MAX_ST = 2.0
VALLEY_MIN_ST = 3.0
VALLEY_MAX_ST = 10.0
CERTIFY_GRID = 33
DEFAULT_SR = 44100.0

def _load_lib():
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

def probe(body: bytes, morph: float, q: float, lib) -> np.ndarray:
    out = (C.c_double * RT_DOUBLES)()
    mr = C.c_double()
    um = C.c_uint32()
    nm = C.c_uint32()
    rc = lib.trench_packed_probe(
        body, len(body), C.c_double(morph), C.c_double(q),
        out, C.byref(mr), C.byref(um), C.byref(nm),
    )
    if rc != 0:
        raise RuntimeError(f"probe failed at m={morph} q={q}: rc={rc}")
    return np.array(out).reshape(NUM_STAGES, NUM_COEFFS)

def biquad_poles(b0, b1, b2, a1, a2, sr):
    disc = a1 * a1 - 4.0 * a2
    poles = []
    if disc >= 0:
        sq = np.sqrt(max(disc, 0))
        for root in [(-a1 + sq) / 2.0, (-a1 - sq) / 2.0]:
            if abs(root) > 1e-12:
                hz = abs(np.angle(root)) * sr / (2 * np.pi)
                r = abs(root)
                poles.append((float(hz), float(r)))
    else:
        sq = np.sqrt(-disc)
        r = np.sqrt(max(a2, 0))
        if r > 1e-12:
            hz = np.arccos(max(-1.0, min(1.0, -a1 / (2.0 * r)))) * sr / (2 * np.pi)
            poles.append((float(hz), float(r)))
    return poles

def biquad_zeros(b0, b1, b2, a1, a2, sr):
    if abs(b0) < 1e-12:
        return []
    disc = b1 * b1 - 4.0 * b0 * b2
    zeros = []
    if disc >= 0:
        sq = np.sqrt(max(disc, 0))
        for root in [(-b1 + sq) / (2.0 * b0), (-b1 - sq) / (2.0 * b0)]:
            if abs(root) > 1e-12:
                hz = abs(np.angle(root)) * sr / (2 * np.pi)
                r = abs(root)
                zeros.append((float(hz), float(r)))
    else:
        sq = np.sqrt(-disc)
        r = np.sqrt(max(b2 / b0, 0))
        if r > 1e-12:
            hz = np.arccos(max(-1.0, min(1.0, -b1 / (2.0 * b0 * r)))) * sr / (2 * np.pi)
            zeros.append((float(hz), float(r)))
    return zeros

def hz_to_st(hz1, hz2):
    if hz1 < 1 or hz2 < 1:
        return 999
    return abs(12.0 * np.log2(hz1 / hz2))

def active_stage_mask(words):
    return [tuple(words[s * 5:(s + 1) * 5]) != PAD for s in range(NUM_STAGES)]

class Violation:
    def __init__(self, axiom, stage, detail):
        self.axiom = axiom
        self.stage = stage
        self.detail = detail

    def __str__(self):
        return f"[AXIOM {self.axiom}] S{self.stage}: {self.detail}"

def check_axiom_1_register(poles_all_corners, active):
    violations = []
    for ci, (corner_label, stages) in enumerate(poles_all_corners):
        anchors = []
        mouths = []
        airs = []
        for si in range(NUM_STAGES):
            if not active[si]:
                continue
            for hz, r in stages[si]:
                if r < 0.85:
                    continue
                if hz < ANCHOR_MAX_HZ:
                    anchors.append((si, hz))
                elif hz < MOUTH_MAX_HZ:
                    mouths.append((si, hz))
                else:
                    airs.append((si, hz))
        all_mouths = mouths + [m for c in poles_all_corners for m in []]
    total = {"anchor": 0, "mouth": 0, "air": 0}
    for _, stages in poles_all_corners:
        for si in range(NUM_STAGES):
            if not active[si]:
                continue
            for hz, r in stages[si]:
                if r < 0.85:
                    continue
                if hz < ANCHOR_MAX_HZ:
                    total["anchor"] += 1
                elif hz < MOUTH_MAX_HZ:
                    total["mouth"] += 1
                else:
                    total["air"] += 1

    if total["mouth"] == 0 and total["air"] == 0:
        violations.append(Violation(1, 0,
            f"no mouth or air voices across any corner (anchor={total['anchor']})"))
    return violations

def check_axiom_2_voicing(poles_all_corners, active):
    violations = []
    for ci, (label, stages) in enumerate(poles_all_corners):
        all_hz = []
        for si in range(NUM_STAGES):
            if not active[si]:
                continue
            for hz, r in stages[si]:
                if r >= 0.85:
                    all_hz.append(hz)
        all_hz.sort()
        et_grid = [440.0 * (2 ** (n / 12.0)) for n in range(-36, 48)]
        for hz in all_hz:
            if hz < 20:
                continue
            for et in et_grid:
                if abs(12.0 * np.log2(hz / et)) < 0.05:
                    break
            else:
                continue
        et_count = sum(
            1 for hz in all_hz
            if any(abs(12.0 * np.log2(hz / et)) < 0.05 for et in et_grid)
        )
        if len(all_hz) >= 3 and et_count >= len(all_hz) * 0.5:
            violations.append(Violation(2, 0,
                f"{label}: {et_count}/{len(all_hz)} voices on A440 grid — "
                f"SHALL be intervallic, not tonal"))
    return violations

def check_axiom_3_motion(poles_all_corners, active):
    m0_poles = {}
    m100_poles = {}
    for si in range(NUM_STAGES):
        if not active[si]:
            continue
        m0_poles[si] = [hz for hz, r in poles_all_corners[0][1][si] if r >= 0.85]
        m100_poles[si] = [hz for hz, r in poles_all_corners[1][1][si] if r >= 0.85]

    directions = []
    for si in sorted(m0_poles.keys()):
        if not m0_poles[si] or not m100_poles[si]:
            continue
        m0_hz = m0_poles[si][0]
        m100_hz = m100_poles[si][0]
        if m0_hz < 1 or m100_hz < 1:
            continue
        st_change = 12.0 * np.log2(m100_hz / m0_hz)
        directions.append(st_change)

    if len(directions) < 2:
        return []

    up = sum(1 for d in directions if d > 4)
    down = sum(1 for d in directions if d < -4)
    flat = sum(1 for d in directions if abs(d) <= 4)

    if up > 0 and down > 0:
        motion = "contrary"
    elif flat >= len(directions) * 0.6:
        motion = "oblique"
    elif up == 0 or down == 0:
        motion = "parallel"
    else:
        return [Violation(3, 0,
            f"unclassifiable motion: {up}↑ {down}↓ {flat}→ "
            f"({[f'{d:+.0f}' for d in directions[:6]]})")]
    return []

def check_axiom_4_zeros(probed_corners, active):
    violations = []
    for ci, (label, rows) in enumerate(probed_corners):
        for si in range(NUM_STAGES):
            if not active[si]:
                continue
            b0, b1, b2, a1, a2 = [float(x) for x in rows[si]]
            zeros_list = biquad_zeros(b0, b1, b2, a1, a2, DEFAULT_SR)
            poles_list = biquad_poles(b0, b1, b2, a1, a2, DEFAULT_SR)
            if not zeros_list or not poles_list:
                continue
            for z_hz, z_r in zeros_list:
                if z_r < 0.01:
                    continue
                nearest_st = min(hz_to_st(z_hz, p_hz) for p_hz, _ in poles_list)
                if nearest_st <= RAZOR_MAX_ST:
                    continue
                if VALLEY_MIN_ST <= nearest_st <= VALLEY_MAX_ST:
                    continue
    return violations

def check_axiom_6_sections(probed_corners, active):
    UNIT_R = 0.9999
    violations = []
    for label, rows in probed_corners:
        sections = []
        for si in range(NUM_STAGES):
            b0, b1, b2, a1, a2 = [float(x) for x in rows[si]]
            sections.append((biquad_poles(b0, b1, b2, a1, a2, DEFAULT_SR),
                             biquad_zeros(b0, b1, b2, a1, a2, DEFAULT_SR)))
        if active[5]:
            zr = max((r for _hz, r in sections[5][1]), default=0.0)
            if zr < UNIT_R:
                violations.append(Violation(
                    6, 0, f"{label}: S6 unit-zero terminator missing "
                          f"(max zero r {zr:.4f} < {UNIT_R})"))
        for si in range(NUM_STAGES):
            if not active[si]:
                continue
            poles_i = [(hz, r) for hz, r in sections[si][0] if hz > 20.0]
            if not poles_i:
                violations.append(Violation(
                    6, si + 1, f"{label}: S{si + 1} active but hollow "
                               "(no voiced pole)"))
    return violations

def check_axiom_5_q_attitude(q0_rows, q100_rows, active):
    radius_changes = []
    for si in range(NUM_STAGES):
        if not active[si]:
            continue
        q0 = q0_rows[si]
        q100 = q100_rows[si]
        q0_poles = biquad_poles(*q0, DEFAULT_SR)
        q100_poles = biquad_poles(*q100, DEFAULT_SR)
        if not q0_poles or not q100_poles:
            continue
        q0_r = q0_poles[0][1]
        q100_r = q100_poles[0][1]
        if q0_r > 0:
            radius_changes.append(float(q100_r - q0_r))

    if len(radius_changes) < 3:
        return []

    positive = [d for d in radius_changes if d > 0.002]
    negative = [d for d in radius_changes if d < -0.002]

    if len(positive) >= len(radius_changes) * 0.8 and len(negative) == 0:
        med = np.median(positive)
        if med > 0.01:
            return [Violation(5, 0,
                f"uniform Q push across {len(positive)}/{len(radius_changes)} stages "
                f"(median Δr={med:.4f}) — SHALL be asymmetric, single-bloomer, "
                f"or spare-the-air")]

    if len(negative) >= len(radius_changes) * 0.8 and len(positive) == 0:
        med = abs(np.median(negative))
        if med > 0.02:
            return [Violation(5, 0,
                f"uniform negative Q across all stages (median Δr={med:.4f})")]

    return []

def check_axiom_10_stability(body, lib):
    grid = CERTIFY_GRID
    for qi in range(grid):
        q = qi / (grid - 1)
        for mi in range(grid):
            m = mi / (grid - 1)
            try:
                rows = probe(body, m, q, lib)
            except RuntimeError:
                return [Violation(10, 0,
                    f"probe failed at m={m:.2f} q={q:.2f}")]
            for si, row in enumerate(rows):
                if any(not np.isfinite(v) for v in row):
                    return [Violation(10, si + 1,
                        f"non-finite biquad at m={m:.2f} q={q:.2f}")]
                poles = biquad_poles(*row, DEFAULT_SR)
                for hz, r in poles:
                    if r >= 1.0:
                        return [Violation(10, si + 1,
                            f"unstable pole r={r:.6f} at {hz:.0f} Hz "
                            f"(m={m:.2f} q={q:.2f})")]
    return []

def lint_body(path: Path, lib) -> list[Violation]:
    body = path.read_bytes()
    if len(body) != 240:
        return [Violation(0, 0, f"not 240 bytes ({len(body)} bytes)")]

    words = list(struct.unpack("<120H", body))
    active = active_stage_mask(words)

    corners = [
        ("M0Q0", 0.0, 0.0),
        ("M100Q0", 1.0, 0.0),
        ("M0Q100", 0.0, 1.0),
        ("M100Q100", 1.0, 1.0),
    ]
    probed = {}
    for label, m, q in corners:
        probed[label] = probe(body, m, q, lib)

    poles_all = []
    for label, m, q in corners:
        rows = probed[label]
        stage_poles = []
        for si in range(NUM_STAGES):
            stage_poles.append(biquad_poles(*rows[si], DEFAULT_SR))
        poles_all.append((label, stage_poles))

    violations = []
    violations += check_axiom_1_register(poles_all, active)
    violations += check_axiom_2_voicing(poles_all, active)
    violations += check_axiom_3_motion(poles_all, active)
    violations += check_axiom_4_zeros([(label, probed[label]) for label, _, _ in corners], active)
    violations += check_axiom_5_q_attitude(probed["M0Q0"], probed["M0Q100"], active)
    violations += check_axiom_6_sections([(label, probed[label]) for label, _, _ in corners], active)
    violations += check_axiom_10_stability(body, lib)
    return violations

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("path", nargs="+", help=".body240 file(s) or directory")
    ap.add_argument("--all", action="store_true", help="lint all bodies in a directory")
    args = ap.parse_args()

    lib = _load_lib()

    paths = []
    for p in args.path:
        pp = Path(p)
        if pp.is_dir() or args.all:
            paths.extend(sorted(pp.glob("*.body240")))
        else:
            paths.append(pp)

    if not paths:
        print("No .body240 files found.")
        sys.exit(1)

    total_violations = 0
    clean = 0
    for path in paths:
        violations = lint_body(path, lib)
        if violations:
            print(f"\n{path.name} — {len(violations)} VIOLATION(S):")
            for v in violations:
                print(f"  {v}")
            total_violations += len(violations)
        else:
            clean += 1
            print(f"{path.name} — CLEAN")

    print(f"\n{clean} clean, {total_violations} violations in {len(paths)} bodies")
    if total_violations > 0:
        print("BUILD BLOCKED: fix violations before shipping.")
        sys.exit(1)

if __name__ == "__main__":
    main()
