from __future__ import annotations

import ctypes
import sys
from pathlib import Path

import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "evidence" / "ship_rail"
CLIPS = OUT / "clips"
DATUM = 48_000.0
GRID = np.geomspace(50.0, 16_000.0, 300)
_Z1 = np.exp(-1j * 2.0 * np.pi * GRID / DATUM)
_Z2 = _Z1 * _Z1

lib = ctypes.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))

try:
    lib.trench_num_stages.restype = ctypes.c_uint32
    lib.trench_num_coeffs.restype = ctypes.c_uint32
    RT_DOUBLES = int(lib.trench_num_stages()) * int(lib.trench_num_coeffs())
except AttributeError:
    RT_DOUBLES = 30
lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int
lib.trench_engine_create.restype = ctypes.c_void_p
lib.trench_engine_destroy.argtypes = [ctypes.c_void_p]
lib.trench_engine_prepare.argtypes = [ctypes.c_void_p, ctypes.c_double]
lib.trench_engine_load_body_bytes_at.argtypes = [
    ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double]
lib.trench_engine_load_body_bytes_at.restype = ctypes.c_int
lib.trench_engine_set_input_mode.argtypes = [ctypes.c_void_p, ctypes.c_uint]
lib.trench_engine_set_spatial_mode.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.trench_engine_set_agc_enabled.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.trench_engine_set_saturation_enabled.argtypes = [ctypes.c_void_p, ctypes.c_int]
lib.trench_engine_set_parameters.argtypes = [ctypes.c_void_p] + [ctypes.c_float] * 5
lib.trench_engine_process_block.argtypes = [
    ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_double,
    ctypes.c_double]

def probe(buf, m: float, q: float):
    bq = (ctypes.c_double * RT_DOUBLES)()
    mr = (ctypes.c_double * 1)()
    um = (ctypes.c_uint32 * 1)()
    nf = (ctypes.c_uint32 * 1)()
    rc = lib.trench_packed_probe_at(buf, 240, m, q, DATUM, bq, mr, um, nf)
    if rc != 0:
        return None, 2.0, 1
    h = np.ones_like(_Z1)
    for s in range(6):
        b0, b1, b2, a1, a2 = bq[s * 5:s * 5 + 5]
        h = h * (b0 + b1 * _Z1 + b2 * _Z2) / (1.0 + a1 * _Z1 + a2 * _Z2)
    db = 20.0 * np.log10(np.abs(h) + 1e-12)
    return db - db.mean(), float(mr[0]), int(um[0]) | int(nf[0])

def score(path: Path):
    raw = path.read_bytes()
    if len(raw) != 240:
        return None
    buf = ctypes.create_string_buffer(raw, 240)
    grid = {}
    for m in (0.0, 0.25, 0.5, 0.75, 1.0):
        for q in (0.0, 0.5, 1.0):
            db, max_r, bad = probe(buf, m, q)
            if db is None or bad or max_r >= 1.0:
                return {"name": path.stem, "cut": "unstable/non-finite on the wheel grid"}
            grid[(m, q)] = db
    rms = lambda d: float(np.sqrt((d * d).mean()))
    travel = rms(grid[(1.0, 0.0)] - grid[(0.0, 0.0)])
    interior = rms(grid[(0.5, 0.0)] - 0.5 * (grid[(0.0, 0.0)] + grid[(1.0, 0.0)]))
    q_width = max(rms(grid[(0.0, 1.0)] - grid[(0.0, 0.0)]),
                  rms(grid[(1.0, 1.0)] - grid[(1.0, 0.0)]))
    contrast = float(max(np.ptp(grid[(m, q)]) for (m, q) in grid))
    sig = np.concatenate([grid[(0.0, 0.0)], grid[(0.5, 0.0)], grid[(1.0, 0.0)]])
    return {"name": path.stem, "cut": None, "travel": travel, "interior": interior,
            "q_width": q_width, "contrast": contrast, "sig": sig, "raw": raw}

