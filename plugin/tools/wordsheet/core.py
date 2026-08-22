from __future__ import annotations

import ctypes
import itertools
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

FROZEN = getattr(sys, "frozen", False)
ROOT = Path(getattr(sys, "_MEIPASS", "")) if FROZEN \
    else Path(__file__).resolve().parents[2]
OUT_ROOT = Path(sys.executable).resolve().parent if FROZEN else ROOT

import numpy as np  # noqa: E402

_dll = next((p for p in (ROOT / "trench_core.dll",
                         ROOT / "target" / "release" / "trench_core.dll")
             if p.exists()), None)
if _dll is None:
    raise SystemExit("trench_core.dll not found")
lib = ctypes.CDLL(str(_dll))

try:
    lib.trench_num_stages.restype = ctypes.c_uint32
    lib.trench_num_coeffs.restype = ctypes.c_uint32
    RT_DOUBLES = int(lib.trench_num_stages()) * int(lib.trench_num_coeffs())
except AttributeError:
    RT_DOUBLES = 30

lib.trench_packed_decode.argtypes = [ctypes.c_uint16]
lib.trench_packed_decode.restype = ctypes.c_double
lib.trench_packed_encode.argtypes = [ctypes.c_double]
lib.trench_packed_encode.restype = ctypes.c_uint16
lib.trench_packed_probe_at.argtypes = [
    ctypes.c_char_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.POINTER(ctypes.c_double),
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(ctypes.c_uint32)]
lib.trench_packed_probe_at.restype = ctypes.c_int
lib.trench_engine_create.argtypes = []
lib.trench_engine_create.restype = ctypes.c_void_p
lib.trench_engine_destroy.argtypes = [ctypes.c_void_p]
lib.trench_engine_prepare.argtypes = [ctypes.c_void_p, ctypes.c_double]
lib.trench_engine_load_body_bytes_at.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_double]
lib.trench_engine_load_body_bytes_at.restype = ctypes.c_int
lib.trench_engine_process_block.argtypes = [
    ctypes.c_void_p, ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_float), ctypes.c_int,
    ctypes.c_double, ctypes.c_double]

lib.trench_fit_arma_endpoint_pinned_pairs.argtypes = [
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double),
    ctypes.c_size_t, ctypes.POINTER(ctypes.c_double), ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_double), ctypes.c_size_t, ctypes.c_double,
    ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint16),
    ctypes.POINTER(ctypes.c_double)]
lib.trench_fit_arma_endpoint_pinned_pairs.restype = ctypes.c_int

decode = lib.trench_packed_decode
encode = lib.trench_packed_encode

RATE = 44_100.0
LANES = 6
K_FLOOR = -math.log(decode(1))
OCT_PER_K = 1.0 / (2.0 * math.log(2.0))
DB_PER_K = 20.0 / math.log(10.0) / 2.0
GRID = np.geomspace(20.0, 20_000.0, 900)

def k_of(word: int) -> float:
    v = decode(int(word))
    return K_FLOOR if v <= 0.0 else -math.log(v)


def word_of(k: float) -> int:
    return int(encode(math.exp(-max(k, 0.0))))


def pair_geometry(k1: float, k2: float, rate: float):
    ek1, ek2 = math.exp(-k1), math.exp(-k2)
    b2 = 1.0 - ek2
    b1 = -2.0 + ek2 + 4.0 * ek1
    if b2 <= 0.0 or b1 * b1 - 4.0 * b2 >= 0.0:
        return None
    r = math.sqrt(b2)
    c = -b1 / (2.0 * r)
    if abs(c) > 1.0:
        return None
    return math.acos(c) / (2.0 * math.pi) * rate, r


def peak_db(r: float) -> float:
    return math.inf if r >= 1.0 else -20.0 * math.log10(max(1.0 - r, 1e-300))



@dataclass
class Row:
    w: list[int] = field(default_factory=lambda: [0, 0, 0, 0, 0])

    def k(self, slot: int) -> float:
        return k_of(self.w[slot])

    def set_k(self, slot: int, k: float) -> None:
        if k == self.k(slot):
            return
        self.w[slot] = word_of(k)

    def zero(self, rate: float):
        return pair_geometry(self.k(0), self.k(1), rate)

    def pole(self, rate: float):
        return pair_geometry(self.k(2), self.k(3), rate)

    def interval_st(self, rate: float) -> float | None:
        p, z = self.pole(rate), self.zero(rate)
        if not p or not z or p[0] <= 0.0 or z[0] <= 0.0:
            return None
        return 12.0 * math.log2(z[0] / p[0])

    def scale(self) -> float:
        return 4.0 * decode(self.w[4])


