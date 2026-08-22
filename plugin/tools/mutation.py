from __future__ import annotations
import copy

import numpy as np

from tools.trenchsrc import TrenchSrc, Voice, QAttitude

ROLE_BANDS = {
    "anchor":   (30.0, 400.0),
    "mouth":    (400.0, 3500.0),
    "air":      (3500.0, 18000.0),
    "terminal": (3500.0, 18000.0),
}

HZ_MIN, HZ_MAX = 20.0, 20000.0
R_MAX = 0.999
SCALE_MIN, SCALE_MAX = 0.1, 3.0

def _band_for(voice: Voice) -> tuple[float, float]:
    return ROLE_BANDS.get(voice.role, (HZ_MIN, HZ_MAX))

def _clamp_hz(hz: float, band: tuple[float, float]) -> float:
    return float(np.clip(hz, max(band[0], HZ_MIN), min(band[1], HZ_MAX)))

def _clamp_r(r: float) -> float:
    return float(np.clip(r, 0.0, R_MAX))

def _clamp_scale(s: float) -> float:
    return float(np.clip(s, SCALE_MIN, SCALE_MAX))

def _jitter_log(hz: float, sigma_octaves: float, band: tuple[float, float],
                rng: np.random.Generator) -> float:
    octaves = rng.normal(0.0, sigma_octaves)
    return _clamp_hz(hz * (2.0 ** octaves), band)

def _resolve_lane(src: TrenchSrc, lane_id: str) -> tuple[int, Voice]:
    v = src.voice_by_id(lane_id)
    if v is None:
        raise KeyError(f"Lane '{lane_id}' not found in {src.name}")
    return src.stage_of(lane_id), v

