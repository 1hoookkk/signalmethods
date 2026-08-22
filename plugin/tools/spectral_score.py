from __future__ import annotations

import ctypes
import json
import math
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pyruntime"))
from arma_measure_lib import lib, pack_and_certify, RT_DOUBLES  # noqa: E402

lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int

CORNERS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]
UNIT_ZERO_RP_DB = 96.0

def r_of(db: float) -> float:
    return 1.0 - 10.0 ** (-db / 20.0)

def corner_roots(v: dict, corner: str) -> list[float]:
    key = corner.lower().replace("_", "")
    if key in {k.lower() for k in v}:
        for k in v:
            if k.lower() == key:
                o = v[k]
                return [float(o["pole_hz"]), r_of(float(o["pole_rp_db"])),
                        float(o["zero_hz"]), r_of(float(o["zero_rp_db"]))]
    base = v["m0q0"]
    pole_hz = float(base["pole_hz"]); pole_db = float(base["pole_rp_db"])
    zero_hz = float(base["zero_hz"]); zero_db = float(base["zero_rp_db"])
    at_m100 = corner in ("M100_Q0", "M100_Q100")
    at_q100 = corner in ("M0_Q100", "M100_Q100")
    if at_m100:
        mo = v.get("morph", {})
        st = float(mo.get("interval_st", 0.0))
        if mo.get("motion") == "falling" and st > 0:
            st = -st
        if mo.get("motion") == "parked":
            st = 0.0
        ratio = 2.0 ** (st / 12.0)
        pole_hz *= ratio
        zero_hz *= ratio
        pole_db += float(mo.get("rp_delta_db", 0.0))
    if at_q100:
        qo = v.get("q", {})
        d = float(qo.get("rp_delta_db", 12.0 if qo.get("motion") == "tighten"
                         else -6.0 if qo.get("motion") == "widen" else 0.0))
        if "rp_delta_db" in qo:
            d = float(qo["rp_delta_db"])
        if qo.get("motion") == "parked":
            d = 0.0
        pole_db += d
        ratio = 2.0 ** (float(qo.get("interval_st", 0.0)) / 12.0)
        pole_hz *= ratio
        zero_hz *= ratio
    return [pole_hz, r_of(pole_db), zero_hz, r_of(zero_db)]

def main():
    score = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    rate = float(score.get("rate_hz", 48_000.0))
    name = score["name"]
    out = Path(sys.argv[2]) if len(sys.argv) > 2 \
        else ROOT / "bodies" / "candidates" / f"SCORE_{name}.body240"

    voices = sorted(score["voices"], key=lambda v: int(v["slot"]))
    assert [int(v["slot"]) for v in voices] == [1, 2, 3, 4, 5, 6], \
        "a score names exactly the six voices"

    corner_words: list[int] = []
    provenance = {"score": score, "corners": {}}
    for corner in CORNERS:
        level_db = float(score.get("corner_level_db", {}).get(corner, 0.0))
        scale = 10.0 ** (level_db / 20.0 / 6.0)
        rows = []
        for v in voices:
            r = corner_roots(v, corner)
            if v.get("terminal", int(v["slot"]) == 6):
                if "zero_rp_db" not in v.get("m0q0", {}) or \
                        v.get("force_terminal", True):
                    r[3] = r_of(UNIT_ZERO_RP_DB)
            rows.append(r + [scale])
        provenance["corners"][corner] = rows
        for p in rows:
            row = (ctypes.c_uint16 * 5)()
            rc = lib.trench_stage_words_from_roots_at(
                (ctypes.c_double * 5)(*p), rate, row)
            assert rc == 0, f"encoder refused {corner} {p}"
            corner_words.extend(row)

    body, max_r = pack_and_certify(corner_words)
    out.write_bytes(body)
    Path(str(out).replace(".body240", ".score.json")).write_text(
        json.dumps(provenance, indent=1), encoding="utf-8")
    print(f"{out.name}: certified, hottest pole {max_r:.4f}")

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    grid = np.geomspace(20.0, 20_000.0, 700)
    z1 = np.exp(-1j * 2.0 * np.pi * grid / rate)
    z2 = z1 * z1

    def response(m, q):
        c = (ctypes.c_double * RT_DOUBLES)()
        mr = ctypes.c_double(); un = ctypes.c_uint32(); nf = ctypes.c_uint32()
        buf = ctypes.create_string_buffer(body, 240)
        assert lib.trench_packed_probe_at(buf, 240, m, q, rate, c,
                                          ctypes.byref(mr), ctypes.byref(un),
                                          ctypes.byref(nf)) == 0
        cc = np.ctypeslib.as_array(c).reshape(6, 5)
        db = np.zeros_like(grid)
        for b0, b1, b2, a1, a2 in cc:
            db += 20.0 * np.log10(np.maximum(np.abs(
                (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2)), 1e-12))
        return db

    fig, axes = plt.subplots(1, 2, figsize=(14, 5), sharey=True)
    cmap = plt.get_cmap("YlOrRd_r")
    for j, m in enumerate(np.linspace(0, 1, 11)):
        axes[0].semilogx(grid, response(float(m), 0.0), color=cmap(j / 10), lw=1.1)
        axes[1].semilogx(grid, response(0.5, float(m)), color=cmap(j / 10), lw=1.1)
    axes[0].set_title(f"{name} - riding MORPH at Q0")
    axes[1].set_title("riding Q at M50")
    for ax in axes:
        ax.set_ylim(-60, 30)
        ax.set_xlim(20, 20000)
        ax.grid(alpha=0.25)
    fig.tight_layout()
    png = Path(str(out).replace(".body240", "_score_sheet.png"))
    fig.savefig(png, dpi=110)
    print(f"sheet: {png}")

if __name__ == "__main__":
    main()
