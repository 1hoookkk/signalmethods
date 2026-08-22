import ctypes
import math
import os
import pathlib
import shutil
import subprocess
import sys

import numpy as np
from scipy.io import wavfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "dev"))
sys.path.insert(0, str(ROOT / "out/build/windows-msvc-release/native/research"))
sys.path.insert(0, str(ROOT / "plugin/pyruntime"))
from x3_step_null import hn, mono, rms_db  # noqa: E402
from ffi import probe  # noqa: E402
import trench_native_research as core  # noqa: E402

SR = 44100.0
BLOCK = 512
APP = ROOT / "out/build/windows-msvc-release/native/app/trench_native.exe"
RENDER = ROOT / "plugin/build-juce9/TRENCH_RenderNull_artefacts/Release/TRENCH_RenderNull.exe"
RUNTIME = ROOT / "out/build/windows-msvc-release/vcpkg_installed/x64-windows-release"
IDENTITY = ROOT / "plugin/plugin/assets/bodies/identity.body240"
WORK = ROOT / os.environ.get("TRENCH_E2E_OUT", "dev/e2e/tb303.body240")
EXTRA = os.environ.get("TRENCH_E2E_ARGS", "").split()
DRY = pathlib.Path(r"C:\Users\hooki\Downloads\trench_capture\dry_saw_49hz_-12dBFS.wav")
TARGETS = {0: "m0.wav", 1: "m100.wav", 2: "m0q100.wav", 3: "m100q100.wav"}
WAVS = pathlib.Path(r"C:\Users\hooki\Downloads")
VOICE_GAIN = 1.6107
GRID = np.asarray(core.erb_grid_hz())
WEIGHT = np.asarray(core.erb_grid_weight())


def author():
    WORK.parent.mkdir(exist_ok=True)
    shutil.copyfile(IDENTITY, WORK)
    env = dict(**os.environ)
    env["PATH"] = str(RUNTIME / "bin") + ";" + env["PATH"]
    env["QT_PLUGIN_PATH"] = str(RUNTIME / "Qt6/plugins")
    env["QT_QPA_PLATFORM"] = "offscreen"
    for corner, wav in TARGETS.items():
        before = WORK.read_bytes()
        run = subprocess.run([str(APP), "--body", str(WORK), "--corner", str(corner), "--saw",
                              "--target", str(WAVS / wav), "--fit", "--save", str(WORK)] + EXTRA,
                             env=env, capture_output=True, text=True, timeout=600)
        after = WORK.read_bytes()
        words = np.frombuffer(after, dtype="<u2").reshape(4, 6, 5)
        print(f"corner {corner} <- {wav}: app exit {run.returncode}, bytes changed {after != before}, "
              f"rows {[hex(w) for w in words[corner][:, 2]]}")
        assert run.returncode == 0, run.stderr
    return WORK.read_bytes()


def corner_parity(body):
    words = np.frombuffer(body, dtype="<u2").reshape(4, 6, 5)
    w = 2 * np.pi * GRID / SR
    z1 = np.exp(-1j * w)
    z2 = z1 * z1
    worst = 0.0
    for corner, (m, q) in enumerate([(0, 0), (1, 0), (0, 1), (1, 1)]):
        app_db = np.asarray(core.cascade_db([int(v) for v in words[corner].reshape(-1)], list(GRID), SR))
        H = np.ones_like(z1)
        for b0, b1, b2, a1, a2 in probe(body, float(m), float(q), SR):
            H *= (b0 + b1 * z1 + b2 * z2) / (1 + a1 * z1 + a2 * z2)
        plugin_db = 20 * np.log10(np.abs(H) + 1e-30)
        worst = max(worst, float(np.abs(app_db - plugin_db).max()))
    print(f"app core vs plugin core at the four corners: worst {worst:.2e} dB")
    return worst


def engine_render(lib, body, source, morph):
    engine = lib.trench_engine_create()
    try:
        lib.trench_engine_prepare(engine, SR)
        lib.trench_engine_load_body_bytes_at.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_double]
        lib.trench_engine_load_body_bytes_at.restype = ctypes.c_int
        assert lib.trench_engine_load_body_bytes_at(engine, body, len(body), SR) == 0
        lib.trench_engine_set_parameters(engine, float(morph[0]), 0.5, 0.0, 0.0, 1.0)
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
        lib.trench_engine_set_x3_movement(engine, 1)
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
                traj.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), 0.5)
            out[off:end] = left
        return out.astype(np.float64)
    finally:
        lib.trench_engine_destroy(engine)


def plugin_null(body_path):
    lib = hn.bind()
    body = body_path.read_bytes()
    dry = mono(DRY) / 32768.0
    n = len(dry)
    cases = {"morph 0": ("morph=0", np.zeros(n)), "morph 1": ("morph=1", np.ones(n)),
             "ramp 4 s": ("morphRamp=4", np.round(np.minimum(1.0, (np.arange(n) // BLOCK * BLOCK) / SR / 4.0) * 1000.0) / 1000.0)}
    for name, (arg, traj) in cases.items():
        out = ROOT / "dev/e2e" / f"render_{arg.split('=')[0]}.wav"
        subprocess.run([str(RENDER), str(DRY), str(body_path), str(out), "q=0.5", arg], check=True, capture_output=True)
        plugin = mono(out) / VOICE_GAIN
        engine = engine_render(lib, body, dry, traj)
        lo, hi = int(1.0 * SR), n
        null = rms_db(plugin[lo:hi] - engine[lo:hi]) - rms_db(engine[lo:hi])
        print(f"plugin vs engine, {name:8s}: level diff {rms_db(plugin[lo:hi]) - rms_db(engine[lo:hi]):+6.2f} dB, null {null:7.1f} dB, peak {np.abs(plugin).max():.3f}")


if __name__ == "__main__":
    stage = sys.argv[1] if len(sys.argv) > 1 else "all"
    if stage in ("author", "all"):
        body = author()
        corner_parity(body)
    if stage in ("null", "all"):
        plugin_null(pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else WORK)