def _assign(cost):
    # Exact minimum-cost assignment. At most 7 lanes, so 5040 permutations is
    # cheaper than pulling in a solver, and it is exact rather than a heuristic.
    n = len(cost)
    best, arg = None, tuple(range(n))
    for perm in itertools.permutations(range(n)):
        t = 0.0
        for i in range(n):
            t += cost[i][perm[i]]
            if best is not None and t >= best:
                break
        else:
            if best is None or t < best:
                best, arg = t, perm
    return list(arg)


NATIVE_BYTES, LEGACY_BYTES = 560, 240
NATIVE_LANES, NATIVE_FRAMES = 7, 8


def body_shape(body: bytes) -> tuple[int, int]:
    return (NATIVE_LANES, NATIVE_FRAMES) if len(body) == NATIVE_BYTES else (LANES, 4)


@dataclass
class Sheet:
    rows: list[list[Row]] = field(default_factory=list)
    locked: list[bool] = field(default_factory=lambda: [False] * LANES)
    rate: float = RATE
    lanes: int = LANES
    frames: list[list[list[int]]] = field(default_factory=list)

    @classmethod
    def from_body(cls, body: bytes, rate: float = RATE) -> Sheet:
        lanes, nframes = body_shape(body)
        frames = []
        for f in range(nframes):
            rows = []
            for s in range(lanes):
                off = (f * lanes + s) * 10
                rows.append([int.from_bytes(body[off + 2 * i:off + 2 * i + 2], "little")
                             for i in range(5)])
            frames.append(rows)
        ends = [[Row(list(w)) for w in frames[0]], [Row(list(w)) for w in frames[1]]]
        locked = [ends[0][s].w == ends[1][s].w for s in range(lanes)]
        return cls(ends, locked, rate, lanes, frames)

    def row(self, lane: int, end: int) -> Row:
        return self.rows[0][lane] if (self.locked[lane] or end == 0) \
            else self.rows[1][lane]

    def pair_by_distance(self) -> None:
        # Shortest total distance in the plane the encoder actually stores.
        # A pole has an angle AND a radius: 1 kHz at r=0.99 is not 1 kHz at
        # r=0.80. Sorting by frequency only solves this if the cost lies on a
        # line - measured against the 2-D answer it disagrees on 17 of 18
        # factory bodies, by up to 3x on multi_q_vox. So the cost is Euclidean
        # in ARMAdillo coordinates, where equal distance is equal perceived
        # change, with an octave and 8.68 dB of resonance weighted alike.
        def arma(row):
            p = row.pole(self.rate)
            if not p or p[0] <= 20.0:
                return None
            th = 2.0 * math.pi * p[0] / self.rate
            ang = math.pi * ((10.0 + math.log2(th / math.pi)) / 10.0)
            res = -20.0 * math.log10(max(1.0 - min(p[1], 0.999999), 1e-9))
            return (ang / (math.pi / 10.0) * 12.0, res / 8.68 * 12.0)

        free = [i for i in range(self.lanes)
                if not self.locked[i]
                and arma(self.rows[0][i]) and arma(self.rows[1][i])]
        if len(free) < 2:
            return
        near = [arma(self.rows[0][i]) for i in free]
        far_pt = [arma(self.rows[1][i]) for i in free]
        cost = [[math.hypot(far_pt[j][0] - near[i][0], far_pt[j][1] - near[i][1])
                 for j in range(len(free))] for i in range(len(free))]
        order = _assign(cost)
        far = [self.rows[1][i] for i in range(self.lanes)]
        for i, lane in enumerate(free):
            self.rows[1][lane] = far[free[order[i]]]

    def swap_pairing(self, a: int, b: int) -> None:
        # Which root at one end travels to which root at the other IS the
        # authored quantity. Swapping the far end of two lanes leaves both
        # endpoints bit-identical and moves the middle of the wheel by up to
        # 95 dB. Every one of the lanes! pairings is reachable by these swaps.
        if a == b or self.locked[a] or self.locked[b]:
            return
        self.rows[1][a], self.rows[1][b] = self.rows[1][b], self.rows[1][a]

    def body(self) -> bytes:
        cols = [[self.row(s, 0) for s in range(self.lanes)],
                [self.row(s, 1) for s in range(self.lanes)]]
        # the two edited columns are frames 0 and 1; every other frame of a cube
        # is written back exactly as it was loaded
        frames = [[list(w) for w in f] for f in self.frames] or [
            [r.w for r in cols[0]], [r.w for r in cols[1]],
            [r.w for r in cols[0]], [r.w for r in cols[1]]]
        frames[0] = [r.w for r in cols[0]]
        frames[1] = [r.w for r in cols[1]]
        out = bytearray()
        for f in frames:
            for w in f:
                for word in w:
                    out += int(word).to_bytes(2, "little")
        return bytes(out)


