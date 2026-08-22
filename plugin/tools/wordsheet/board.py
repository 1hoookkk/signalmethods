from __future__ import annotations

import math

from .core import (BW_MAX_HZ, BW_MIN_HZ, LANES, Row, radius_from_bw, require_spec)


BAND = 0.78
HIT_PX = 22.0
_LO = math.log(BW_MIN_HZ)
_SPAN = math.log(BW_MAX_HZ) - _LO


def lane_y(lane: int, bw_hz: float, lanes: int = LANES) -> float:
    b = min(max(bw_hz, BW_MIN_HZ), BW_MAX_HZ)
    frac = 1.0 - (math.log(b) - _LO) / _SPAN
    return (lanes - 1 - lane) + frac * BAND


def y_to_bw(lane: int, y: float, lanes: int = LANES) -> float:
    frac = 1.0 - (y - (lanes - 1 - lane)) / BAND
    b = math.exp(_LO + min(max(frac, 0.0), 1.0) * _SPAN)
    return min(max(b, BW_MIN_HZ), BW_MAX_HZ)


def set_pair(row: Row, kind: int, hz: float, bw_hz: float, rate: float) -> str:
    bad = require_spec(hz, bw_hz, rate)
    if bad:
        return bad
    r = min(max(radius_from_bw(bw_hz, rate), 0.0), 0.99999)
    b2 = r * r
    ek2 = 1.0 - b2
    w = 2.0 * math.pi * min(max(hz, 1.0), rate * 0.49) / rate
    b1 = -2.0 * r * math.cos(w)
    ek1 = (1.0 + b1 + b2) / 4.0
    if ek1 <= 0.0 or ek2 <= 0.0:
        return "that frequency and bandwidth do not make a stable root"
    base = 0 if kind == 0 else 2
    row.set_k(base, -math.log(ek1))
    row.set_k(base + 1, -math.log(ek2))
    return ""
