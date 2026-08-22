from __future__ import annotations

import ctypes
import hashlib
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib  # noqa: E402

GRID = np.geomspace(20.0, 20_000.0, 700)

try:
    lib.trench_num_stages.restype = ctypes.c_uint32
    lib.trench_num_coeffs.restype = ctypes.c_uint32
    RT_STAGES = int(lib.trench_num_stages())
    RT_COEFFS = int(lib.trench_num_coeffs())
except AttributeError:
    RT_STAGES, RT_COEFFS = 6, 5
BODY_STAGES, NUM_COEFFS = 6, RT_COEFFS
NUM_STAGES = BODY_STAGES

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int
lib.trench_stage_roots_from_words_at.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_double,
    ctypes.POINTER(ctypes.c_double)]
lib.trench_stage_roots_from_words_at.restype = ctypes.c_int
lib.trench_packed_interpolate.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.POINTER(ctypes.c_double)]
lib.trench_packed_interpolate.restype = ctypes.c_int
lib.trench_packed_decode.argtypes = [ctypes.c_uint16]
lib.trench_packed_decode.restype = ctypes.c_double

NATIVE_BYTES, LEGACY_BYTES = 560, 240
NATIVE_FRAMES, NATIVE_STAGES = 8, 7

def load_body(path: Path) -> bytes:
    body = Path(path).read_bytes()
    assert len(body) in (LEGACY_BYTES, NATIVE_BYTES), \
        f"{path}: {len(body)} bytes, want {LEGACY_BYTES} or {NATIVE_BYTES}"
    return body

def is_cube(body: bytes) -> bool:
    return len(body) == NATIVE_BYTES

def stages_of(body: bytes) -> int:
    return NATIVE_STAGES if is_cube(body) else BODY_STAGES

def frames_of(body: bytes) -> int:
    return NATIVE_FRAMES if is_cube(body) else 4

def body_sha256(body: bytes) -> str:
    return hashlib.sha256(body).hexdigest()

def corner_words(body: bytes) -> np.ndarray:
    return np.frombuffer(body, dtype="<u2").reshape(
        frames_of(body), stages_of(body), NUM_COEFFS).copy()

def lerp_u16(a: int, b: int, frac: float) -> int:
    diff = np.float32(np.int32(b) - np.int32(a))
    d = int(diff * np.float32(frac))
    delta_i16 = ((d + 0x8000) & 0xFFFF) - 0x8000
    return (delta_i16 + int(a)) & 0xFFFF

def interpolate_words(body: bytes, morph: float, q: float, z: float = 0.0) -> np.ndarray:
    w = corner_words(body)
    ns, nf = stages_of(body), frames_of(body)
    m32, q32, z32 = np.float32(morph), np.float32(q), np.float32(z)
    out = np.zeros((ns, NUM_COEFFS), dtype=np.uint16)
    for si in range(ns):
        for wi in range(NUM_COEFFS):
            plane = []
            for base in range(0, nf, 4):
                e0 = lerp_u16(int(w[base + 0, si, wi]), int(w[base + 1, si, wi]), m32)
                e1 = lerp_u16(int(w[base + 2, si, wi]), int(w[base + 3, si, wi]), m32)
                plane.append(lerp_u16(e0, e1, q32))
            out[si, wi] = plane[0] if len(plane) == 1 else lerp_u16(plane[0], plane[1], z32)
    return out

COMBINE_K = 4.0

def verify_word_law(body: bytes, states=((0.3, 0.7), (0.68, 0.0), (1.0, 1.0))) -> None:
    buf = ctypes.create_string_buffer(body, 240)
    for m, q in [(0, 0), (1, 0), (0, 1), (1, 1), *states]:
        kernel = (ctypes.c_double * (RT_STAGES * RT_COEFFS))()
        assert lib.trench_packed_interpolate(buf, 240, float(m), float(q), kernel) == 0
        ours = interpolate_words(body, m, q)
        d = np.array([lib.trench_packed_decode(int(wd))
                      for wd in ours.ravel()]).reshape(NUM_STAGES, NUM_COEFFS)
        rebuilt = np.stack([
            COMBINE_K * d[:, 0] + d[:, 1],
            d[:, 1],
            COMBINE_K * d[:, 2] + d[:, 3],
            d[:, 3],
            COMBINE_K * d[:, 4],
        ], axis=1)
        dll = np.ctypeslib.as_array(kernel).reshape(RT_STAGES, RT_COEFFS)[:BODY_STAGES]
        if not np.array_equal(rebuilt, dll):
            raise AssertionError(f"word-law drift at M{m} Q{q}")