def probe(body: bytes, morph: float, rate: float):
    lanes, nframes = body_shape(body)
    if nframes == 4:
        coeffs = (ctypes.c_double * RT_DOUBLES)()
        max_r = ctypes.c_double()
        unstable = ctypes.c_uint32()
        nonfinite = ctypes.c_uint32()
        rc = lib.trench_packed_probe_at(body, len(body), morph, 0.0, rate, coeffs,
                                        ctypes.byref(max_r),
                                        ctypes.byref(unstable),
                                        ctypes.byref(nonfinite))
        if rc != 0 or unstable.value or nonfinite.value:
            return None
        return [list(coeffs[s * 5:s * 5 + 5]) for s in range(lanes)]
    # trench_packed_probe_at is 240-byte only and takes no third axis, so a cube
    # goes through the word law here. Same arithmetic, checked by verify_word_law.
    words = np.frombuffer(body, dtype="<u2").reshape(nframes, lanes, 5).astype(np.int64)
    m = np.float32(morph)
    out = []
    for s in range(lanes):
        row = []
        for i in range(5):
            a, b = int(words[0, s, i]), int(words[1, s, i])
            d = np.int32(np.float32(np.float32(b - a) * m))
            d16 = np.int16(np.uint16(np.uint32(d) & np.uint32(0xFFFF)))
            row.append(int((np.int32(d16) + np.int32(a)) & np.int32(0xFFFF)))
        d = [decode(x) for x in row]
        c0 = 4.0 * d[0] + d[1]; c2 = 4.0 * d[2] + d[3]; c4 = 4.0 * d[4]
        out.append([c4, (c0 - 2.0) * c4, (1.0 - d[1]) * c4, c2 - 2.0, 1.0 - d[3]])
    return out


def closest_approach(body: bytes, rate: float, steps: int = 81):
    # The pairing does not decide WHETHER two poles meet, only where and when.
    # Talking Hedz meets at 557 Hz at MORPH 40; minimum-travel pairing on the
    # same twelve poles meets at 1939 Hz at MORPH 67.
    lanes, _ = body_shape(body)
    best = (9e9, 0.0, 0.0, 0, 0)
    for i in range(steps):
        m = i / (steps - 1)
        w = interp_words(body, m)
        hz = []
        for s in range(lanes):
            d = [decode(x) for x in w[s]]
            a1 = 4.0 * d[2] + d[3] - 2.0
            a2 = 1.0 - d[3]
            if a1 * a1 - 4.0 * a2 >= 0.0 or a2 <= 0.0:
                hz.append(None)
                continue
            r = math.sqrt(a2)
            hz.append(math.acos(max(-1.0, min(1.0, -a1 / (2.0 * r))))
                      / (2.0 * math.pi) * rate)
        for a in range(lanes):
            for b in range(a + 1, lanes):
                if not hz[a] or not hz[b] or hz[a] < 20 or hz[b] < 20:
                    continue
                st = abs(12.0 * math.log2(hz[b] / hz[a]))
                if st < best[0]:
                    best = (st, m, hz[a], a + 1, b + 1)
    return best


def interp_words(body: bytes, morph: float):
    lanes, nframes = body_shape(body)
    w = np.frombuffer(body, dtype="<u2").reshape(nframes, lanes, 5).astype(np.int64)
    m = np.float32(morph)
    out = []
    for s in range(lanes):
        row = []
        for i in range(5):
            a, b = int(w[0, s, i]), int(w[1, s, i])
            d = np.int32(np.float32(np.float32(b - a) * m))
            d16 = np.int16(np.uint16(np.uint32(d) & np.uint32(0xFFFF)))
            row.append(int((np.int32(d16) + np.int32(a)) & np.int32(0xFFFF)))
        out.append(row)
    return out


