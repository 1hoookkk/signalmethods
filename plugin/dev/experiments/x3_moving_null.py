"""Moving-wheel null: the real trench_core engine against an independent
Python implementation of the recovered X3 movement law (X3_MOVEMENT_SPEC.md):

  - the COMPLETE summed Morph target one-poled once per 32-sample tick
    (s = s - s*R + target; out = R*s; R = 0.4516276717185974)
  - packed u16 words interpolated once per tick at the smoothed coordinate
  - kernel rows ramped linearly across the block, biquad derived per sample
  - the change-test skip freezing the ramp when nothing moved

KEY OFF, filter only: AGC, DC blocker, saturation, section nonlinearity,
spatial, preamp, GRIT all disabled. Raw native-rate null, no gain fitting.

This nulls our engine against a re-implementation of the recovered law.
It is NOT machine parity: no X3 moving-wheel capture exists and the
DLL-in-host render path has never been built.
"""
import ctypes
import json
import math
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "phaser2_capture_diagnostic_20260813"))
import headless_null as hn

R = 0.4516276717185974
BLOCK = 512
TICK = 32
SAMPLE_RATE = 48_000


def kernel_from_words(packed):
    d0, d1, d2, d3, d4 = map(hn.decode, packed)
    return [4 * d0 + d1, d1, 4 * d2 + d3, d3, 4 * d4]


def kernel_to_biquad(k):
    k0, k1, k2, k3, k4 = k
    return [k4, (k0 - 2) * k4, (1 - k1) * k4, k2 - 2, 1 - k3]


def biquad_to_kernel(b):
    b0, b1, b2, a1, a2 = b
    if b0 == 0.0:
        return [2.0, 1.0, a1 + 2.0, 1.0 - a2, 0.0]
    return [b1 / b0 + 2.0, 1.0 - b2 / b0, a1 + 2.0, 1.0 - a2, b0]


def words_at(words, active_stages, coord32, q):
    corners = np.asarray(words, dtype=np.uint16).reshape(4, active_stages, 5)
    rows = []
    for stage in range(active_stages):
        packed = []
        for column in range(5):
            q0 = hn.lerp_u16(corners[0, stage, column], corners[1, stage, column], coord32)
            q1 = hn.lerp_u16(corners[2, stage, column], corners[3, stage, column], coord32)
            packed.append(hn.lerp_u16(q0, q1, np.float32(q)))
        rows.append(tuple(packed))
    return rows


def reference_moving(source, morph, words, active_stages, q):
    n = len(source)
    nst = active_stages
    coeffs = [[0.0] * 5 for _ in range(nst)]
    kernel = [[0.0] * 5 for _ in range(nst)]
    ktarget = [[0.0] * 5 for _ in range(nst)]
    kdeltas = [[0.0] * 5 for _ in range(nst)]
    w1 = [0.0] * nst
    w2 = [0.0] * nst
    kernel_ramp = False
    kernel_valid = False
    pole_s = 0.0
    seeded = False
    prev_words = None
    out = np.empty(n, dtype=np.float32)

    for i in range(n):
        if i % TICK == 0:
            m = float(np.float32(morph[i]))
            if not seeded:
                seeded = True
                pole_s = m / R
                smoothed = m
            else:
                pole_s = pole_s - pole_s * R + m
                smoothed = R * pole_s
            rows_w = words_at(words, nst, np.float32(smoothed), q)
            if i == 0:
                for s in range(nst):
                    coeffs[s] = kernel_to_biquad(kernel_from_words(rows_w[s]))
                prev_words = rows_w
                kernel_ramp = False
                kernel_valid = False
            elif rows_w == prev_words:
                for s in range(nst):
                    kdeltas[s] = [0.0] * 5
            else:
                if not kernel_valid:
                    for s in range(nst):
                        kernel[s] = biquad_to_kernel(coeffs[s])
                        ktarget[s] = list(kernel[s])
                    kernel_valid = True
                for s in range(nst):
                    target = biquad_to_kernel(kernel_to_biquad(kernel_from_words(rows_w[s])))
                    kernel[s] = list(ktarget[s])
                    kdeltas[s] = [(target[j] - kernel[s][j]) / TICK for j in range(5)]
                    ktarget[s] = target
                kernel_ramp = True
                prev_words = rows_w
        v = float(np.float32(source[i]))
        for s in range(nst):
            if kernel_ramp:
                coeffs[s] = kernel_to_biquad(kernel[s])
            b0, b1, b2, a1, a2 = coeffs[s]
            y = b0 * v + w1[s]
            w1[s] = b1 * v - a1 * y + w2[s]
            w2[s] = b2 * v - a2 * y
            if kernel_ramp:
                for j in range(5):
                    kernel[s][j] += kdeltas[s][j]
            v = y
        out[i] = np.float32(v)
    return out


