from __future__ import annotations

import ctypes
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))

from arma_measure_lib import lib, RT_DOUBLES  # noqa: E402  (loads trench_core.dll)

# probe() slices six stages on purpose: RT_DOUBLES is 35 now (seven sections),
# and every caller of this module reads a legacy 240-byte body.
LEGACY_STAGES = 6

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_char_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int

lib.trench_stage_roots_from_words_at.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_double,
    ctypes.POINTER(ctypes.c_double)]
lib.trench_stage_roots_from_words_at.restype = ctypes.c_int

lib.trench_stage_words_from_roots_at.argtypes = [
    ctypes.POINTER(ctypes.c_double), ctypes.c_double,
    ctypes.POINTER(ctypes.c_uint16)]
lib.trench_stage_words_from_roots_at.restype = ctypes.c_int

lib.trench_engine_create.argtypes = []
lib.trench_engine_create.restype = ctypes.c_void_p
lib.trench_engine_destroy.argtypes = [ctypes.c_void_p]
lib.trench_engine_prepare.argtypes = [ctypes.c_void_p, ctypes.c_double]
lib.trench_engine_load_body_bytes_at.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_double]
lib.trench_engine_load_body_bytes_at.restype = ctypes.c_int
lib.trench_engine_process_block.argtypes = [
    ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float), ctypes.c_int,
    ctypes.c_double, ctypes.c_double]

def probe(body: bytes, morph: float, q: float, rate: float):
    coeffs = (ctypes.c_double * RT_DOUBLES)()
    max_r = ctypes.c_double()
    unstable = ctypes.c_uint32()
    nonfinite = ctypes.c_uint32()
    rc = lib.trench_packed_probe_at(body, len(body), morph, q, rate, coeffs,
                                    ctypes.byref(max_r),
                                    ctypes.byref(unstable),
                                    ctypes.byref(nonfinite))
    if rc != 0 or unstable.value or nonfinite.value:
        return None
    return [list(coeffs[s * 5:s * 5 + 5]) for s in range(6)]

def roots_from_coeffs(c, zero: bool, rate: float):
    if zero:
        if abs(c[0]) < 1e-12:
            return 0.0, 0.0
        p1, p0 = c[1] / c[0], c[2] / c[0]
    else:
        p1, p0 = c[3], c[4]
    disc = p1 * p1 - 4.0 * p0
    if disc >= 0.0 or p0 <= 0.0:
        return 0.0, 0.0
    r = math.sqrt(p0)
    hz = math.acos(max(-1.0, min(1.0, -p1 / (2.0 * r)))) \
        / (2.0 * math.pi) * rate
    return hz, r

def roots_from_words(words5, rate: float):
    w = (ctypes.c_uint16 * 5)(*words5)
    out = (ctypes.c_double * 5)()
    if lib.trench_stage_roots_from_words_at(w, rate, out) != 0:
        return None
    return list(out)

def words_from_roots(pole_hz, pole_r, zero_hz, zero_r, scale, rate: float):
    r = (ctypes.c_double * 5)(pole_hz, pole_r, zero_hz, zero_r, scale)
    out = (ctypes.c_uint16 * 5)()
    if lib.trench_stage_words_from_roots_at(r, rate, out) != 0:
        return None
    return list(out)
