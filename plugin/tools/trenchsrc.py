from __future__ import annotations
import ctypes as C
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

import yaml
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
RATE = 44100.0
PAD = (0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000)
NUM_STAGES = 6
NUM_COEFFS = 5

@dataclass
class Voice:
    id: str
    role: str = "mouth"
    m0_hz: float = 1000.0
    m0_r: float = 0.0
    m0_zero_hz: float = 1000.0
    m0_zero_r: float = 0.0
    m100_hz: float = 1000.0
    m100_r: float = 0.0
    m100_zero_hz: float = 1000.0
    m100_zero_r: float = 0.0
    scale: float = 1.0

    def roots_m0(self) -> list[float]:
        return [self.m0_hz, self.m0_r, self.m0_zero_hz, self.m0_zero_r, self.scale]

    def roots_m100(self) -> list[float]:
        return [self.m100_hz, self.m100_r, self.m100_zero_hz, self.m100_zero_r, self.scale]

@dataclass
class QAttitude:
    lane: int
    behaviour: str = "flat"
    arm_delta_r: float = 0.0
    new_hz: float = 0.0

@dataclass
class GuidePose:
    morph: float = 0.5
    q: float = 0.5
    label: str = ""
    target_cutoff_hz: float | None = None
    target_resonance_db: float | None = None
    target_centroid_hz: float | None = None
    target_bass_db: float | None = None
    weight: float = 1.0

    def active_targets(self) -> list[str]:
        names = []
        if self.target_cutoff_hz is not None:
            names.append("cutoff_hz")
        if self.target_resonance_db is not None:
            names.append("resonance_db")
        if self.target_centroid_hz is not None:
            names.append("centroid_hz")
        if self.target_bass_db is not None:
            names.append("bass_db")
        return names

