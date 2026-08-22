from __future__ import annotations

import cmath
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from pyruntime import ffi  # noqa: E402

RATE_HZ = 44_100.0
CORNERS = ("M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100")
CORNER_MQ = {"M0_Q0": (0.0, 0.0), "M100_Q0": (1.0, 0.0),
             "M0_Q100": (0.0, 1.0), "M100_Q100": (1.0, 1.0)}

def log_grid(points: int = 256, low_hz: float = 20.0, high_hz: float = 20_000.0) -> list[float]:
    span = math.log10(high_hz / low_hz)
    return [low_hz * 10 ** (i / (points - 1) * span) for i in range(points)]

def coeffs_to_biquad(coeffs) -> tuple[float, float, float, float, float]:
    c0, c1, c2, c3, c4 = coeffs
    return (c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3)

def biquad_db(biquad, freqs: list[float], rate_hz: float = RATE_HZ) -> list[float]:
    b0, b1, b2, a1, a2 = biquad
    out = []
    for f in freqs:
        z = cmath.exp(-2j * math.pi * f / rate_hz)
        num = b0 + b1 * z + b2 * z * z
        den = 1.0 + a1 * z + a2 * z * z
        out.append(20 * math.log10(max(abs(num / den), 1e-12)))
    return out

def zero_attenuation_db(zero_hz: float, zero_r: float, pole_hz: float,
                        rate_hz: float = RATE_HZ) -> float:
    wz = 2.0 * math.pi * zero_hz / rate_hz
    wp = 2.0 * math.pi * pole_hz / rate_hz
    z = cmath.exp(-1j * wp)
    mag = abs((1.0 - zero_r * cmath.exp(1j * wz) * z)
              * (1.0 - zero_r * cmath.exp(-1j * wz) * z))
    return 20.0 * math.log10(max(mag, 1e-12))

def zero_radius_for_budget(pole_hz: float, budget_db: float,
                           rate_hz: float = RATE_HZ) -> float:
    lo, hi = 0.0, 0.99999
    for _ in range(80):
        mid = 0.5 * (lo + hi)
        if zero_attenuation_db(pole_hz, mid, pole_hz, rate_hz) > budget_db:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)

def stage_curves(body: bytes, morph: float, q: float, freqs: list[float],
                 rate_hz: float = RATE_HZ):
    rows = ffi.probe(body, morph, q, rate_hz)
    if rows is None:
        return None
    isolated = [biquad_db(row, freqs, rate_hz) for row in rows]
    cumulative, running = [], [0.0] * len(freqs)
    for curve in isolated:
        running = [a + b for a, b in zip(running, curve)]
        cumulative.append(list(running))
    return isolated, cumulative

def corner_db(body: bytes, corner: str, freqs: list[float],
              rate_hz: float = RATE_HZ) -> list[float] | None:
    morph, q = CORNER_MQ[corner]
    curves = stage_curves(body, morph, q, freqs, rate_hz)
    return None if curves is None else curves[1][5]

def interior_sweep(body: bytes, steps: int = 21, freqs: list[float] | None = None,
                   rate_hz: float = RATE_HZ) -> dict:
    freqs = freqs or log_grid()
    out = {
        "grid": f"{steps}x{steps}",
        "rate_hz": rate_hz,
        "refused": [],
        "worst_output_db": -math.inf,
        "worst_output_at": None,
        "worst_intermediate_db": -math.inf,
        "worst_intermediate_at": None,
        "worst_overshoot_db": -math.inf,
        "worst_overshoot_at": None,
    }
    for mi in range(steps):
        for qi in range(steps):
            morph, q = mi / (steps - 1), qi / (steps - 1)
            curves = stage_curves(body, morph, q, freqs, rate_hz)
            if curves is None:
                out["refused"].append([morph, q])
                continue
            _isolated, cumulative = curves
            output = max(cumulative[5])
            intermediate = max(max(c) for c in cumulative[:5])
            if output > out["worst_output_db"]:
                out["worst_output_db"], out["worst_output_at"] = output, [morph, q]
            if intermediate > out["worst_intermediate_db"]:
                out["worst_intermediate_db"] = intermediate
                out["worst_intermediate_at"] = [morph, q]
            if intermediate - output > out["worst_overshoot_db"]:
                out["worst_overshoot_db"] = intermediate - output
                out["worst_overshoot_at"] = [morph, q, intermediate, output]
    return out

def corner_summary(body: bytes, freqs: list[float] | None = None,
                   rate_hz: float = RATE_HZ) -> dict:
    freqs = freqs or log_grid()
    summary = {}
    for corner in CORNERS:
        morph, q = CORNER_MQ[corner]
        curves = stage_curves(body, morph, q, freqs, rate_hz)
        if curves is None:
            summary[corner] = None
            continue
        isolated, cumulative = curves
        summary[corner] = {
            "isolated_peak_db": [max(c) for c in isolated],
            "cumulative_peak_db": [max(c) for c in cumulative],
            "output_peak_db": max(cumulative[5]),
        }
    return summary

def format_report(body: bytes, name: str, steps: int = 21) -> str:
    freqs = log_grid()
    summary = corner_summary(body, freqs)
    sweep = interior_sweep(body, steps, freqs)
    lines = [f"{name}", "-" * len(name),
             f"{'corner':11s} " + " ".join(f" S{k + 1}iso" for k in range(6))
             + "  " + " ".join(f"S1..{k + 1}" for k in range(6))]
    for corner in CORNERS:
        entry = summary[corner]
        if entry is None:
            lines.append(f"{corner:11s} packed runtime REFUSED this corner")
            continue
        iso = " ".join(f"{v:6.1f}" for v in entry["isolated_peak_db"])
        cum = " ".join(f"{v:6.1f}" for v in entry["cumulative_peak_db"])
        lines.append(f"{corner:11s} {iso}  {cum}")
    over = sweep["worst_overshoot_at"]
    lines += [
        f"interior {sweep['grid']}: output peak {sweep['worst_output_db']:.2f} dB "
        f"at morph {sweep['worst_output_at'][0]:.2f} Q {sweep['worst_output_at'][1]:.2f}",
        f"interior {sweep['grid']}: worst intermediate {sweep['worst_intermediate_db']:.2f} dB "
        f"at morph {sweep['worst_intermediate_at'][0]:.2f} Q {sweep['worst_intermediate_at'][1]:.2f}",
        f"interior {sweep['grid']}: worst intermediate overshoot above output "
        f"{sweep['worst_overshoot_db']:.2f} dB at morph {over[0]:.2f} Q {over[1]:.2f} "
        f"(intermediate {over[2]:.1f} dB, output {over[3]:.1f} dB)",
    ]
    if sweep["refused"]:
        lines.append(f"packed runtime REFUSED {len(sweep['refused'])} interior points")
    return "\n".join(lines)

def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("usage: python -m tools.cascade_headroom BODY.body240 [more...]")
    for arg in sys.argv[1:]:
        path = Path(arg)
        body = path.read_bytes()
        if len(body) != 240:
            raise SystemExit(f"{path}: {len(body)} bytes, not 240")
        print(format_report(body, path.name))
        print()

if __name__ == "__main__":
    main()
