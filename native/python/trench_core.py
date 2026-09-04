import ctypes
import os
import sys
from typing import List, Optional, Tuple, Union
import numpy as np

def _find_dll() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(here, "trench_core_c.dll"),
        os.path.join(here, "..", "..", "out", "build", "app", "native", "core", "trench_core_c.dll"),
        os.path.join(here, "..", "..", "out", "build", "app", "bin", "trench_core_c.dll"),
        os.path.join(here, "..", "core", "trench_core_c.dll"),
    ]
    for c in candidates:
        norm = os.path.normpath(c)
        if os.path.isfile(norm):
            return norm
    raise FileNotFoundError("Could not find trench_core_c.dll in any candidate path")

_dll_path = _find_dll()
_dll = ctypes.CDLL(_dll_path)

class TrenchSectionGeometry(ctypes.Structure):
    _fields_ = [
        ("pole_type", ctypes.c_int),
        ("pole_a", ctypes.c_double),
        ("pole_b", ctypes.c_double),
        ("zero_type", ctypes.c_int),
        ("zero_a", ctypes.c_double),
        ("zero_b", ctypes.c_double),
        ("scale", ctypes.c_double),
    ]

class TrenchResonance(ctypes.Structure):
    _fields_ = [
        ("hz", ctypes.c_double),
        ("bw_hz", ctypes.c_double),
        ("gain_db", ctypes.c_double),
    ]

_dll.trench_decode_word.argtypes = [ctypes.c_uint16]
_dll.trench_decode_word.restype = ctypes.c_double

_dll.trench_decode_fractional.argtypes = [ctypes.c_double]
_dll.trench_decode_fractional.restype = ctypes.c_double

_dll.trench_encode_word.argtypes = [ctypes.c_double]
_dll.trench_encode_word.restype = ctypes.c_uint16

_dll.trench_interpolate_word.argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_float]
_dll.trench_interpolate_word.restype = ctypes.c_uint16

_dll.trench_body_create_legacy.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
_dll.trench_body_create_legacy.restype = ctypes.c_void_p

_dll.trench_body_create_native.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
_dll.trench_body_create_native.restype = ctypes.c_void_p

_dll.trench_body_create_from_bytes.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
_dll.trench_body_create_from_bytes.restype = ctypes.c_void_p

_dll.trench_body_destroy.argtypes = [ctypes.c_void_p]
_dll.trench_body_destroy.restype = None

_dll.trench_body_get_words.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint16 * 5)]
_dll.trench_body_get_words.restype = ctypes.c_int

_dll.trench_body_set_words.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint16 * 5)]
_dll.trench_body_set_words.restype = ctypes.c_int

_dll.trench_body_get_legacy_bytes.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_dll.trench_body_get_legacy_bytes.restype = ctypes.c_int

_dll.trench_body_get_native_bytes.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_dll.trench_body_get_native_bytes.restype = None

_dll.trench_body_is_legacy_representable.argtypes = [ctypes.c_void_p]
_dll.trench_body_is_legacy_representable.restype = ctypes.c_int

_dll.trench_body_interpolate_words.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.POINTER((ctypes.c_uint16 * 5) * 7)]
_dll.trench_body_interpolate_words.restype = None

_dll.trench_body_interpolate_biquads.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.POINTER((ctypes.c_double * 5) * 7)]
_dll.trench_body_interpolate_biquads.restype = None

_dll.trench_body_interpolate_biquads_float.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.POINTER((ctypes.c_double * 5) * 7)]
_dll.trench_body_interpolate_biquads_float.restype = None

_dll.trench_body_cascade.argtypes = [ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float, ctypes.c_double, ctypes.c_double, ctypes.POINTER((ctypes.c_double * 5) * 7)]
_dll.trench_body_cascade.restype = None

_dll.trench_section_geometry_get.argtypes = [ctypes.POINTER(ctypes.c_uint16 * 5), ctypes.c_double, ctypes.POINTER(TrenchSectionGeometry)]
_dll.trench_section_geometry_get.restype = None

_dll.trench_section_geometry_set.argtypes = [ctypes.POINTER(TrenchSectionGeometry), ctypes.c_double, ctypes.POINTER(ctypes.c_uint16 * 5)]
_dll.trench_section_geometry_set.restype = None