@dataclass
class TrenchSrc:
    name: str = "untitled"
    category: str = "WORKHORSE"
    scale_policy: str = "unity_dc"
    voices: list[Voice] = field(default_factory=list)
    q_attitudes: list[QAttitude] = field(default_factory=list)
    motion_wiring: dict = field(default_factory=dict)
    guide_poses: list[GuidePose] = field(default_factory=list)

    def voice_by_id(self, lane_id: str) -> Voice | None:
        for v in self.voices:
            if v.id == lane_id:
                return v
        return None

    def lane_ids(self) -> list[str]:
        return [v.id for v in self.voices]

    def swap_stages(self, i: int, j: int):
        self.voices[i], self.voices[j] = self.voices[j], self.voices[i]
        if i < len(self.q_attitudes) and j < len(self.q_attitudes):
            self.q_attitudes[i], self.q_attitudes[j] = \
                self.q_attitudes[j], self.q_attitudes[i]

    def stage_of(self, lane_id: str) -> int | None:
        for i, v in enumerate(self.voices):
            if v.id == lane_id:
                return i
        return None

    @classmethod
    def from_yaml(cls, path: Path | str) -> "TrenchSrc":
        with open(path) as f:
            data = yaml.safe_load(f)
        return cls.from_dict(data)

    @classmethod
    def from_dict(cls, data: dict) -> "TrenchSrc":
        voices = []
        for v in data.get("voices", []):
            voices.append(Voice(
                id=v.get("id", f"S{len(voices)+1}"),
                role=v.get("role", "mouth"),
                m0_hz=v.get("m0_hz", 1000.0),
                m0_r=v.get("m0_r", 0.0),
                m0_zero_hz=v.get("m0_zero_hz", 1000.0),
                m0_zero_r=v.get("m0_zero_r", 0.0),
                m100_hz=v.get("m100_hz", 1000.0),
                m100_r=v.get("m100_r", 0.0),
                m100_zero_hz=v.get("m100_zero_hz", 1000.0),
                m100_zero_r=v.get("m100_zero_r", 0.0),
                scale=v.get("scale", 1.0),
            ))
        q_attitudes = []
        for q in data.get("q_attitude", []):
            q_attitudes.append(QAttitude(
                lane=q.get("lane", 1),
                behaviour=q.get("behaviour", "flat"),
                arm_delta_r=q.get("arm_delta_r", 0.0),
                new_hz=q.get("new_hz", 0.0),
            ))
        guide_poses = []
        for g in data.get("guide_poses", []):
            guide_poses.append(GuidePose(
                morph=g.get("morph", 0.5),
                q=g.get("q", 0.5),
                label=g.get("label", ""),
                target_cutoff_hz=g.get("target_cutoff_hz"),
                target_resonance_db=g.get("target_resonance_db"),
                target_centroid_hz=g.get("target_centroid_hz"),
                target_bass_db=g.get("target_bass_db"),
                weight=g.get("weight", 1.0),
            ))
        return cls(
            name=data.get("name", "untitled"),
            category=data.get("category", "WORKHORSE"),
            scale_policy=data.get("scale_policy", "unity_dc"),
            voices=voices,
            q_attitudes=q_attitudes,
            motion_wiring=data.get("motion_wiring", {"morph": "preserve_order"}),
            guide_poses=guide_poses,
        )

    def to_dict(self) -> dict:
        return {
            "format": "trenchsrc-v1",
            "name": self.name,
            "category": self.category,
            "scale_policy": self.scale_policy,
            "voices": [
                {
                    "id": v.id,
                    "role": v.role,
                    "m0_hz": v.m0_hz,
                    "m0_r": v.m0_r,
                    "m0_zero_hz": v.m0_zero_hz,
                    "m0_zero_r": v.m0_zero_r,
                    "m100_hz": v.m100_hz,
                    "m100_r": v.m100_r,
                    "m100_zero_hz": v.m100_zero_hz,
                    "m100_zero_r": v.m100_zero_r,
                    "scale": v.scale,
                }
                for v in self.voices
            ],
            "q_attitude": [
                {"lane": q.lane, "behaviour": q.behaviour, "arm_delta_r": q.arm_delta_r}
                for q in self.q_attitudes
            ],
            "motion_wiring": self.motion_wiring,
            "guide_poses": [
                {
                    "morph": g.morph,
                    "q": g.q,
                    "label": g.label,
                    "target_cutoff_hz": g.target_cutoff_hz,
                    "target_resonance_db": g.target_resonance_db,
                    "target_centroid_hz": g.target_centroid_hz,
                    "target_bass_db": g.target_bass_db,
                    "weight": g.weight,
                }
                for g in self.guide_poses
            ],
        }

    def to_yaml(self, path: Path | str):
        with open(path, "w") as f:
            yaml.dump(self.to_dict(), f, default_flow_style=False, sort_keys=False)

    def compile(self) -> bytes:
        all_words = []
        for ci, (m, q) in enumerate([(0, 0), (1, 0), (0, 1), (1, 1)]):
            for si in range(NUM_STAGES):
                if si < len(self.voices):
                    v = self.voices[si]
                    if ci == 0:
                        roots = v.roots_m0()
                    elif ci == 1:
                        roots = v.roots_m100()
                    elif ci == 2:
                        roots = v.roots_m0()
                        roots = self._apply_q(roots, si, ci)
                    else:
                        roots = v.roots_m100()
                        roots = self._apply_q(roots, si, ci)
                else:
                    roots = [1000.0, 0.0, 1000.0, 0.0, 1.0]

                words = _encode_roots(roots, RATE)
                all_words.extend(words)

        return struct.pack("<120H", *all_words)

    def _apply_q(self, roots: list[float], si: int, ci: int) -> list[float]:
        r = list(roots)
        for qa in self.q_attitudes:
            if qa.lane == si + 1:
                if qa.behaviour == "arm":
                    r[1] = min(0.999, r[1] + qa.arm_delta_r)
                elif qa.behaviour == "defuse":
                    r[1] = max(0.0, r[1] - qa.arm_delta_r)
                elif qa.behaviour == "revoice" and qa.new_hz > 0:
                    r[0] = qa.new_hz
        return r

    @classmethod
    def random_seed(cls, name: str, category: str, num_voices: int = 6,
                    rng: np.random.Generator | None = None) -> "TrenchSrc":
        if rng is None:
            rng = np.random.default_rng()

        roles = ["anchor", "mouth", "mouth", "mouth", "air", "terminal"]
        voices = []
        for i in range(num_voices):
            role = roles[min(i, len(roles) - 1)]
            if role == "anchor":
                hz_lo, hz_hi = 30, 400
            elif role == "air" or role == "terminal":
                hz_lo, hz_hi = 3500, 18000
            else:
                hz_lo, hz_hi = 400, 3500

            m0_hz = float(rng.uniform(hz_lo, hz_hi))
            m100_hz = float(rng.uniform(hz_lo, hz_hi))
            r_val = float(rng.uniform(0.0, 0.97))

            voices.append(Voice(
                id=f"S{i+1}", role=role,
                m0_hz=m0_hz, m0_r=r_val,
                m0_zero_hz=m0_hz * rng.uniform(0.5, 5.0),
                m0_zero_r=rng.uniform(0.0, 0.5),
                m100_hz=m100_hz, m100_r=r_val,
                m100_zero_hz=m100_hz * rng.uniform(0.5, 5.0),
                m100_zero_r=rng.uniform(0.0, 0.5),
                scale=float(rng.uniform(0.3, 1.5)),
            ))

        q_attitudes = [
            QAttitude(lane=i + 1, behaviour="arm", arm_delta_r=0.04)
            for i in range(num_voices)
        ]

        return cls(
            name=name, category=category,
            voices=voices, q_attitudes=q_attitudes,
            motion_wiring={"morph": "preserve_order"},
        )

_lib = None

def _get_lib():
    global _lib
    if _lib is None:
        _lib = C.CDLL(str(ROOT / "target" / "release" / "trench_core.dll"))
        _lib.trench_stage_words_from_roots_at.argtypes = [
            C.POINTER(C.c_double), C.c_double, C.POINTER(C.c_uint16)]
        _lib.trench_stage_words_from_roots_at.restype = C.c_int
    return _lib

def _encode_roots(roots: list[float], rate: float) -> list[int]:
    lib = _get_lib()
    r_arr = (C.c_double * 5)(*roots)
    w_arr = (C.c_uint16 * 5)()
    rc = lib.trench_stage_words_from_roots_at(r_arr, rate, w_arr)
    if rc != 0:
        return list(PAD)
    return [int(w_arr[i]) for i in range(5)]

def render_body240(src: TrenchSrc) -> bytes:
    return src.compile()
