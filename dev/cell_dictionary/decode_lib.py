"""
Independent, from-scratch reimplementation of the trench-core minifloat /
stage-geometry decode law, plus a from-scratch (different code path) decoder
for the raw Morpheus records_stream.bin biphase-mark transfer stream.

This module exists to REPRODUCE, not to trust, the existing repository
decoders (trench-core/src/minifloat.rs, trench-core/src/stage_law.rs,
dev/decode_cubes_complete.py). See phase0_reproduce.py for the comparison.

Read-only: this module never writes into the repository outside
dev/cell_dictionary/output/.
"""
import math
import struct

TAU = 2.0 * math.pi

# ---------------------------------------------------------------------------
# minifloat decode -- ported from trench-core/src/minifloat.rs decode() (L4-20)
# ---------------------------------------------------------------------------

def minifloat_decode(word):
    u = word + 1
    if u == 65536:
        return 1.0
    if u == 1:
        return 0.0
    e = (u >> 12) & 0xF
    m = float(u & 0xFFF)
    if e == 0:
        x = m / 4096.0
    else:
        x = (m + 4096.0) / 8192.0
    return x * (2.0 ** (e - 15))


# ---------------------------------------------------------------------------
# stage geometry -- ported from trench-core/src/stage_law.rs
# pair_geometry_at() (L258-280) and geometry_from_words_at() (L229-235)
# ---------------------------------------------------------------------------

class Conjugate:
    __slots__ = ("hz", "r")
    kind = "conjugate"

    def __init__(self, hz, r):
        self.hz = hz
        self.r = r

    def __repr__(self):
        return f"Conjugate(hz={self.hz:.4f}, r={self.r:.6f})"


class RealPair:
    __slots__ = ("root_a", "root_b")
    kind = "real"

    def __init__(self, root_a, root_b):
        self.root_a = root_a
        self.root_b = root_b

    def __repr__(self):
        return f"RealPair(a={self.root_a:.6f}, b={self.root_b:.6f})"


class Degenerate:
    kind = "degenerate"

    def __repr__(self):
        return "Degenerate()"


def pair_geometry_at(d_mag, d_rsq, sample_rate_hz):
    q = 1.0 - d_rsq
    c = 4.0 * d_mag + d_rsq
    p = c - 2.0
    if p == 0.0 and q == 0.0:
        return Degenerate()
    disc = p * p - 4.0 * q
    if disc < 0.0:
        r = math.sqrt(q)
        cos_w = -p / (2.0 * r)
        # guard against tiny float overshoot of acos domain
        cos_w = max(-1.0, min(1.0, cos_w))
        hz = math.acos(cos_w) / TAU * sample_rate_hz
        return Conjugate(hz, r)
    else:
        s = math.sqrt(disc)
        return RealPair((-p + s) / 2.0, (-p - s) / 2.0)


class StageGeometry:
    __slots__ = ("pole", "zero", "scale")

    def __init__(self, pole, zero, scale):
        self.pole = pole
        self.zero = zero
        self.scale = scale


def geometry_from_words_at(words, sample_rate_hz):
    zero = pair_geometry_at(minifloat_decode(words[0]), minifloat_decode(words[1]), sample_rate_hz)
    pole = pair_geometry_at(minifloat_decode(words[2]), minifloat_decode(words[3]), sample_rate_hz)
    scale = 4.0 * minifloat_decode(words[4])
    return StageGeometry(pole=pole, zero=zero, scale=scale)


# ---------------------------------------------------------------------------
# P2K 240-byte legacy body: 4 corners x 6 stages x 5 u16 words, LE,
# corner-major then stage-major then word-major.
# Ported from trench-core/src/minifloat.rs from_rom_bytes() (L139-157).
# ---------------------------------------------------------------------------

LEGACY_CORNERS = 4
LEGACY_STAGES = 6
NUM_COEFFS = 5
LEGACY_BODY_BYTES = LEGACY_CORNERS * LEGACY_STAGES * NUM_COEFFS * 2
P2K_CORNER_LABELS = ["M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100"]


