import argparse
import ctypes
import glob
import json
import os
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PRESETS = os.path.join(ROOT, "plugin", "presets", "bodies", "X3F_*.json")
DEFAULT_DLL = os.path.join(ROOT, "target", "release", "trench_core.dll")
BLOCK = 512
NSAMP = 2048
MORPHS = (0.0, 0.25, 0.5, 0.75, 1.0)
QS = (0.0, 1.0)


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
    return output.astype(np.float32)


def bind(path):
    lib = ctypes.CDLL(path)
    lib.trench_engine_create.restype = ctypes.c_void_p
    lib.trench_engine_destroy.argtypes = [ctypes.c_void_p]
    lib.trench_engine_prepare.argtypes = [ctypes.c_void_p, ctypes.c_double]
    lib.trench_engine_load_runtime_preset.argtypes = [
        ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint16),
        ctypes.c_size_t, ctypes.c_size_t, ctypes.c_double, ctypes.c_double]
    lib.trench_engine_load_runtime_preset.restype = ctypes.c_int
    for name in ("set_agc_enabled", "set_dc_block_enabled", "set_saturation_enabled",
                 "set_nonlinearity_enabled", "set_key_snap", "set_input_mode",
                 "set_spatial_mode"):
        getattr(lib, "trench_engine_" + name).argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.trench_engine_set_parameters.argtypes = [
        ctypes.c_void_p, ctypes.c_float, ctypes.c_float, ctypes.c_float,
        ctypes.c_float, ctypes.c_float]
    lib.trench_engine_set_agc_drive.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.trench_engine_set_grit.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.trench_engine_set_input_preamp.argtypes = [ctypes.c_void_p, ctypes.c_float]
    lib.trench_engine_process_block.argtypes = [
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
        ctypes.c_int, ctypes.c_double, ctypes.c_double]
    return lib


def engine_render(lib, source, words, active_stages, rate, morph, q, agc=False, drive=2.0):
    engine = lib.trench_engine_create()
    if not engine:
        raise RuntimeError("trench_engine_create failed")
    try:
        lib.trench_engine_prepare(engine, float(rate))
        packed = (ctypes.c_uint16 * len(words))(*words)
        rc = lib.trench_engine_load_runtime_preset(
            engine, b"x3 null gate", packed, len(words), active_stages,
            float(rate), 1.0)
        if rc != 0:
            raise RuntimeError(f"load_runtime_preset rc={rc}")
        lib.trench_engine_set_parameters(engine, morph, q, 0.0, 0.0, 1.0)
        lib.trench_engine_set_spatial_mode(engine, 2)
        lib.trench_engine_set_key_snap(engine, 0)
        lib.trench_engine_set_input_mode(engine, 0)
        lib.trench_engine_set_grit(engine, 0.0)
        lib.trench_engine_set_input_preamp(engine, 0.0)
        lib.trench_engine_set_agc_enabled(engine, 1 if agc else 0)
        lib.trench_engine_set_agc_drive(engine, drive)
        lib.trench_engine_set_dc_block_enabled(engine, 0)
        lib.trench_engine_set_saturation_enabled(engine, 0)
        lib.trench_engine_set_nonlinearity_enabled(engine, 0)
        out = np.empty(source.shape, dtype=np.float32)
        for offset in range(0, len(source), BLOCK):
            end = min(offset + BLOCK, len(source))
            left = np.array(source[offset:end], dtype=np.float32, copy=True)
            right = left.copy()
            lib.trench_engine_process_block(
                engine, left.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                right.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                len(left), morph, q)
            if not np.array_equal(left, right):
                raise AssertionError("spatial OFF produced unequal channels")
            out[offset:end] = left
        return out
    finally:
        lib.trench_engine_destroy(engine)


def raw_null_db(observed, reference):
    obs = observed.astype(np.float64)
    ref = reference.astype(np.float64)
    residual = obs - ref
    if not np.any(residual):
        return None
    denom = float(np.sqrt(np.mean(ref ** 2)))
    num = float(np.sqrt(np.mean(residual ** 2)))
    if denom <= 0.0:
        return 0.0
    return 20.0 * np.log10(max(num, 1e-300) / denom)


