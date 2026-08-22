import ctypes
import glob
import os
import sys

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DLL = os.path.join(ROOT, "target", "release", "trench_core.dll")
BODIES = os.path.join(ROOT, "filters", "bodies", "CAVL_beer_bottle_to_bathtub.body240")
SR = 44100.0
N = int(SR * 2.0)
CHUNK = 512


def bind(path):
    lib = ctypes.CDLL(path)
    lib.trench_engine_create.restype = ctypes.c_void_p
    lib.trench_engine_destroy.argtypes = [ctypes.c_void_p]
    lib.trench_engine_prepare.argtypes = [ctypes.c_void_p, ctypes.c_double]
    lib.trench_engine_load_body_bytes.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    lib.trench_engine_load_body_bytes.restype = ctypes.c_int
    for name in ("set_agc_enabled", "set_dc_block_enabled", "set_saturation_enabled",
                 "set_nonlinearity_enabled", "set_key_snap", "set_input_mode",
                 "set_spatial_mode", "set_x3_movement"):
        getattr(lib, "trench_engine_" + name).argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.trench_engine_set_parameters.argtypes = [
        ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float,
        ctypes.c_float, ctypes.c_float]
    lib.trench_engine_set_grit.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.trench_engine_set_input_preamp.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.trench_engine_process_trajectory.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
        ctypes.c_int, ctypes.POINTER(ctypes.c_float), ctypes.c_double]
    return lib


def render(lib, body, source, morphs, x3):
    engine = lib.trench_engine_create()
    if not engine:
        raise RuntimeError("trench_engine_create failed")
    try:
        lib.trench_engine_prepare(engine, SR)
        raw = (ctypes.c_uint8 * len(body))(*body)
        rc = lib.trench_engine_load_body_bytes(engine, raw, len(body))
        if rc != 0:
            raise RuntimeError(f"load_body_bytes rc={rc}")
        lib.trench_engine_set_parameters(engine, 0.0, 0.0, 0.0, 0.0, 1.0)
        lib.trench_engine_set_spatial_mode(engine, 2)
        lib.trench_engine_set_key_snap(engine, 0)
        lib.trench_engine_set_input_mode(engine, 0)
        lib.trench_engine_set_grit(engine, 0.0)
        lib.trench_engine_set_input_preamp(engine, 0.0)
        lib.trench_engine_set_agc_enabled(engine, 0)
        lib.trench_engine_set_dc_block_enabled(engine, 0)
        lib.trench_engine_set_saturation_enabled(engine, 0)
        lib.trench_engine_set_nonlinearity_enabled(engine, 0)
        lib.trench_engine_set_x3_movement(engine, 1 if x3 else 0)
        out = np.empty(source.shape, dtype=np.float32)
        for off in range(0, len(source), CHUNK):
            end = min(off + CHUNK, len(source))
            left = np.array(source[off:end], dtype=np.float32, copy=True)
            right = left.copy()
            traj = np.ascontiguousarray(morphs[off:end], dtype=np.float32)
            lib.trench_engine_process_trajectory(
                engine, left.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                right.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                len(left), traj.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), 0.0)
            out[off:end] = left
        return out
    finally:
        lib.trench_engine_destroy(engine)


def saw(n, hz):
    t = np.arange(n) / SR
    return (0.25 * (2.0 * ((t * hz) % 1.0) - 1.0)).astype(np.float32)


def env(x, hop=128):
    m = len(x) // hop
    return np.sqrt(np.mean(np.square(x[: m * hop].reshape(m, hop)), axis=1))


def main():
    bodies = sorted(glob.glob(BODIES))
    if not bodies:
        print("no shipped bodies found")
        return 2
    path = bodies[0]
    body = open(path, "rb").read()
    lib = bind(DLL)
    source = saw(N, 110.0)
    t = np.arange(N) / SR

    tri = 1.0 - np.abs(1.0 - 2.0 * (t / t[-1]))
    step = np.where(t < 1.0, 0.2, 0.8).astype(np.float32)
    fast = 0.5 + 0.45 * np.sin(2.0 * np.pi * 6.0 * t)

    scen = [("triangle sweep 0-1-0 over 2 s", tri),
            ("step 0.2 -> 0.8 at 1.0 s", step),
            ("6 Hz wheel sine, depth 0.45", fast)]

    fig, axes = plt.subplots(len(scen), 2, figsize=(14, 9))
    hop = 128
    for row, (name, morphs) in enumerate(scen):
        m = morphs.astype(np.float32)
        a = render(lib, body, source, m, x3=False)
        b = render(lib, body, source, m, x3=True)
        ea, eb = env(a, hop), env(b, hop)
        te = np.arange(len(ea)) * hop / SR
        resid = 20.0 * np.log10(
            max(np.sqrt(np.mean((a.astype(np.float64) - b.astype(np.float64)) ** 2)), 1e-30)
            / max(np.sqrt(np.mean(np.square(a.astype(np.float64)))), 1e-30))
        ax = axes[row][0]
        ax.plot(te, 20 * np.log10(np.maximum(ea, 1e-9)), lw=0.8, label="shipping (per-sample)")
        ax.plot(te, 20 * np.log10(np.maximum(eb, 1e-9)), lw=0.8, label="X3 parity (block+ramp)")
        ax.set_title(f"{name}   residual {resid:.1f} dB", fontsize=9)
        ax.set_ylabel("RMS dB")
        ax.legend(fontsize=7)
        zoom = slice(int(SR * 0.99), int(SR * 1.05)) if row == 1 else slice(int(SR * 0.5), int(SR * 0.5) + 2048)
        ax2 = axes[row][1]
        ax2.plot(t[zoom], a[zoom], lw=0.6, label="shipping")
        ax2.plot(t[zoom], b[zoom], lw=0.6, label="X3 parity")
        ax2.set_title("waveform zoom", fontsize=9)
        ax2.legend(fontsize=7)
        print(f"{name}: residual {resid:.1f} dB re shipping")
    axes[-1][0].set_xlabel("s")
    axes[-1][1].set_xlabel("s")
    fig.suptitle(f"X3 movement parity A/B - {os.path.basename(path)} - saw 110 Hz", fontsize=11)
    fig.tight_layout()
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "movement_ab.png")
    fig.savefig(out, dpi=110)
    print("wrote", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