def parse_p2k_bytes(data):
    assert len(data) == LEGACY_BODY_BYTES, f"expected {LEGACY_BODY_BYTES} bytes, got {len(data)}"
    idx = 0
    corners = []
    for _ci in range(LEGACY_CORNERS):
        stages = []
        for _si in range(LEGACY_STAGES):
            words = []
            for _wi in range(NUM_COEFFS):
                w = data[idx] | (data[idx + 1] << 8)
                idx += 2
                words.append(w)
            stages.append(words)
        corners.append(stages)
    assert idx == LEGACY_BODY_BYTES
    return corners  # corners[ci][si] = [w0..w4] raw u16


NATIVE_CORNERS = 8
NATIVE_STAGES = 7
BODY_BYTES = NATIVE_CORNERS * NATIVE_STAGES * NUM_COEFFS * 2  # 560


def parse_native_bytes(data):
    """560-byte native body: 8 corners x 7 stages x 5 u16 words, LE,
    corner-major then stage-major then word-major.
    Ported from trench-core/src/minifloat.rs from_native_bytes() (L158-175)."""
    assert len(data) == BODY_BYTES, f"expected {BODY_BYTES} bytes, got {len(data)}"
    idx = 0
    corners = []
    for _ci in range(NATIVE_CORNERS):
        stages = []
        for _si in range(NATIVE_STAGES):
            words = []
            for _wi in range(NUM_COEFFS):
                w = data[idx] | (data[idx + 1] << 8)
                idx += 2
                words.append(w)
            stages.append(words)
        corners.append(stages)
    assert idx == BODY_BYTES
    return corners  # corners[ci][si] = [w0..w4] raw u16


def decode_native_body(data, sample_rate_hz):
    raw = parse_native_bytes(data)
    return [[geometry_from_words_at(words, sample_rate_hz) for words in stages] for stages in raw]


def decode_p2k_body(data, sample_rate_hz=39_062.5):
    """Returns corners[ci][si] = StageGeometry, ci indexed exactly as
    P2K_CORNER_LABELS (byte-array corner order == that label order, per
    author/src/bin/regen_recipes.rs's `for (ci, corner) in CORNERS.iter()`)."""
    raw = parse_p2k_bytes(data)
    return [[geometry_from_words_at(words, sample_rate_hz) for words in stages] for stages in raw]


# ---------------------------------------------------------------------------
# Morpheus records_stream.bin: independent re-implementation of the raw
# record/field/angle/radius/gain extraction, using a DIFFERENT bit-extraction
# technique (explicit bit-string slicing per 32-bit LE word) than
# dev/decode_cubes_complete.py's giant-int accumulate-and-shift approach, as
# a genuine cross-check of the same documented law rather than a retyping
# of the same code path.
# ---------------------------------------------------------------------------

REC0 = 340
STRIDE = 332
NREC = 289
MORPH_FS = 39_062.5

K_ANGLE = struct.unpack('<f', struct.pack('<I', 0x32C90FDB))[0]
K_RADIUS = struct.unpack('<f', struct.pack('<I', 0x32800800))[0]
K_GAIN = struct.unpack('<f', struct.pack('<I', 0x338007FF))[0]


def _bits_of_u32_le_word(word_bytes):
    """4 LE bytes -> 32-bit unsigned int -> 32-char MSB-first bit string."""
    val = struct.unpack('<I', word_bytes)[0]
    return format(val, '032b')


def block_fields_v2(block_bytes, nfields):
    """Extract `nfields` 11-bit fields, MSB-first, from a byte block
    interpreted as a sequence of little-endian 32-bit words concatenated
    MSB-first (word 0's top bit first). Independent bit-string implementation
    of dev/decode_cubes_complete.py's block_fields()."""
    nwords = (len(block_bytes) + 3) // 4
    padded = block_bytes + b"\x00" * (nwords * 4 - len(block_bytes))
    bitstring = "".join(
        _bits_of_u32_le_word(padded[i * 4:(i + 1) * 4]) for i in range(nwords)
    )
    fields = []
    for k in range(nfields):
        # bitstring is already MSB-first (format(...,'032b') puts each
        # word's bit31 at index 0), so field k is simply the k-th 11-bit
        # chunk from the front -- NOT total_bits - 11*(k+1), which was an
        # LSB-first offset applied to an MSB-first string and silently
        # reversed the whole field order (caught by cross-checking against
        # ref/morpheus/cubes_decoded.json for LPFlange.4, where the null
        # vs. active Z-plane came out backwards).
        start = 11 * k
        chunk = bitstring[start:start + 11]
        fields.append(int(chunk, 2))
    return fields