def engine_moving(lib, source, morph, words, active_stages, q):
    engine = lib.trench_engine_create()
    if not engine:
        raise RuntimeError("trench_engine_create failed")
    try:
        lib.trench_engine_prepare(engine, float(SAMPLE_RATE))
        packed = (ctypes.c_uint16 * len(words))(*words)
        rc = lib.trench_engine_load_runtime_preset(
            engine, b"moving null", packed, len(words), active_stages,
            float(SAMPLE_RATE), 1.0)
        if rc != 0:
            raise RuntimeError(f"load_runtime_preset failed: {rc}")
        lib.trench_engine_set_parameters(engine, float(morph[0]), q, 0.0, 0.0, 1.0)
        lib.trench_engine_set_spatial_mode(engine, 2)
        lib.trench_engine_set_key_snap(engine, 0)
        lib.trench_engine_set_input_mode(engine, 0)
        lib.trench_engine_set_grit(engine, 0.0)
        lib.trench_engine_set_input_preamp(engine, 0.0)
        lib.trench_engine_set_agc_enabled(engine, 0)
        lib.trench_engine_set_dc_block_enabled(engine, 0)
        lib.trench_engine_set_saturation_enabled(engine, 0)
        lib.trench_engine_set_nonlinearity_enabled(engine, 0)
        lib.trench_engine_process_trajectory.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_float),
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_int,
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_double,
        ]
        lib.trench_engine_process_trajectory.restype = None
        result = np.empty(source.shape, dtype=np.float32)
        for offset in range(0, len(source), BLOCK):
            end = min(offset + BLOCK, len(source))
            left = np.array(source[offset:end], dtype=np.float32, copy=True)
            right = left.copy()
            traj = np.ascontiguousarray(morph[offset:end], dtype=np.float32)
            lib.trench_engine_process_trajectory(
                engine,
                left.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                right.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                len(left),
                traj.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                q,
            )
            result[offset:end] = left
        return result
    finally:
        lib.trench_engine_destroy(engine)


def main():
    preset = json.loads(hn.PRESET.read_text())
    words = preset["banks"][str(SAMPLE_RATE)]
    active = int(preset["active_stages"])
    source = hn.stimulus()
    n = len(source)
    morph = np.linspace(0.0, 1.0, n, dtype=np.float32)
    q = 1.0

    dll_path = Path(hn.ROOT) / "target/release/trench_core.dll"
    hn.bind.__globals__["DLL"] = dll_path
    lib = hn.bind()
    obs = engine_moving(lib, source, morph, words, active, q)
    ref = reference_moving(source, morph, words, active, q)
    m = hn.metrics(obs, ref)
    print("moving null, phaser2, KEY OFF, filter only, wheel 0->1 over 3 s")
    print(f"  dll                     {dll_path}")
    print(f"  raw null RMS dBFS       {m['raw_null_rms_dbfs']:.2f}")
    print(f"  raw null peak dBFS      {m['raw_null_peak_dbfs']:.2f}")
    print(f"  raw null relative dB    {m['raw_null_relative_db']:.2f}")
    print(f"  correlation             {m['correlation']:.6f}")
    print(f"  sample exact            {m['sample_exact']}")


if __name__ == "__main__":
    main()