def travel_ruler(body: bytes, rate: float, steps: int = 81):
    # Martens, PALETTE (ICMC 1985): "the results of the last curve fitting are
    # used to predict how the synthesis parameter values should be spaced in the
    # next so as to separate the timbres by roughly equal perceptual distances".
    #
    # The wheel is linear in stored words, which says nothing about how the
    # sound is spaced. Walk the morph, measure how far the response actually
    # moves at each step, and return the cumulative fraction of the total change.
    # Where that curve is steep the wheel is doing a lot per degree; where it is
    # flat it is doing nothing.
    prev = None
    pos, cum, total = [], [], 0.0
    for i in range(steps):
        t = i / (steps - 1)
        cs = probe(body, t, rate)
        if cs is None:
            continue
        d = sum(section_db(c, rate) for c in cs)
        if prev is not None:
            total += float(np.sqrt(np.mean((d - prev) ** 2)))
        pos.append(t)
        cum.append(total)
        prev = d
    if total <= 0.0:
        return pos, [0.0] * len(pos)
    return pos, [c / total for c in cum]


def equal_steps(body: bytes, rate: float, n: int = 9):
    # The wheel positions that divide the morph into n-1 equal steps of change.
    # These are the places to audition or plot, not 0/12.5/25/...
    pos, frac = travel_ruler(body, rate)
    if not pos:
        return [i / (n - 1) for i in range(n)]
    return [float(np.interp(k / (n - 1), frac, pos)) for k in range(n)]


def section_db(c, rate: float) -> np.ndarray:
    w = 2.0 * np.pi * GRID / rate
    z1, z2 = np.exp(-1j * w), np.exp(-2j * w)
    return 20.0 * np.log10(
        np.abs((c[0] + c[1] * z1 + c[2] * z2)
               / (1.0 + c[3] * z1 + c[4] * z2)) + 1e-12)


def describe(row: Row, rate: float) -> str:
    p, z = row.pole(rate), row.zero(rate)
    if not p:
        return "pole real-rooted"
    st = row.interval_st(rate)
    bits = [f"pole {p[0]:,.0f} Hz  bw {bw_from_radius(p[1], rate):,.0f}"]
    if not z:
        bits.append("zero real-rooted")
    else:
        d = "null" if z[1] >= 1.0 else f"bw {bw_from_radius(z[1], rate):,.0f}"
        bits.append(f"zero {z[0]:,.0f} Hz  {d}")
    if st is not None:
        bits.append(f"{st:+.1f} st")
    return "   ".join(bits)


def radius_from_db(db: float) -> float:
    return 1.0 - 10.0 ** (-db / 20.0)


def response_db(body: bytes, morph: float, rate: float = RATE):
    cs = probe(body, morph, rate)
    if cs is None:
        return None
    return sum(section_db(c, rate) for c in cs)


def fit_endpoint(sheet, end: int, target_db, rate: float = RATE):
    poles, zeros = [], []
    for lane in range(LANES):
        row = sheet.row(lane, end)
        p, z = row.pole(rate), row.zero(rate)
        poles.append(p[0] if p else 0.0)
        zeros.append(z[0] if z else 0.0)
    if not any(h > 0 for h in poles):
        return "every lane is real-rooted - nothing to pin"
    n = len(GRID)
    tgt = np.asarray(target_db, dtype=float)
    tgt = tgt - float(np.mean(tgt))
    roots = (ctypes.c_double * RT_DOUBLES)()
    words = (ctypes.c_uint16 * 30)()
    met = (ctypes.c_double * 4)()
    rc = lib.trench_fit_arma_endpoint_pinned_pairs(
        (ctypes.c_double * n)(*GRID), (ctypes.c_double * n)(*tgt), n,
        (ctypes.c_double * LANES)(*poles), LANES,
        (ctypes.c_double * LANES)(*zeros), LANES,
        rate, roots, words, met)
    if rc != 0:
        return f"the fitter refused (rc {rc})"
    for lane in range(LANES):
        sheet.row(lane, end).w = [int(words[lane * 5 + i]) for i in range(5)]
    return f"fit {met[0]:.2f} dB rms, {int(met[2])} of 6 lanes used"


BW_MIN_HZ = 5.0
BW_MAX_HZ = 4000.0


def radius_from_bw(bw_hz: float, rate: float = RATE) -> float:
    return math.exp(-math.pi * bw_hz / rate)


def bw_from_radius(r: float, rate: float = RATE) -> float:
    if r <= 0.0 or r >= 1.0:
        return math.inf
    return -math.log(r) * rate / math.pi


def require_spec(hz: float, bw_hz: float, rate: float = RATE) -> str:
    if not (hz > 0.0) or hz > rate * 0.49:
        return f"frequency must be 0..{rate * 0.49:,.0f} Hz"
    if not (BW_MIN_HZ <= bw_hz <= BW_MAX_HZ):
        return f"bandwidth must be {BW_MIN_HZ:.0f}..{BW_MAX_HZ:,.0f} Hz"
    return ""