_dll.trench_section_design.argtypes = [ctypes.POINTER(ctypes.c_uint16 * 5), ctypes.c_double, ctypes.POINTER(ctypes.c_double * 5)]
_dll.trench_section_design.restype = None

_dll.trench_cascade_response_db.argtypes = [ctypes.POINTER((ctypes.c_double * 5) * 7), ctypes.c_size_t, ctypes.POINTER(ctypes.c_double), ctypes.c_size_t, ctypes.c_double, ctypes.POINTER(ctypes.c_double)]
_dll.trench_cascade_response_db.restype = None

_dll.trench_section_response_db.argtypes = [ctypes.POINTER(ctypes.c_double * 5), ctypes.c_double, ctypes.c_double]
_dll.trench_section_response_db.restype = ctypes.c_double

_dll.trench_runner_create.argtypes = [ctypes.c_double]
_dll.trench_runner_create.restype = ctypes.c_void_p

_dll.trench_runner_destroy.argtypes = [ctypes.c_void_p]
_dll.trench_runner_destroy.restype = None

_dll.trench_runner_set_target.argtypes = [ctypes.c_void_p, ctypes.POINTER((ctypes.c_double * 5) * 7), ctypes.c_size_t]
_dll.trench_runner_set_target.restype = None

_dll.trench_runner_set_immediate.argtypes = [ctypes.c_void_p, ctypes.POINTER((ctypes.c_double * 5) * 7), ctypes.c_size_t]
_dll.trench_runner_set_immediate.restype = None

_dll.trench_runner_set_glide.argtypes = [ctypes.c_void_p, ctypes.POINTER((ctypes.c_double * 5) * 7), ctypes.c_size_t, ctypes.c_size_t]
_dll.trench_runner_set_glide.restype = None

_dll.trench_runner_set_bite.argtypes = [ctypes.c_void_p, ctypes.c_double]
_dll.trench_runner_set_bite.restype = None

_dll.trench_runner_set_sample_rate.argtypes = [ctypes.c_void_p, ctypes.c_double]
_dll.trench_runner_set_sample_rate.restype = None

_dll.trench_runner_set_ring_leveller.argtypes = [ctypes.c_void_p, ctypes.c_int]
_dll.trench_runner_set_ring_leveller.restype = None

_dll.trench_runner_reset.argtypes = [ctypes.c_void_p]
_dll.trench_runner_reset.restype = None

_dll.trench_runner_process.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_size_t]
_dll.trench_runner_process.restype = None

_dll.trench_transpose_cascade.argtypes = [ctypes.POINTER((ctypes.c_double * 5) * 7), ctypes.c_size_t, ctypes.c_double, ctypes.c_double, ctypes.POINTER((ctypes.c_double * 5) * 7)]
_dll.trench_transpose_cascade.restype = None

_dll.trench_audio_speech_poles.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_size_t, ctypes.c_double, ctypes.c_size_t, ctypes.POINTER(TrenchResonance)]
_dll.trench_audio_speech_poles.restype = ctypes.c_size_t

_dll.trench_audio_resonances.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.c_size_t, ctypes.c_double, ctypes.c_size_t, ctypes.POINTER(TrenchResonance)]
_dll.trench_audio_resonances.restype = ctypes.c_size_t

_dll.trench_p2k_mag_word_for.argtypes = [ctypes.c_double, ctypes.c_uint16]
_dll.trench_p2k_mag_word_for.restype = ctypes.c_uint16

_dll.trench_p2k_dial_word.argtypes = [ctypes.c_size_t]
_dll.trench_p2k_dial_word.restype = ctypes.c_uint16

_dll.trench_p2k_dial_of_word.argtypes = [ctypes.c_uint16]
_dll.trench_p2k_dial_of_word.restype = ctypes.c_size_t

def decode_word(word: int) -> float:
    return _dll.trench_decode_word(ctypes.c_uint16(word))

def decode_fractional(word: float) -> float:
    return _dll.trench_decode_fractional(ctypes.c_double(word))

def encode_word(value: float) -> int:
    return int(_dll.trench_encode_word(ctypes.c_double(value)))

def interpolate_word(a: int, b: int, fraction: float) -> int:
    return int(_dll.trench_interpolate_word(ctypes.c_uint16(a), ctypes.c_uint16(b), ctypes.c_float(fraction)))

