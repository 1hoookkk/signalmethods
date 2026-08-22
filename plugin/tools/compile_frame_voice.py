from __future__ import annotations
import ctypes as C, struct, json, os, sys
import numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from pyruntime.packed_interp import coeffs_to_words

SR = 39062.5
PAD = (0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000)
CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
DLL = os.path.join(ROOT, "target", "release", "trench_core.dll")
_lib = C.CDLL(DLL)
try:
    _lib.trench_num_stages.restype = C.c_uint32
    _lib.trench_num_coeffs.restype = C.c_uint32
    RT_DOUBLES = int(_lib.trench_num_stages()) * int(_lib.trench_num_coeffs())
except AttributeError:
    RT_DOUBLES = 30
_lib.trench_packed_probe.restype = C.c_int32
_lib.trench_packed_probe.argtypes = [C.c_char_p, C.c_size_t, C.c_double, C.c_double,
    C.POINTER(C.c_double), C.POINTER(C.c_double), C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)]

def biquad_to_kernel(b0, b1, b2, a1, a2):
    return (b1 / b0 + 2.0, 1.0 - b2 / b0, a1 + 2.0, 1.0 - a2, b0)

def biquad(fc, pole_r, zero_off_oct, zero_r, gain=1.0):
    wp = 2 * np.pi * fc / SR
    wz = 2 * np.pi * (fc * 2.0 ** zero_off_oct) / SR
    return (gain, -2 * zero_r * np.cos(wz) * gain, zero_r ** 2 * gain,
            -2 * pole_r * np.cos(wp), pole_r ** 2)

def stage_words(role, p):
    fc = p.get("fc", 1000.0); r = p.get("r", 0.97); zoff = p.get("zoff", 0.0)
    if role == "s5" or role == "notch":
        zr = 1.0 if role == "s5" else p.get("zero_r", 0.97)
        bq = biquad(fc, r, zoff, zr)
    elif role == "body":
        bq = biquad(fc, r, p.get("zoff", 1.4), 1.0)
    elif role == "air":
        bq = biquad(fc, r, p.get("zoff", 0.4), 0.85)
    else:
        bq = biquad(fc, r, zoff, p.get("zero_r", 0.9))
    return coeffs_to_words(*biquad_to_kernel(*bq))

def compile_recipe(recipe):
    stages = recipe["stages"]
    assert len(stages) == 6, "biquad budget is 6"
    words = {c: [] for c in CORNERS}
    for si, st in enumerate(stages):
        role = st["role"]
        if si == 5 and role != "s5":
            role = "s5"
        for c in CORNERS:
            if role == "sentinel":
                words[c].append(PAD)
            else:
                p = st.get(c) or st.get("all") or {}
                words[c].append(tuple(stage_words(role, p)))
    body = bytearray()
    for c in CORNERS:
        for row in words[c]:
            for w in row:
                body += struct.pack("<H", int(w) & 0xFFFF)
    return bytes(body)

def engine_check(body):
    worst = 0.0; unst = 0; nonf = 0
    for m, q in [(0., 0.), (1., 0.), (0., 1.), (1., 1.), (0.5, 0.5)]:
        out = (C.c_double * RT_DOUBLES)(); mr = C.c_double(); um = C.c_uint32(); nm = C.c_uint32()
        rc = _lib.trench_packed_probe(body, 240, C.c_double(m), C.c_double(q), out,
                                      C.byref(mr), C.byref(um), C.byref(nm))
        assert rc == 0, rc
        worst = max(worst, mr.value); unst |= um.value; nonf |= nm.value
    return worst, unst, nonf

def main():
    recipe = json.loads(open(sys.argv[1]).read())
    body = compile_recipe(recipe)
    worst, unst, nonf = engine_check(body)
    ok = worst < 1.0 and nonf == 0
    out_dir = os.path.join(ROOT, "plugin", "presets", "bodies")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, f"{recipe['name']}.body240")
    if ok:
        open(out, "wb").write(body)
    print(f"{recipe['name']}: max_pole_r={worst:.3f} unstable_mask={unst:04b} nonfinite={nonf}")
    print("WROTE " + out if ok else "REJECTED (unstable/nonfinite) — not written")

if __name__ == "__main__":
    main()