def mutate_pole_hz(src: TrenchSrc, lane_id: str = "S1",
                   sigma_octaves: float = 0.2,
                   rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    s = copy.deepcopy(src)
    _, v = _resolve_lane(s, lane_id)
    band = _band_for(v)
    v.m0_hz = _jitter_log(v.m0_hz, sigma_octaves, band, rng)
    v.m100_hz = _jitter_log(v.m100_hz, sigma_octaves, band, rng)
    return s

def mutate_pole_r(src: TrenchSrc, lane_id: str = "S1",
                  sigma: float = 0.03,
                  rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    s = copy.deepcopy(src)
    _, v = _resolve_lane(s, lane_id)
    v.m0_r = _clamp_r(v.m0_r + rng.normal(0.0, sigma))
    v.m100_r = _clamp_r(v.m100_r + rng.normal(0.0, sigma))
    return s

def mutate_zero_hz(src: TrenchSrc, lane_id: str = "S1",
                   sigma_octaves: float = 0.3,
                   rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    s = copy.deepcopy(src)
    _, v = _resolve_lane(s, lane_id)
    band = _band_for(v)
    v.m0_zero_hz = _jitter_log(v.m0_zero_hz, sigma_octaves, band, rng)
    v.m100_zero_hz = _jitter_log(v.m100_zero_hz, sigma_octaves, band, rng)
    return s

def mutate_zero_r(src: TrenchSrc, lane_id: str = "S1",
                  sigma: float = 0.05,
                  rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    s = copy.deepcopy(src)
    _, v = _resolve_lane(s, lane_id)
    v.m0_zero_r = _clamp_r(v.m0_zero_r + rng.normal(0.0, sigma))
    v.m100_zero_r = _clamp_r(v.m100_zero_r + rng.normal(0.0, sigma))
    return s

def mutate_scale(src: TrenchSrc, lane_id: str = "S1",
                 sigma: float = 0.1,
                 rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    s = copy.deepcopy(src)
    _, v = _resolve_lane(s, lane_id)
    v.scale = _clamp_scale(v.scale + rng.normal(0.0, sigma))
    return s

Q_BEHAVIOURS = ["flat", "arm", "defuse"]

def mutate_q_attitude(src: TrenchSrc, lane_id: str = "S1",
                      rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    s = copy.deepcopy(src)
    stage_idx, _ = _resolve_lane(s, lane_id)
    q_lane_num = stage_idx + 1
    for qa in s.q_attitudes:
        if qa.lane == q_lane_num:
            if rng.random() < 0.5:
                current = qa.behaviour
                options = [b for b in Q_BEHAVIOURS if b != current]
                qa.behaviour = options[rng.integers(0, len(options))]
            else:
                qa.arm_delta_r = _clamp_r(qa.arm_delta_r + rng.normal(0.0, 0.02))
                if qa.arm_delta_r < 0.005:
                    qa.behaviour = "flat"
            break
    return s

def swap_stages(src: TrenchSrc, i: int, j: int) -> TrenchSrc:
    s = copy.deepcopy(src)
    s.swap_stages(i, j)
    return s

def crossover(parent_a: TrenchSrc, parent_b: TrenchSrc,
              rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    child = copy.deepcopy(parent_a)
    votes_a = 0
    for i, va in enumerate(child.voices):
        vb = parent_b.voice_by_id(va.id)
        if vb is not None and rng.random() < 0.5:
            child.voices[i] = copy.deepcopy(vb)
        else:
            votes_a += 1

    if votes_a < len(child.voices) / 2:
        child.q_attitudes = copy.deepcopy(parent_b.q_attitudes)
        child.motion_wiring = copy.deepcopy(parent_b.motion_wiring)
    return child

def revoice_lane(src: TrenchSrc, lane_id: str = "S1",
                 rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    s = copy.deepcopy(src)
    v = s.voice_by_id(lane_id)
    if v is None:
        raise KeyError(f"Lane '{lane_id}' not found")
    role = v.role
    band = ROLE_BANDS.get(role, (HZ_MIN, HZ_MAX))
    hz_lo, hz_hi = band

    v.m0_hz = float(rng.uniform(hz_lo, hz_hi))
    v.m0_r = float(rng.uniform(0.0, 0.97))
    v.m0_zero_hz = float(rng.uniform(hz_lo * 0.5, hz_hi * 2.0))
    v.m0_zero_r = float(rng.uniform(0.0, 0.5))
    v.m100_hz = float(rng.uniform(hz_lo, hz_hi))
    v.m100_r = float(rng.uniform(0.0, 0.97))
    v.m100_zero_hz = float(rng.uniform(hz_lo * 0.5, hz_hi * 2.0))
    v.m100_zero_r = float(rng.uniform(0.0, 0.5))
    v.scale = float(rng.uniform(0.3, 1.5))
    return s

OPERATORS: list[tuple[callable, float, str]] = [
    (mutate_pole_hz,     4.0, "pole_hz"),
    (mutate_pole_r,      3.0, "pole_r"),
    (mutate_zero_hz,     2.0, "zero_hz"),
    (mutate_zero_r,      1.5, "zero_r"),
    (mutate_scale,       1.5, "scale"),
    (mutate_q_attitude,  1.0, "q_attitude"),
    (revoice_lane,       0.5, "revoice"),
]

WEIGHTS = np.array([w for _, w, _ in OPERATORS], dtype=float)
WEIGHTS /= WEIGHTS.sum()

def random_mutation(src: TrenchSrc,
                    rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    lane_ids = src.lane_ids()
    lane_id = lane_ids[rng.integers(0, len(lane_ids))]

    op_fn = OPERATORS[rng.choice(len(OPERATORS), p=WEIGHTS)][0]
    return op_fn(src, lane_id=lane_id, rng=rng)

def random_mutation_chain(src: TrenchSrc, n_mutations: int = 2,
                          rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    s = src
    for _ in range(n_mutations):
        s = random_mutation(s, rng=rng)
    return s

def random_pairing_mutation(src_a: TrenchSrc, src_b: TrenchSrc,
                            rng: np.random.Generator | None = None) -> TrenchSrc:
    if rng is None:
        rng = np.random.default_rng()
    child = crossover(src_a, src_b, rng=rng)
    return random_mutation(child, rng=rng)