def p2k_mag_word_for(hz: float, rsq_word: int) -> int:
    return int(_dll.trench_p2k_mag_word_for(ctypes.c_double(hz), ctypes.c_uint16(rsq_word)))

def p2k_dial_word(byte_val: int) -> int:
    return int(_dll.trench_p2k_dial_word(ctypes.c_size_t(byte_val)))

def p2k_dial_of_word(word: int) -> int:
    return int(_dll.trench_p2k_dial_of_word(ctypes.c_uint16(word)))

class Body:
    def __init__(self, handle: int):
        if not handle:
            raise ValueError("Invalid null Body handle")
        self._handle = handle

    def __del__(self):
        if hasattr(self, "_handle") and self._handle:
            _dll.trench_body_destroy(ctypes.c_void_p(self._handle))
            self._handle = 0

    @classmethod
    def from_legacy_bytes(cls, data: bytes) -> "Body":
        if len(data) < 240:
            raise ValueError("Legacy body requires at least 240 bytes")
        h = _dll.trench_body_create_legacy(data, len(data))
        if not h:
            raise RuntimeError("Failed to create body from legacy bytes")
        return cls(h)

    @classmethod
    def from_native_bytes(cls, data: bytes) -> "Body":
        if len(data) < 560:
            raise ValueError("Native body requires at least 560 bytes")
        h = _dll.trench_body_create_native(data, len(data))
        if not h:
            raise RuntimeError("Failed to create body from native bytes")
        return cls(h)

    @classmethod
    def from_bytes(cls, data: bytes) -> "Body":
        if len(data) == 240:
            return cls.from_legacy_bytes(data)
        if len(data) == 560:
            return cls.from_native_bytes(data)
        raise ValueError(f"Expected 240 or 560 bytes, got {len(data)}")

    @classmethod
    def from_file(cls, path: str) -> "Body":
        with open(path, "rb") as f:
            return cls.from_bytes(f.read())

    def is_legacy_representable(self) -> bool:
        return bool(_dll.trench_body_is_legacy_representable(ctypes.c_void_p(self._handle)))

    def to_legacy_bytes(self) -> bytes:
        buf = ctypes.create_string_buffer(240)
        ok = _dll.trench_body_get_legacy_bytes(ctypes.c_void_p(self._handle), buf)
        if not ok:
            raise ValueError("Body cannot be represented in legacy 240-byte format")
        return bytes(buf.raw)

    def to_native_bytes(self) -> bytes:
        buf = ctypes.create_string_buffer(560)
        _dll.trench_body_get_native_bytes(ctypes.c_void_p(self._handle), buf)
        return bytes(buf.raw)

    def get_words(self, corner: int, section: int) -> List[int]:
        buf = (ctypes.c_uint16 * 5)()
        ok = _dll.trench_body_get_words(ctypes.c_void_p(self._handle), ctypes.c_size_t(corner), ctypes.c_size_t(section), ctypes.byref(buf))
        if not ok:
            raise IndexError(f"Corner {corner} or section {section} out of range")
        return [int(buf[i]) for i in range(5)]

    def set_words(self, corner: int, section: int, words: List[int]):
        if len(words) != 5:
            raise ValueError("Section requires 5 words")
        buf = (ctypes.c_uint16 * 5)(*words)
        ok = _dll.trench_body_set_words(ctypes.c_void_p(self._handle), ctypes.c_size_t(corner), ctypes.c_size_t(section), ctypes.byref(buf))
        if not ok:
            raise IndexError(f"Corner {corner} or section {section} out of range")

    def interpolate_words(self, morph: float, q: float, z: float = 0.0) -> List[List[int]]:
        buf = ((ctypes.c_uint16 * 5) * 7)()
        _dll.trench_body_interpolate_words(
            ctypes.c_void_p(self._handle),
            ctypes.c_float(morph),
            ctypes.c_float(q),
            ctypes.c_float(z),
            ctypes.byref(buf),
        )
        return [[int(buf[s][i]) for i in range(5)] for s in range(7)]

    def interpolate_biquads(self, morph: float, q: float, z: float = 0.0) -> List[List[float]]:
        buf = ((ctypes.c_double * 5) * 7)()
        _dll.trench_body_interpolate_biquads(
            ctypes.c_void_p(self._handle),
            ctypes.c_float(morph),
            ctypes.c_float(q),
            ctypes.c_float(z),
            ctypes.byref(buf),
        )
        return [[float(buf[s][i]) for i in range(5)] for s in range(7)]

    def interpolate_biquads_float(self, morph: float, q: float, z: float = 0.0) -> List[List[float]]:
        buf = ((ctypes.c_double * 5) * 7)()
        _dll.trench_body_interpolate_biquads_float(
            ctypes.c_void_p(self._handle),
            ctypes.c_float(morph),
            ctypes.c_float(q),
            ctypes.c_float(z),
            ctypes.byref(buf),
        )
        return [[float(buf[s][i]) for i in range(5)] for s in range(7)]

    def cascade(self, morph: float, q: float, z: float = 0.0, datum_hz: float = 44100.0, host_hz: float = 44100.0) -> List[List[float]]:
        buf = ((ctypes.c_double * 5) * 7)()
        _dll.trench_body_cascade(
            ctypes.c_void_p(self._handle),
            ctypes.c_float(morph),
            ctypes.c_float(q),
            ctypes.c_float(z),
            ctypes.c_double(datum_hz),
            ctypes.c_double(host_hz),
            ctypes.byref(buf),
        )
        return [[float(buf[s][i]) for i in range(5)] for s in range(7)]

