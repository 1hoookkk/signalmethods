"""Headless Phaser 2 null gate: real trench_core engine vs independent X3-bank IIR.

The stimulus is generated directly at 48 kHz.  No DAW, resampling, latency
alignment, or fitted gain is involved in the raw null measurement.
"""

import ctypes
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf


ROOT = Path(r"C:\Users\hooki\trench-x3-clean")
HERE = Path(__file__).resolve().parent
DLL = HERE / "cargo_target/release/trench_core.dll"
PRESET = ROOT / "plugin/presets/bodies/X3F_phaser_2.json"
OUT = HERE / "headless_null"
SAMPLE_RATE = 48_000
BLOCK = 512
MORPHS = (0.0, 0.5, 1.0)
Q = 1.0
ABLATIONS = {
    "filter_only": (),
    "agc_only": ("agc",),
    "dc_only": ("dc",),
    "saturation_only": ("saturation",),
    "section_nonlinearity_only": ("section_nonlinearity",),
    "factory_dsp": ("agc", "dc", "saturation", "section_nonlinearity"),
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def decode(word):
    u = int(word) + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    exponent = (u >> 12) & 15
    mantissa = u & 4095
    value = mantissa / 4096.0 if exponent == 0 else (mantissa + 4096.0) / 8192.0
    return value * 2.0 ** (exponent - 15)


def lerp_u16(a, b, fraction):
    # Rust: ((b-a) as f32 * frac) as i32 as i16, then wrapping u16 add.
    delta = int(np.float32(int(b) - int(a)) * np.float32(fraction))
    delta = ((delta + 32768) % 65536) - 32768
    return (int(a) + delta) % 65536


def rows_for(words, active_stages, morph, q):
    corners = np.asarray(words, dtype=np.uint16).reshape(4, active_stages, 5)
    rows = []
    for stage in range(active_stages):
        packed = []
        for column in range(5):
            q0 = lerp_u16(corners[0, stage, column], corners[1, stage, column], morph)
            q1 = lerp_u16(corners[2, stage, column], corners[3, stage, column], morph)
            packed.append(lerp_u16(q0, q1, q))
        d0, d1, d2, d3, d4 = map(decode, packed)
        k0, k1, k2, k3, k4 = 4 * d0 + d1, d1, 4 * d2 + d3, d3, 4 * d4
        rows.append((k4, (k0 - 2) * k4, (1 - k1) * k4, k2 - 2, 1 - k3))
    return rows


def reference_render(source, rows):
    """The same f64 transposed DF-II topology, independently implemented."""
    output = source.astype(np.float32).astype(np.float64)
    for b0, b1, b2, a1, a2 in rows:
        rendered = np.empty(output.shape, dtype=np.float64)
        w1 = 0.0
        w2 = 0.0
        for i, x in enumerate(output):
            y = b0 * x + w1
            w1 = b1 * x - a1 * y + w2
            w2 = b2 * x - a2 * y
            rendered[i] = y
        output = rendered
    # trench_core crosses the ABI as f32 only after the whole cascade.
    return output.astype(np.float32)


def bind():
    lib = ctypes.CDLL(str(DLL))
    lib.trench_engine_create.restype = ctypes.c_void_p
    lib.trench_engine_destroy.argtypes = [ctypes.c_void_p]
    lib.trench_engine_destroy.restype = None
    lib.trench_engine_prepare.argtypes = [ctypes.c_void_p, ctypes.c_double]
    lib.trench_engine_prepare.restype = None
    lib.trench_engine_load_runtime_preset.argtypes = [
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_uint16),
        ctypes.c_size_t,
        ctypes.c_size_t,
        ctypes.c_double,
        ctypes.c_double,
    ]
    lib.trench_engine_load_runtime_preset.restype = ctypes.c_int
    lib.trench_engine_set_parameters.argtypes = [
        ctypes.c_void_p,
        ctypes.c_float,
        ctypes.c_float,
        ctypes.c_float,
        ctypes.c_float,
        ctypes.c_float,
    ]
    lib.trench_engine_set_parameters.restype = None
    lib.trench_engine_process_block.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_float),
        ctypes.POINTER(ctypes.c_float),
        ctypes.c_int,
        ctypes.c_double,
        ctypes.c_double,
    ]
    lib.trench_engine_process_block.restype = None
    for name in (
        "trench_engine_set_agc_enabled",
        "trench_engine_set_dc_block_enabled",
        "trench_engine_set_saturation_enabled",
        "trench_engine_set_nonlinearity_enabled",
        "trench_engine_set_spatial_mode",
        "trench_engine_set_key_snap",
        "trench_engine_set_input_mode",
    ):
        getattr(lib, name).argtypes = [ctypes.c_void_p, ctypes.c_int]
        getattr(lib, name).restype = None
    lib.trench_engine_set_grit.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.trench_engine_set_grit.restype = None
    lib.trench_engine_set_input_preamp.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.trench_engine_set_input_preamp.restype = None
    return lib