def stimulus(n):
    rng = np.random.default_rng(20260813)
    x = rng.standard_normal(n) * 0.25
    return x.astype(np.float32)


def render_one(dll, path, rate_key, morph, q):
    d = json.load(open(path))
    stages = int(d["active_stages"])
    words = [int(w) for w in d["banks"][rate_key]]
    lib = bind(dll)
    source = stimulus(NSAMP)
    rendered = engine_render(lib, source, words, stages, float(rate_key), morph, q)
    reference = reference_render(source, rows_for(words, stages, morph, q))
    return raw_null_db(rendered, reference)


def isolated_null(dll, path, rate_key, morph, q):
    import subprocess
    r = subprocess.run(
        [sys.executable, os.path.abspath(__file__), "--one",
         "--dll", dll, "--preset", path, "--rate", rate_key,
         "--morph", repr(morph), "--q", repr(q)],
        capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"isolated render failed: {r.stderr.strip()[:200]}")
    out = r.stdout.strip().splitlines()[-1]
    return None if out == "exact" else float(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--json", default="")
    ap.add_argument("--one", action="store_true")
    ap.add_argument("--preset", default="")
    ap.add_argument("--rate", default="")
    ap.add_argument("--morph", type=float, default=0.0)
    ap.add_argument("--q", type=float, default=0.0)
    ap.add_argument("--in-process", action="store_true")
    ap.add_argument("--agc", action="store_true")
    ap.add_argument("--agc-drive", type=float, default=1.0)
    args = ap.parse_args()

    if args.one:
        db = render_one(args.dll, args.preset, args.rate, args.morph, args.q)
        print("exact" if db is None else repr(float(db)))
        return 0

    lib = bind(args.dll)
    source = stimulus(NSAMP)
    presets = sorted(glob.glob(PRESETS))
    if not presets:
        print("no X3F presets found")
        return 2

    failures = []
    checked = 0
    report = []
    print(f"X3 NULL GATE  raw null, no fitted gain, KEY OFF, filter-only, unity boost")
    print(f"  dll     {args.dll}")
    print(f"  stimulus {NSAMP} samples, deterministic\n")
    print(f"{'preset':<26} {'rate':>7} {'states':>7} {'worst raw null':>16}")
    for path in presets:
        d = json.load(open(path))
        stages = int(d["active_stages"])
        name = os.path.basename(path)
        for rate_key, words in sorted(d["banks"].items(), key=lambda kv: float(kv[0])):
            rate = float(rate_key)
            words = [int(w) for w in words]
            worst = None
            states = 0
            for q in QS:
                for morph in MORPHS:
                    if args.in_process:
                        rendered = engine_render(lib, source, words, stages, rate, morph, q, args.agc, args.agc_drive)
                        reference = reference_render(source, rows_for(words, stages, morph, q))
                        db = raw_null_db(rendered, reference)
                    else:
                        db = isolated_null(args.dll, path, rate_key, morph, q)
                    states += 1
                    checked += 1
                    if db is not None:
                        if worst is None or db > worst:
                            worst = db
                        failures.append((name, rate_key, morph, q, db))
                    report.append({"preset": name, "rate": rate_key, "morph": morph,
                                   "q": q, "sample_exact": db is None,
                                   "raw_null_relative_db": db})
            label = "sample-exact" if worst is None else f"{worst:+.2f} dB"
            print(f"{name:<26} {rate_key:>7} {states:>7} {label:>16}")

    print(f"\n{checked} states checked, {len(failures)} not sample-exact")
    for name, rate, morph, q, db in failures[:20]:
        print(f"  FAIL {name} bank {rate} morph {morph} q {q}: raw null {db:+.2f} dB")
    if args.json:
        with open(args.json, "w") as f:
            json.dump({"checked": checked, "failures": len(failures),
                       "states": report}, f, indent=1)
        print(f"wrote {args.json}")
    print("\nPASS" if not failures else "\nFAIL")
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