def cascade_response_db(
    biquads: Union[List[List[float]], np.ndarray],
    freqs_hz: Union[List[float], np.ndarray],
    sample_rate_hz: float = 44100.0,
) -> np.ndarray:
    n_sec = len(biquads)
    n_pts = len(freqs_hz)
    bq_arr = ((ctypes.c_double * 5) * 7)()
    for s in range(min(n_sec, 7)):
        for i in range(5):
            bq_arr[s][i] = float(biquads[s][i])
    for s in range(n_sec, 7):
        bq_arr[s][0] = 1.0
        bq_arr[s][1] = 0.0
        bq_arr[s][2] = 0.0
        bq_arr[s][3] = 0.0
        bq_arr[s][4] = 0.0

    freqs_c = (ctypes.c_double * n_pts)(*[float(f) for f in freqs_hz])
    out_c = (ctypes.c_double * n_pts)()
    _dll.trench_cascade_response_db(
        ctypes.byref(bq_arr),
        ctypes.c_size_t(n_sec),
        freqs_c,
        ctypes.c_size_t(n_pts),
        ctypes.c_double(sample_rate_hz),
        out_c,
    )
    return np.array([float(out_c[p]) for p in range(n_pts)], dtype=np.float64)

def section_response_db(biquad: List[float], freq_hz: float, sample_rate_hz: float = 44100.0) -> float:
    arr = (ctypes.c_double * 5)(*[float(x) for x in biquad])
    return float(_dll.trench_section_response_db(ctypes.byref(arr), ctypes.c_double(freq_hz), ctypes.c_double(sample_rate_hz)))

def transpose_cascade(biquads: List[List[float]], ratio: float, host_hz: float = 44100.0) -> List[List[float]]:
    n_sec = len(biquads)
    in_arr = ((ctypes.c_double * 5) * 7)()
    out_arr = ((ctypes.c_double * 5) * 7)()
    for s in range(min(n_sec, 7)):
        for i in range(5):
            in_arr[s][i] = float(biquads[s][i])
    for s in range(n_sec, 7):
        in_arr[s][0] = 1.0
        in_arr[s][1] = 0.0
        in_arr[s][2] = 0.0
        in_arr[s][3] = 0.0
        in_arr[s][4] = 0.0
    _dll.trench_transpose_cascade(
        ctypes.byref(in_arr),
        ctypes.c_size_t(n_sec),
        ctypes.c_double(ratio),
        ctypes.c_double(host_hz),
        ctypes.byref(out_arr),
    )
    return [[float(out_arr[s][i]) for i in range(5)] for s in range(7)]