def engine_render(lib, source, words, active_stages, morph, enabled=()):
    engine = lib.trench_engine_create()
    if not engine:
        raise RuntimeError("trench_engine_create failed")
    try:
        lib.trench_engine_prepare(engine, float(SAMPLE_RATE))
        packed = (ctypes.c_uint16 * len(words))(*words)
        rc = lib.trench_engine_load_runtime_preset(
            engine,
            b"Phaser 2 headless null",
            packed,
            len(words),
            active_stages,
            float(SAMPLE_RATE),
            1.0,
        )
        if rc != 0:
            raise RuntimeError(f"trench_engine_load_runtime_preset failed: {rc}")

        # Unity amount, zero SPACE, zero preamp/GRIT, no input emulation, KEY OFF.
        lib.trench_engine_set_parameters(engine, morph, Q, 0.0, 0.0, 1.0)
        lib.trench_engine_set_spatial_mode(engine, 2)
        lib.trench_engine_set_key_snap(engine, 0)
        lib.trench_engine_set_input_mode(engine, 0)
        lib.trench_engine_set_grit(engine, 0.0)
        lib.trench_engine_set_input_preamp(engine, 0.0)
        enabled = set(enabled)
        lib.trench_engine_set_agc_enabled(engine, int("agc" in enabled))
        lib.trench_engine_set_dc_block_enabled(engine, int("dc" in enabled))
        lib.trench_engine_set_saturation_enabled(engine, int("saturation" in enabled))
        lib.trench_engine_set_nonlinearity_enabled(engine, int("section_nonlinearity" in enabled))

        result = np.empty(source.shape, dtype=np.float32)
        for offset in range(0, len(source), BLOCK):
            end = min(offset + BLOCK, len(source))
            left = np.ascontiguousarray(source[offset:end], dtype=np.float32)
            right = left.copy()
            lib.trench_engine_process_block(
                engine,
                left.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                right.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                len(left),
                morph,
                Q,
            )
            if not np.array_equal(left, right):
                raise AssertionError("mono input produced unequal channels with spatial OFF")
            result[offset:end] = left
        return result
    finally:
        lib.trench_engine_destroy(engine)


def metrics(observed, reference):
    obs = observed.astype(np.float64)
    ref = reference.astype(np.float64)
    null = obs - ref
    ref_rms = math.sqrt(float(np.mean(ref * ref)))
    null_rms = math.sqrt(float(np.mean(null * null)))
    denominator = float(np.dot(ref, ref))
    gain = float(np.dot(ref, obs) / denominator)
    fitted_null = obs - gain * ref
    fitted_rms = math.sqrt(float(np.mean(fitted_null * fitted_null)))
    return {
        "raw_null_relative_db": 20 * math.log10(max(null_rms, 1e-300) / ref_rms),
        "raw_null_rms_dbfs": 20 * math.log10(max(null_rms, 1e-300)),
        "raw_null_peak_dbfs": 20 * math.log10(max(float(np.max(np.abs(null))), 1e-300)),
        "best_fit_gain_db": 20 * math.log10(abs(gain)),
        "gain_matched_null_relative_db": 20 * math.log10(max(fitted_rms, 1e-300) / ref_rms),
        "correlation": float(np.corrcoef(obs, ref)[0, 1]),
        "sample_exact": bool(np.array_equal(observed, reference)),
    }


