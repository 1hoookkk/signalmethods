from __future__ import annotations

import ctypes
import sys
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "bodies" / "candidates"
FS = 48_000.0

KLATT = {
    "IY": (310, 2020, 2960), "IH": (400, 1800, 2570), "EH": (530, 1680, 2500),
    "AE": (620, 1660, 2430), "AA": (700, 1220, 2600), "AO": (600,  990, 2570),
    "UH": (450, 1030, 2380), "UW": (350,  870, 2240), "ER": (490, 1350, 1690),
}
BW_Q0, BW_Q100 = 150.0, 15.0

lib = ctypes.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))

try:
    lib.trench_num_stages.restype = ctypes.c_uint32
    lib.trench_num_coeffs.restype = ctypes.c_uint32
    RT_DOUBLES = int(lib.trench_num_stages()) * int(lib.trench_num_coeffs())
except AttributeError:
    RT_DOUBLES = 30
lib.trench_stage_words_from_roots_at.argtypes = [
    ctypes.POINTER(ctypes.c_double), ctypes.c_double, ctypes.POINTER(ctypes.c_uint16)]
lib.trench_stage_words_from_roots_at.restype = ctypes.c_int
lib.trench_pack_body_from_corner_words.argtypes = [
    ctypes.POINTER(ctypes.c_uint16), ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint8)]
lib.trench_pack_body_from_corner_words.restype = ctypes.c_int
lib.trench_certify_body.argtypes = [
    ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.c_uint, ctypes.c_double,
    ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double)]