class Runner:
    def __init__(self, sample_rate_hz: float = 44100.0):
        self._handle = _dll.trench_runner_create(ctypes.c_double(sample_rate_hz))
        if not self._handle:
            raise RuntimeError("Failed to create CascadeRunner")

    def __del__(self):
        if hasattr(self, "_handle") and self._handle:
            _dll.trench_runner_destroy(ctypes.c_void_p(self._handle))
            self._handle = 0

    def set_immediate(self, biquads: List[List[float]]):
        n_sec = len(biquads)
        arr = ((ctypes.c_double * 5) * 7)()
        for s in range(min(n_sec, 7)):
            for i in range(5):
                arr[s][i] = float(biquads[s][i])
        for s in range(n_sec, 7):
            arr[s][0] = 1.0
            arr[s][1] = 0.0
            arr[s][2] = 0.0
            arr[s][3] = 0.0
            arr[s][4] = 0.0
        _dll.trench_runner_set_immediate(ctypes.c_void_p(self._handle), ctypes.byref(arr), ctypes.c_size_t(n_sec))

    def set_target(self, biquads: List[List[float]]):
        n_sec = len(biquads)
        arr = ((ctypes.c_double * 5) * 7)()
        for s in range(min(n_sec, 7)):
            for i in range(5):
                arr[s][i] = float(biquads[s][i])
        for s in range(n_sec, 7):
            arr[s][0] = 1.0
            arr[s][1] = 0.0
            arr[s][2] = 0.0
            arr[s][3] = 0.0
            arr[s][4] = 0.0
        _dll.trench_runner_set_target(ctypes.c_void_p(self._handle), ctypes.byref(arr), ctypes.c_size_t(n_sec))

    def set_glide(self, biquads: List[List[float]], samples: int):
        n_sec = len(biquads)
        arr = ((ctypes.c_double * 5) * 7)()
        for s in range(min(n_sec, 7)):
            for i in range(5):
                arr[s][i] = float(biquads[s][i])
        for s in range(n_sec, 7):
            arr[s][0] = 1.0
            arr[s][1] = 0.0
            arr[s][2] = 0.0
            arr[s][3] = 0.0
            arr[s][4] = 0.0
        _dll.trench_runner_set_glide(ctypes.c_void_p(self._handle), ctypes.byref(arr), ctypes.c_size_t(n_sec), ctypes.c_size_t(samples))

    def set_bite(self, bite: float):
        _dll.trench_runner_set_bite(ctypes.c_void_p(self._handle), ctypes.c_double(bite))

    def set_sample_rate(self, sample_rate_hz: float):
        _dll.trench_runner_set_sample_rate(ctypes.c_void_p(self._handle), ctypes.c_double(sample_rate_hz))

    def set_ring_leveller(self, enabled: bool):
        _dll.trench_runner_set_ring_leveller(ctypes.c_void_p(self._handle), ctypes.c_int(1 if enabled else 0))

    def reset(self):
        _dll.trench_runner_reset(ctypes.c_void_p(self._handle))

    def process(self, samples: np.ndarray) -> np.ndarray:
        block = np.ascontiguousarray(samples, dtype=np.float32)
        n = block.size
        ptr = block.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        _dll.trench_runner_process(ctypes.c_void_p(self._handle), ptr, ctypes.c_size_t(n))
        return block

def speech_poles(mono_samples: np.ndarray, sample_rate_hz: float, max_count: int = 6) -> List[Tuple[float, float, float]]:
    samples = np.ascontiguousarray(mono_samples, dtype=np.float32)
    res = (TrenchResonance * max_count)()
    ptr = samples.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
    written = _dll.trench_audio_speech_poles(ptr, ctypes.c_size_t(samples.size), ctypes.c_double(sample_rate_hz), ctypes.c_size_t(max_count), res)
    return [(float(res[i].hz), float(res[i].bw_hz), float(res[i].gain_db)) for i in range(written)]

def audio_resonances(mono_samples: np.ndarray, sample_rate_hz: float, max_count: int = 6) -> List[Tuple[float, float, float]]:
    samples = np.ascontiguousarray(mono_samples, dtype=np.float32)
    res = (TrenchResonance * max_count)()
    ptr = samples.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
    written = _dll.trench_audio_resonances(ptr, ctypes.c_size_t(samples.size), ctypes.c_double(sample_rate_hz), ctypes.c_size_t(max_count), res)
    return [(float(res[i].hz), float(res[i].bw_hz), float(res[i].gain_db)) for i in range(written)]
