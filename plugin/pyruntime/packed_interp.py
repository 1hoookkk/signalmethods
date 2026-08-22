from __future__ import annotations

import ctypes
import math

try:
    from . import trench_ffi as _core
except ImportError:
    try:
        import trench_ffi as _core  # type: ignore
    except ImportError:
        _core = None

COMBINE_K = 4.0

def core_available() -> bool:
    return _core is not None and _core.available()

def core_backend() -> str:
    return "trench-core" if core_available() else "python-fallback"

def _require_core(fn_name: str) -> None:
    if _core is None or not _core.available():
        raise RuntimeError(
            f"{fn_name} requires the trench-core shared library — "
            "build it (`cargo build -p trench-core --release`) and ensure the "
            "DLL/.so is on the load path. The pure-Python reference is not the "
            "shipping interpolation path; refusing to silently substitute."
        )

def decode(word: int) -> float:
    if _core is not None and _core.available():
        return _core.decode(word)
    u = (int(word) & 0xFFFF) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = u & 0xFFF
    x = m / 4096.0 if e == 0 else (m | 0x1000) / 8192.0
    return math.ldexp(x, e - 15)

def encode(value: float) -> int:
    if _core is not None and _core.available():
        return _core.encode(value)
    v = float(value)
    if v >= 1.0:
        return 0xFFFF
    if v <= 0.0:
        return 0x0000

    denorm_mant = round(v * 134_217_728.0)
    if 0 < denorm_mant <= 0xFFF:
        return (denorm_mant - 1) & 0xFFFF

    log2_val = math.log2(v)
    exp_stored = min(int(math.floor(log2_val)) + 1, 0)
    if exp_stored < -14:
        return 0x0000

    biased_exp = exp_stored + 15
    scale = 2.0 ** (exp_stored - 13)
    mant_with_hidden = round(v / scale)

    if mant_with_hidden >= 0x2000:
        if exp_stored < 0:
            biased_exp += 1
            exp_stored += 1
            scale2 = 2.0 ** (exp_stored - 13)
            mant_with_hidden = round(v / scale2)
            mant = min(mant_with_hidden & 0xFFF, 0xFFF)
            u = (biased_exp << 12) | mant
            return (u - 1) & 0xFFFF
        return 0xFFFF

    mant = min(max(mant_with_hidden - 0x1000, 0), 0xFFF)
    u = (biased_exp << 12) | mant
    return (u - 1) & 0xFFFF

def _f32(x: float) -> float:
    return ctypes.c_float(x).value

def lerp_u16(a: int, b: int, frac: float) -> int:
    a = int(a) & 0xFFFF
    b = int(b) & 0xFFFF
    product = _f32(_f32(b - a) * _f32(frac))
    trunc = int(product)
    delta_i16 = ((trunc + 0x8000) % 0x10000) - 0x8000
    return (a + delta_i16) & 0xFFFF

def coeffs_to_words(c0: float, c1: float, c2: float, c3: float, c4: float) -> tuple[int, ...]:
    return (
        encode((c0 - c1) / COMBINE_K),
        encode(c1),
        encode((c2 - c3) / COMBINE_K),
        encode(c3),
        encode(c4 / COMBINE_K),
    )

def words_to_coeffs(words: tuple[int, ...]) -> tuple[float, ...]:
    d = [decode(w) for w in words]
    return (
        COMBINE_K * d[0] + d[1],
        d[1],
        COMBINE_K * d[2] + d[3],
        d[3],
        COMBINE_K * d[4],
    )

def kernel_to_biquad(row: tuple[float, ...]) -> tuple[float, ...]:
    c0, c1, c2, c3, c4 = row
    return (c4, (c0 - 2.0) * c4, (1.0 - c1) * c4, c2 - 2.0, 1.0 - c3)

def _pole_radius(a1: float, a2: float) -> float:
    if not math.isfinite(a1) or not math.isfinite(a2):
        return math.inf
    disc = a1 * a1 - 4.0 * a2
    if disc < 0.0:
        return max(a2, 0.0) ** 0.5
    sq = disc ** 0.5
    return max(abs((-a1 + sq) / 2.0), abs((-a1 - sq) / 2.0))

def packed_probe(
    corner_words: dict[str, list[tuple[int, ...]]],
    morph: float,
    q: float,
) -> dict:
    _require_core("packed_probe")
    body_bytes = _core.body_bytes_from_corner_words(corner_words)
    return _core.packed_probe(body_bytes, morph, q)

def _packed_bilinear_reference(
    corner_words: dict[str, list[tuple[int, ...]]],
    morph: float,
    q: float,
) -> list[tuple[float, ...]]:
    num_stages = len(corner_words["A"])
    result = []
    for si in range(num_stages):
        a = corner_words["A"][si]
        b = corner_words["B"][si]
        c = corner_words["C"][si]
        d = corner_words["D"][si]

        out_words = tuple(
            lerp_u16(
                lerp_u16(a[wi], b[wi], morph),
                lerp_u16(c[wi], d[wi], morph),
                q,
            )
            for wi in range(5)
        )
        result.append(words_to_coeffs(out_words))
    return result

def packed_bilinear(
    corner_words: dict[str, list[tuple[int, ...]]],
    morph: float,
    q: float,
) -> list[tuple[float, ...]]:
    _require_core("packed_bilinear")
    return _core.packed_bilinear(corner_words, morph, q)

def build_corner_words_from_coeffs(
    corner_coeffs: dict[str, list[tuple[float, ...]]],
) -> dict[str, list[tuple[int, ...]]]:
    return {
        name: [coeffs_to_words(*stage) for stage in stages]
        for name, stages in corner_coeffs.items()
    }