def render_clip(raw: bytes, out_path: Path):
    n = int(0.40 * DATUM)
    t = np.arange(n) / DATUM
    env = np.minimum(1.0, t / 0.01) * np.exp(-t / 0.36)
    notes = [0, 3, 7, 12, 7, 3, 0, -5, 0, 3, 7, 12, 15, 12, 7, 3, 0, -5, -12, 0]
    saw = np.concatenate([
        (2.0 * ((t * 65.406 * 2.0 ** (k / 12.0)) % 1.0) - 1.0) * 0.5 * env
        for k in notes]).astype(np.float32)
    morphs = np.linspace(0.0, 1.0, len(saw))
    eng = lib.trench_engine_create()
    lib.trench_engine_prepare(eng, DATUM)
    buf = ctypes.create_string_buffer(raw, 240)
    assert lib.trench_engine_load_body_bytes_at(eng, buf, 240, DATUM) == 0
    lib.trench_engine_set_input_mode(eng, 0)
    lib.trench_engine_set_spatial_mode(eng, 2)
    lib.trench_engine_set_agc_enabled(eng, 1)
    lib.trench_engine_set_saturation_enabled(eng, 0)
    out = np.zeros_like(saw)
    for start in range(0, len(saw), 512):
        seg = np.ascontiguousarray(saw[start:start + 512])
        r = seg.copy()
        m = float(morphs[start])
        lib.trench_engine_set_parameters(eng, m, 0.0, 0.0, 0.0, 1.0)
        lib.trench_engine_process_block(
            eng, seg.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            r.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
            len(seg), m, 0.0)
        out[start:start + len(seg)] = seg
    lib.trench_engine_destroy(eng)
    peak = float(np.abs(out).max())
    if peak > 0.891:
        out = out * (0.891 / peak)
    wavfile.write(out_path, int(DATUM), (np.clip(out, -1, 1) * 32767).astype(np.int16))

def group_of(name: str) -> str:
    n = name.lower()
    if n.startswith("x3_") and any(k in n for k in
        ("lowpass", "highpass", "bandpass", "swept_eq", "contrary")):
        return "workhorse"
    if n.startswith("x3_"):
        return "colour"
    if any(k in n for k in ("vox", "vowel", "vocal", "mouth", "ah", "oo", "ee")):
        return "voice"
    if n.startswith(("cavl", "vowl")):
        return "voice"
    return "character"

def main():
    CLIPS.mkdir(parents=True, exist_ok=True)
    paths = sorted((ROOT / "bodies" / "candidates").glob("*.body240"))
    print(f"{len(paths)} candidates")
    scored, cuts = [], []
    for p in paths:
        s = score(p)
        if s is None:
            continue
        (cuts if s["cut"] else scored).append(s)

    alive = []
    for s in scored:
        if s["travel"] < 1.5:
            s["cut"] = f"dead travel ({s['travel']:.1f} dB rms M0->M100)"
            cuts.append(s)
        else:
            alive.append(s)

    names = {s["name"] for s in alive}
    stayed = []
    for s in alive:
        if not s["name"].endswith("_ride") and s["name"] + "_ride" in names:
            s["cut"] = f"superseded by {s['name']}_ride (same capture, fitted interior)"
            cuts.append(s)
        else:
            stayed.append(s)
    alive = stayed

    alive.sort(key=lambda s: -s["travel"])
    kept = []
    for s in alive:
        dup = None
        for k in kept:
            d = float(np.sqrt(np.mean((s["sig"] - k["sig"]) ** 2)))
            if d < 1.5:
                dup = k
                break
        if dup:
            s["cut"] = f"near-duplicate of {dup['name']} ({d:.1f} dB rms apart)"
            cuts.append(s)
        else:
            kept.append(s)

    print(f"{len(kept)} kept, {len(cuts)} cut; rendering clips...")
    groups: dict[str, list] = {}
    for s in kept:
        groups.setdefault(group_of(s["name"]), []).append(s)
    for g, members in groups.items():
        members.sort(key=lambda s: -(s["interior"] + s["travel"]))
        for s in members:
            render_clip(s["raw"], CLIPS / f"{g}__{s['name']}.wav")

    lines = ["# PROPOSED SHIP RAIL (measured staging - ears are the gate)", ""]
    order = ["workhorse", "character", "voice", "colour"]
    for g in order + [x for x in groups if x not in order]:
        if g not in groups:
            continue
        lines.append(f"## {g.upper()} ({len(groups[g])})")
        for s in groups[g]:
            lines.append(
                f"- **{s['name']}** — travel {s['travel']:.1f} dB, interior"
                f" {s['interior']:.1f} dB, Q {s['q_width']:.1f} dB,"
                f" contrast {s['contrast']:.0f} dB")
        lines.append("")
    lines.append(f"## CUT ({len(cuts)}) — measured, not tasted")
    for s in sorted(cuts, key=lambda s: s["name"]):
        lines.append(f"- {s['name']}: {s['cut']}")
    (OUT / "RAIL.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"RAIL.md + {sum(len(m) for m in groups.values())} clips -> {OUT}")

if __name__ == "__main__":
    main()