lib.trench_certify_body.restype = None
lib.trench_packed_probe_at.argtypes = [
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double, ctypes.c_double,
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int

GRID = np.geomspace(20, 20_000, 800)
_Z1 = np.exp(-1j * 2 * np.pi * GRID / FS)
_Z2 = _Z1 * _Z1

def stage_words(pHz, pR, zHz, zR, scale=1.0):
    roots = (ctypes.c_double * 5)(pHz, pR, zHz, zR, scale)
    out = (ctypes.c_uint16 * 5)()
    if lib.trench_stage_words_from_roots_at(roots, FS, out) != 0:
        raise RuntimeError(f"encoder refused {pHz=} {pR=} {zHz=} {zR=}")
    return list(out)

IDENT = stage_words(0.0, 0.0, 0.0, 0.0, 1.0)

def r_of_bw(bw_hz):
    return float(np.exp(-np.pi * bw_hz / FS))

def frame_rows(corner):
    m100, q100 = corner in (1, 3), corner >= 2
    s1 = (9800.0, 0.9985 if q100 else 0.968,
          1600.0 if m100 else 330.0, 0.94)
    s6 = (1900.0 if m100 else 250.0, 0.9985 if q100 else 0.992,
          18500.0 if m100 else 8000.0, 1.0)
    return s1, s6

def corner_rows(A, B, corner, frame=False):
    m100, q100 = corner in (1, 3), corner >= 2
    formants = KLATT[B] if m100 else KLATT[A]
    pR = r_of_bw(BW_Q100 if q100 else BW_Q0)
    zR = r_of_bw(600.0)
    if frame:
        s1, s6 = frame_rows(corner)
        rows = [s1]
        for i, f in enumerate(formants):
            fpR = 0.995 if q100 else (0.985 if i == 0 else 0.975)
            fzR = 0.90 if i == 0 else 0.93
            rows.append((float(f), fpR, float(f) * 1.45, fzR))
        rows.append((5100.0 if m100 else 4800.0, 0.985 if q100 else 0.965,
                     7300.0 if m100 else 6900.0, 0.90))
        rows.append(s6)
        return rows
    rows = [(80.0, 0.95, 80.0, 0.90)]
    rows += [(float(f), pR, float(f), zR) for f in formants]
    rows += [None, None]
    return rows

def response_db(rows, scale=1.0):
    h = np.ones_like(_Z1)
    for r in rows:
        if r is None:
            continue
        pHz, pR, zHz, zR = r
        a1, a2 = -2 * pR * np.cos(2 * np.pi * pHz / FS), pR * pR
        b1, b2 = -2 * zR * np.cos(2 * np.pi * zHz / FS), zR * zR
        h *= scale * (1 + b1 * _Z1 + b2 * _Z2) / (1 + a1 * _Z1 + a2 * _Z2)
    return 20 * np.log10(np.abs(h) + 1e-12)

def probe_db(body, m, q):
    buf = ctypes.create_string_buffer(bytes(body), 240)
    bq = (ctypes.c_double * RT_DOUBLES)()
    mr = (ctypes.c_double * 1)()
    um = (ctypes.c_uint32 * 1)()
    nf = (ctypes.c_uint32 * 1)()
    assert lib.trench_packed_probe_at(buf, 240, m, q, FS, bq, mr, um, nf) == 0
    h = np.ones_like(_Z1)
    for s in range(6):
        b0, b1, b2, a1, a2 = bq[s * 5:s * 5 + 5]
        h *= (b0 + b1 * _Z1 + b2 * _Z2) / (1.0 + a1 * _Z1 + a2 * _Z2)
    return 20 * np.log10(np.abs(h) + 1e-12)

def zone_check(rows_by_corner, allow_cross=()):
    n = len(rows_by_corner[0])
    for i in range(n):
        for j in range(i + 1, n):
            signs = set()
            for rows in rows_by_corner:
                a, b = rows[i], rows[j]
                if a is None or b is None:
                    continue
                signs.add(a[0] < b[0])
            declared = (i, j) in allow_cross or (j, i) in allow_cross
            if len(signs) > 1 and not declared:
                raise RuntimeError(
                    f"ZONING: lanes {i+1} and {j+1} cross between corners - "
                    "declare them a traveller pair or separate their zones")
            if declared and len(signs) == 1:
                raise RuntimeError(
                    f"ZONING: lanes {i+1}/{j+1} declared travellers but never cross")

def stage_dbs(body, m, q):
    buf = ctypes.create_string_buffer(bytes(body), 240)
    bq = (ctypes.c_double * RT_DOUBLES)()
    mr = (ctypes.c_double * 1)()
    um = (ctypes.c_uint32 * 1)()
    nf = (ctypes.c_uint32 * 1)()
    assert lib.trench_packed_probe_at(buf, 240, m, q, FS, bq, mr, um, nf) == 0
    out = []
    for s in range(6):
        b0, b1, b2, a1, a2 = bq[s * 5:s * 5 + 5]
        h = (b0 + b1 * _Z1 + b2 * _Z2) / (1.0 + a1 * _Z1 + a2 * _Z2)
        out.append(20 * np.log10(np.abs(h) + 1e-12))
    return out

def default_plot(body, name, subtitle=""):
    import subprocess
    body_file = OUT / f"{name}.body240"
    subprocess.run([sys.executable, str(ROOT / "tools" / "inspect_body.py"),
                    str(body_file), str(FS)], check=True,
                   capture_output=True, text=True)
    legacy = OUT / f"{name}.png"
    if legacy.exists():
        legacy.unlink()

def make_vowel_morph(vowel_A, vowel_B, frame=False):
    A, B = vowel_A.upper(), vowel_B.upper()
    by_corner = [corner_rows(A, B, c, frame) for c in range(4)]
    allow = []
    if frame:
        for i in (1, 2, 3):
            orders = {by_corner[c][i][0] < by_corner[c][5][0] for c in range(4)
                      if by_corner[c][i] and by_corner[c][5]}
            if len(orders) > 1:
                allow.append((i, 5))
    zone_check(by_corner, allow_cross=tuple(allow))
    words = []
    for c in range(4):
        rows = corner_rows(A, B, c, frame)
        active = sum(1 for r in rows if r)
        peak = response_db(rows).max()
        sc = 10 ** (-peak / 20 / active)
        for r in rows:
            words += IDENT if r is None else stage_words(*r, sc)
    wbuf = (ctypes.c_uint16 * 120)(*words)
    body = (ctypes.c_uint8 * 240)()
    if lib.trench_pack_body_from_corner_words(wbuf, 120, body) != 0:
        raise RuntimeError("packer refused")
    p = ctypes.c_int(); mr = ctypes.c_double(); fm = ctypes.c_double(); fq = ctypes.c_double()
    lib.trench_certify_body(body, 240, 17, 1.0, ctypes.byref(p), ctypes.byref(mr),
                            ctypes.byref(fm), ctypes.byref(fq))
    if p.value != 1:
        raise RuntimeError(f"CERT FAIL at morph {fm.value:.2f} q {fq.value:.2f} (pole {mr.value:.4f})")

    name = f"VOWEL_{A}_to_{B}" + ("_framed" if frame else "")
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / f"{name}.body240").write_bytes(bytes(body))
    default_plot(body, name, f"{A} -> {B}")
    print(f"{name}: CERT PASS  hottest pole {mr.value:.4f}")
    return OUT / f"{name}.body240"

if __name__ == "__main__":
    args = [a.upper() for a in sys.argv[1:]]
    pairs = list(zip(args[::2], args[1::2])) if args else \
            [("IY", "AO"), ("IY", "AA"), ("UW", "AE"), ("IY", "ER")]
    for a, b in pairs:
        make_vowel_morph(a, b)