def _encoded(field):
    return (field << 4) | (0xF if field else 0)


def _float_law(v):
    e = (v >> 11) & 0xF
    m = v & 0x7FF
    h = 1 if e else 0
    return (m | (h << 11)) << (e - h)


def decode_angle_v2(field):
    v = _encoded(field)
    e = (v >> 11) & 0xF
    m = v & 0x7FF
    return ((m | 0x800) << e) * K_ANGLE


def decode_radius_v2(field):
    return 1.0 - _float_law(_encoded(field)) * K_RADIUS


def decode_gain_v2(field):
    return _float_law(_encoded(field)) * K_GAIN


def decode_morpheus_record_v2(rec):
    """rec: 332-byte raw record (12-byte name + 320-byte payload)."""
    assert len(rec) == STRIDE
    name = rec[:12].decode('ascii', errors='replace').rstrip('\x00').rstrip()
    payload = rec[12:]
    stage_params = []
    for s in range(7):
        block = payload[s * 44:(s + 1) * 44]
        f = block_fields_v2(block, 32)
        # param-major, 8 corners per param: [pole_angle, pole_radius, zero_angle, zero_radius]
        stage_params.append([f[p * 8:(p + 1) * 8] for p in range(4)])
    gain_block = payload[308:320]
    gains_raw = block_fields_v2(gain_block, 8)
    tail_word = struct.unpack_from('<I', payload, 316)[0]

    corners = []
    for c in range(8):
        secs = []
        for s in range(7):
            pf, pr, zf, zr = (stage_params[s][p][c] for p in range(4))
            pole_angle = decode_angle_v2(pf)
            zero_angle = decode_angle_v2(zf)
            secs.append({
                'pole': {'hz': pole_angle * MORPH_FS / TAU, 'r': decode_radius_v2(pr)},
                'zero': {'hz': zero_angle * MORPH_FS / TAU, 'r': decode_radius_v2(zr)},
                'raw': [pf, pr, zf, zr],
            })
        corners.append({'gain': decode_gain_v2(gains_raw[c]), 'gain_raw': gains_raw[c], 'sections': secs})
    return {'name': name, 'tail_word': tail_word, 'corners': corners}


def load_records_stream(path):
    with open(path, 'rb') as f:
        data = f.read()
    return data


def decode_all_morpheus_records_v2(stream_path):
    data = load_records_stream(stream_path)
    out = []
    for i in range(NREC):
        rec = data[REC0 + i * STRIDE: REC0 + (i + 1) * STRIDE]
        out.append(decode_morpheus_record_v2(rec))
    return out


# ---------------------------------------------------------------------------
# damping-space radius coordinate: d = -ln(r)   (proportional to bandwidth
# at fixed sample rate for a conjugate pole/zero pair). r must be in (0, 1].
# r == 0 (root at DC / degenerate) is undefined in damping space; callers
# must exclude those states from damping-space distance computations and
# report them separately -- never silently clamp.
# ---------------------------------------------------------------------------

def damping(r):
    if r <= 0.0:
        return None
    if r >= 1.0:
        return 0.0
    return -math.log(r)


# ---------------------------------------------------------------------------
# R' log-polar radius mapping (armadillo.rs), kept ONLY for the reproduce
# step's cross-reference -- not used as the primary metric per the user's
# explicit instruction (damping space is primary).
# ---------------------------------------------------------------------------

def r_prime_db(r):
    gap = 1.0 - r
    if gap <= 0.0:
        return None
    return -20.0 * math.log10(gap)


def semitones(hz_num, hz_den):
    if hz_num <= 0.0 or hz_den <= 0.0:
        return None
    return 12.0 * math.log2(hz_num / hz_den)


# ---------------------------------------------------------------------------
# biquad coefficients + H(z) magnitude, for response-domain error.
# Ported from trench-core/src/stage_law.rs pair_coefficients_at() (L216-225)
# and StageGeometry::biquad_at() (L22-32); response-domain dB summation
# matches CLAUDE.md's Rules ("dB responses add") and the mag()/resp()
# pattern used throughout trench-core/src/minifloat.rs's own test code.
# ---------------------------------------------------------------------------