def stimulus():
    rng = np.random.default_rng(0x5833)
    source = (rng.standard_normal(SAMPLE_RATE * 3) * 10 ** (-24 / 20)).astype(np.float32)
    return np.clip(source, -0.45, 0.45).astype(np.float32)


def worker(morph, state, destination):
    preset = json.loads(PRESET.read_text())
    words = preset["banks"][str(SAMPLE_RATE)]
    rendered = engine_render(
        bind(), stimulus(), words, int(preset["active_stages"]), morph, ABLATIONS[state]
    )
    np.save(destination, rendered, allow_pickle=False)


def isolated_render(morph, state, destination):
    # Fresh process per state is deliberate.  The lifecycle probe below records
    # why repeated create/destroy calls in one process cannot be trusted yet.
    subprocess.run(
        [sys.executable, str(Path(__file__).resolve()), "--worker", str(morph), state, str(destination)],
        check=True,
    )
    return np.load(destination, allow_pickle=False)


def main():
    preset = json.loads(PRESET.read_text())
    words = preset["banks"][str(SAMPLE_RATE)]
    stages = int(preset["active_stages"])
    # Native-rate, deterministic broadband stimulus at -24 dBFS RMS.
    source = stimulus()
    report = {
        "purpose": "Phaser 2 Q100 headless default/null gate",
        "sample_rate": SAMPLE_RATE,
        "stimulus": "deterministic native-48-kHz broadband noise; no resampling",
        "q": Q,
        "runtime_bank_provenance": preset["provenance"],
        "engine_binary": {"path": str(DLL), "sha256": sha256(DLL)},
        "preset": {"path": str(PRESET), "sha256": sha256(PRESET)},
        "fixed_state": "KEY OFF; amount 100%; boost 1; preamp/GRIT/input/spatial OFF",
        "ablation_states": {state: list(enabled) for state, enabled in ABLATIONS.items()},
        "render_isolation": "Each state was rendered in a fresh process because the repeated-instance FFI lifecycle probe is not stable.",
        "morphs": {},
    }
    OUT.mkdir(exist_ok=True)
    sf.write(str(OUT / "stimulus_48k.wav"), source, SAMPLE_RATE, subtype="FLOAT")
    for morph in MORPHS:
        label = f"M{round(morph * 100)}"
        reference = reference_render(source, rows_for(words, stages, morph, Q))
        renders = {}
        for state in ABLATIONS:
            worker_path = OUT / f".{label}_{state}.npy"
            renders[state] = isolated_render(morph, state, worker_path)
        report["morphs"][label] = {
            f"{state}_engine_vs_x3_bank": metrics(rendered, reference)
            for state, rendered in renders.items()
        }
        sf.write(str(OUT / f"{label}_x3_bank_reference.wav"), reference, SAMPLE_RATE, subtype="FLOAT")
        for state in ("filter_only", "factory_dsp"):
            rendered = renders[state]
            sf.write(str(OUT / f"{label}_{state}_engine.wav"), rendered, SAMPLE_RATE, subtype="FLOAT")
            sf.write(str(OUT / f"{label}_{state}_raw_null.wav"), rendered - reference, SAMPLE_RATE, subtype="FLOAT")

    # Reproduce the independent-instance anomaly without contaminating the
    # authoritative isolated renders above.
    lib = bind()
    probe_reference = reference_render(source, rows_for(words, stages, 0.0, Q))
    report["repeated_instance_ffi_probe"] = []
    for iteration in range(4):
        rendered = engine_render(lib, source, words, stages, 0.0, ABLATIONS["agc_only"])
        report["repeated_instance_ffi_probe"].append(
            {"iteration": iteration + 1, **metrics(rendered, probe_reference)}
        )
    report_path = HERE / "headless_null_report.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    if len(sys.argv) == 5 and sys.argv[1] == "--worker":
        worker(float(sys.argv[2]), sys.argv[3], Path(sys.argv[4]))
    else:
        main()