def _biquads_from_words(words: np.ndarray) -> np.ndarray:
    """The word law, in Python. `trench_packed_probe_at` is 240-byte only and
    takes no third axis, so a cube is decoded here instead. Cross-checked
    against the DLL on every legacy body by `verify_word_law`."""
    d = np.array([[lib.trench_packed_decode(int(x)) for x in row] for row in words])
    c0 = COMBINE_K * d[:, 0] + d[:, 1]
    c1 = d[:, 1]
    c2 = COMBINE_K * d[:, 2] + d[:, 3]
    c3 = d[:, 3]
    c4 = COMBINE_K * d[:, 4]
    return np.stack([c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3], axis=1)

def biquads_at(body: bytes, morph: float, q: float, rate: float,
               z: float = 0.0) -> np.ndarray | None:
    if is_cube(body):
        return _biquads_from_words(interpolate_words(body, morph, q, z))
    c = (ctypes.c_double * (RT_STAGES * RT_COEFFS))()
    mr = ctypes.c_double(); un = ctypes.c_uint32(); nf = ctypes.c_uint32()
    buf = ctypes.create_string_buffer(body, 240)
    if lib.trench_packed_probe_at(buf, 240, morph, q, rate, c, ctypes.byref(mr),
                                  ctypes.byref(un), ctypes.byref(nf)) != 0 \
       or un.value or nf.value:
        return None
    return np.ctypeslib.as_array(c).reshape(RT_STAGES, RT_COEFFS)[:BODY_STAGES].copy()

def roots_from_words(words_row, rate: float):
    w = (ctypes.c_uint16 * NUM_COEFFS)(*[int(x) for x in words_row])
    out = (ctypes.c_double * 5)()
    if lib.trench_stage_roots_from_words_at(w, rate, out) != 0:
        return None
    return tuple(out)

def _root_kind(hz: float, rate: float) -> str:
    return "real" if hz <= 1.0 or hz >= rate * 0.5 - 1.0 else "conjugate"

def stage_complex(biquad: np.ndarray, rate: float) -> np.ndarray:
    z1 = np.exp(-1j * 2.0 * np.pi * GRID / rate)
    b0, b1, b2, a1, a2 = biquad
    return (b0 + b1 * z1 + b2 * z1 * z1) / (1.0 + a1 * z1 + a2 * z1 * z1)

def to_db(h: np.ndarray) -> np.ndarray:
    return 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))

def probe_state(body: bytes, morph: float, q: float, rate: float) -> dict | None:
    bq = biquads_at(body, morph, q, rate)
    if bq is None:
        return None
    words = interpolate_words(body, morph, q)
    H = np.stack([stage_complex(bq[s], rate) for s in range(NUM_STAGES)])
    cum_after = np.cumprod(H, axis=0)
    cum_before = np.concatenate([np.ones((1, len(GRID)), dtype=complex),
                                 cum_after[:-1]])
    sections = []
    for s in range(NUM_STAGES):
        r = roots_from_words(words[s], rate)
        geometry = None
        if r is not None:
            pole_hz, pole_r, zero_hz, zero_r, scale = r
            geometry = {
                "pole": {"kind": _root_kind(pole_hz, rate),
                         "hz": pole_hz, "radius": pole_r},
                "zero": {"kind": _root_kind(zero_hz, rate),
                         "hz": zero_hz, "radius": zero_r},
                "scale": scale,
            }
        sections.append({
            "lane": s + 1,
            "packed_words": [int(x) for x in words[s]],
            "biquad": [float(x) for x in bq[s]],
            "geometry": geometry,
            "stage_complex": H[s],
            "before_complex": cum_before[s],
            "after_complex": cum_after[s],
        })
    return {"morph": float(morph), "q": float(q), "sections": sections}

def stage_curves(body: bytes, m: float, q: float, rate: float, z: float = 0.0):
    ns = stages_of(body)
    bq = biquads_at(body, m, q, rate, z)
    if bq is None:
        return [np.full_like(GRID, -60.0)] * ns
    return [to_db(stage_complex(bq[s], rate)) for s in range(ns)]

def response(body: bytes, m: float, q: float, rate: float, z: float = 0.0) -> np.ndarray:
    return np.sum(stage_curves(body, m, q, rate, z), axis=0)

def corner_geometry(body: bytes, corner: int, rate: float):
    w = corner_words(body)
    return [roots_from_words(w[corner, s], rate) for s in range(stages_of(body))]
