"""Step null: EmulatorX3's Talking Hedz with Morph stepping 0 -> 1 at 2.0 s
against the TRENCH engine rendered headless on the same dry file with the
same step, X3-movement path on and off.  Filter only (AGC, DC, saturation,
nonlinearity, preamp, grit, key off).  The residual around the step is the
movement law."""
import ctypes
import math
import pathlib
import sys

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, r"C:\Users\hooki\trench-native\plugin\dev\experiments\phaser2_capture_diagnostic_20260813")
import headless_null as hn  # noqa: E402

SR = 44100.0
BLOCK = 512
BODY = pathlib.Path(r"C:\Users\hooki\trench-native\plugin\ref\presets\P2k_013_talking_hedz.bin").read_bytes()
WORDS = list(np.frombuffer(BODY, dtype="<u2"))
DRY = pathlib.Path(r"C:\Users\hooki\Downloads\trench_capture\dry_saw_49hz_-12dBFS.wav")
CAP = pathlib.Path(r"C:\Users\hooki\Downloads\hedzstep.wav")
STEP_S = 2.0
Q = 0.5


def mono(path):
    sr, x = wavfile.read(path)
    x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(1)
    assert sr == SR
    return x


def render(lib, source, morph, x3_movement):
    engine = lib.trench_engine_create()
    try:
        lib.trench_engine_prepare(engine, SR)
        lib.trench_engine_load_body_bytes_at.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_double]
        lib.trench_engine_load_body_bytes_at.restype = ctypes.c_int
        rc = lib.trench_engine_load_body_bytes_at(engine, BODY, len(BODY), SR)
        assert rc == 0, rc
        lib.trench_engine_set_parameters(engine, float(morph[0]), Q, 0.0, 0.0, 1.0)
        lib.trench_engine_set_spatial_mode(engine, 2)
        lib.trench_engine_set_key_snap(engine, 0)
        lib.trench_engine_set_input_mode(engine, 0)
        lib.trench_engine_set_grit(engine, 0.0)
        lib.trench_engine_set_input_preamp(engine, 0.0)
        lib.trench_engine_set_agc_enabled(engine, 0)
        lib.trench_engine_set_dc_block_enabled(engine, 0)
        lib.trench_engine_set_saturation_enabled(engine, 0)
        lib.trench_engine_set_nonlinearity_enabled(engine, 0)
        lib.trench_engine_set_x3_movement.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.trench_engine_set_x3_movement.restype = None
        lib.trench_engine_set_x3_movement(engine, int(x3_movement))
        lib.trench_engine_process_trajectory.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
            ctypes.c_int, ctypes.POINTER(ctypes.c_float), ctypes.c_double]
        lib.trench_engine_process_trajectory.restype = None
        out = np.empty(len(source), dtype=np.float32)
        for off in range(0, len(source), BLOCK):
            end = min(off + BLOCK, len(source))
            left = np.array(source[off:end], dtype=np.float32)
            right = left.copy()
            traj = np.ascontiguousarray(morph[off:end], dtype=np.float32)
            lib.trench_engine_process_trajectory(
                engine, left.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                right.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), len(left),
                traj.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), Q)
            out[off:end] = left
        return out.astype(np.float64)
    finally:
        lib.trench_engine_destroy(engine)


def rms_db(x):
    return 10 * math.log10(np.mean(x * x) + 1e-30)


def main():
    lib = hn.bind()
    dry = mono(DRY)
    cap = mono(CAP)
    n = min(len(dry), len(cap))
    dry, cap = dry[:n], cap[:n]
    step = int(STEP_S * SR)
    morph = np.zeros(n)
    morph[step:] = 1.0
    renders = {"x3 path": render(lib, dry, morph, True), "per-sample": render(lib, dry, morph, False)}

    def window(x, a, b):
        return x[int(a * SR): int(b * SR)]

    # level match on the settled tail (both corners are known to null)
    gain = {k: rms_db(window(cap, 6, 11)) - rms_db(window(r, 6, 11)) for k, r in renders.items()}
    print("level offset capture - render on the settled tail:",
          {k: f"{g:+.2f} dB" for k, g in gain.items()})
    for k in renders:
        renders[k] *= 10 ** (gain[k] / 20)

    print("\nnull depth (residual rms relative to capture rms), per region:")
    regions = [("before step 1.0-1.9 s", 1.0, 1.9), ("step 1.99-2.01", 1.99, 2.01), ("2.01-2.03", 2.01, 2.03),
               ("2.03-2.06", 2.03, 2.06), ("2.06-2.10", 2.06, 2.10), ("2.10-2.20", 2.10, 2.20),
               ("2.2-2.5", 2.2, 2.5), ("settled 6-11", 6.0, 11.0)]
    print(f"   {'region':<22}" + "".join(f"{k:>14}" for k in renders))
    for name, a, b in regions:
        c = window(cap, a, b)
        line = f"   {name:<22}"
        for k, r in renders.items():
            line += f"{rms_db(c - window(r, a, b)) - rms_db(c):14.1f}"
        print(line)

    # where does X3 settle?  residual vs the instantaneous render, 1 ms bins after the step
    print("\nresidual vs the per-sample (instant) render in 1 ms bins after the step, dB rel. capture:")
    r = renders["per-sample"]
    for ms in range(0, 40, 2):
        a = STEP_S + ms / 1000
        b = a + 0.001
        print(f"   +{ms:2d} ms: {rms_db(window(cap, a, b) - window(r, a, b)) - rms_db(window(cap, a, b)):6.1f}")
    print("\nsame, vs the x3-path render:")
    r = renders["x3 path"]
    for ms in range(0, 40, 2):
        a = STEP_S + ms / 1000
        b = a + 0.001
        print(f"   +{ms:2d} ms: {rms_db(window(cap, a, b) - window(r, a, b)) - rms_db(window(cap, a, b)):6.1f}")

    # does the capture's step start exactly at 2.000 s?  first 1 ms bin where capture departs from the pre-step render
    pre = render(lib, dry, np.zeros(n), False) * 10 ** (gain["per-sample"] / 20)
    for ms in range(-5, 15):
        a = STEP_S + ms / 1000
        d = rms_db(window(cap, a, a + 0.001) - window(pre, a, a + 0.001)) - rms_db(window(cap, a, a + 0.001))
        if d > -20:
            print(f"\ncapture departs from morph-0 at step {ms:+d} ms ({d:.1f} dB)")
            break
    for k, r in renders.items():
        wavfile.write(rf"C:\Users\hooki\Downloads\trench_capture\render_step_{k.replace(' ', '_')}.wav", int(SR), r.astype(np.float32))


if __name__ == "__main__":
    main()