def pair_coefficients_at(pair, sr):
    if isinstance(pair, Conjugate):
        angle = TAU * pair.hz / sr
        return (-2.0 * pair.r * math.cos(angle), pair.r * pair.r)
    if isinstance(pair, RealPair):
        return (-(pair.root_a + pair.root_b), pair.root_a * pair.root_b)
    return (0.0, 0.0)  # Degenerate


def stage_biquad(geom, sr):
    zero_p, zero_q = pair_coefficients_at(geom.zero, sr)
    pole_p, pole_q = pair_coefficients_at(geom.pole, sr)
    return (geom.scale, geom.scale * zero_p, geom.scale * zero_q, pole_p, pole_q)


def stage_response_db(bq, hz_array, sr):
    """hz_array: iterable of Hz. Returns list of dB at each frequency for
    this one stage's biquad."""
    b0, b1, b2, a1, a2 = bq
    out = []
    for hz in hz_array:
        w = TAU * hz / sr
        cw, sw = math.cos(w), math.sin(w)
        c2w, s2w = math.cos(2 * w), math.sin(2 * w)
        nr = b0 + b1 * cw + b2 * c2w
        ni = -(b1 * sw + b2 * s2w)
        dr = 1.0 + a1 * cw + a2 * c2w
        di = -(a1 * sw + a2 * s2w)
        n = math.sqrt(nr * nr + ni * ni)
        d = max(math.sqrt(dr * dr + di * di), 1e-12)
        mag = max(n / d, 1e-12)
        out.append(20.0 * math.log10(mag))
    return out


def corner_response_db(stage_geoms, hz_array, sr):
    """stage_geoms: list of StageGeometry for one corner (all stages,
    identity/degenerate stages contribute exactly 0 dB). Returns the summed
    (cascade) dB response across hz_array."""
    total = [0.0] * len(hz_array)
    for g in stage_geoms:
        bq = stage_biquad(g, sr)
        db = stage_response_db(bq, hz_array, sr)
        for i, v in enumerate(db):
            total[i] += v
    return total


def log_grid_hz(lo=40.0, hi=16000.0, n=200):
    ratio = hi / lo
    return [lo * (ratio ** (i / (n - 1))) for i in range(n)]


def hz_cents_bucket(hz, step_cents=10.0):
    """Quantize a frequency to the nearest step_cents bucket, on a log scale,
    so that two Hz values within the project's own documented ~10-cent
    packed-grid precision land in the SAME bucket -- sample-rate-independent
    once each object has already been decoded at its own correct datum."""
    if hz is None or hz <= 0.0:
        return None
    cents = 1200.0 * math.log2(hz)
    return round(round(cents / step_cents) * step_cents, 1)


def canonical_root_key(pair, step_cents=10.0, r_step=5e-4):
    if isinstance(pair, Conjugate):
        r_bucket = round(round(pair.r / r_step) * r_step, 6)
        return ("conjugate", hz_cents_bucket(pair.hz, step_cents), r_bucket)
    if isinstance(pair, RealPair):
        # linear roots, not log/cents -- bucket at the same absolute
        # granularity as the radius tolerance (both are unit-circle-scale
        # eigenvalue-like quantities)
        a = round(round(pair.root_a / r_step) * r_step, 6)
        b = round(round(pair.root_b / r_step) * r_step, 6)
        return ("real", a, b)
    return ("degenerate",)


def canonical_stage_key(geom, step_cents=10.0, r_step=5e-4, scale_db_step=0.01):
    pole_key = canonical_root_key(geom.pole, step_cents, r_step)
    zero_key = canonical_root_key(geom.zero, step_cents, r_step)
    if geom.scale is None or geom.scale <= 0.0:
        scale_key = None
    else:
        db = 20.0 * math.log10(geom.scale)
        scale_key = round(round(db / scale_db_step) * scale_db_step, 3)
    return (pole_key, zero_key, scale_key)


def stage_is_identity(geom):
    return isinstance(geom.pole, Degenerate) and isinstance(geom.zero, Degenerate)


def rms(values):
    if not values:
        return 0.0
    return math.sqrt(sum(v * v for v in values) / len(values))
