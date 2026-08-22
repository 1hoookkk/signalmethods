from __future__ import annotations
import json
from dataclasses import dataclass, field
from pathlib import Path

PROFILES_DIR = Path(__file__).resolve().parents[1] / "profiles"

@dataclass
class FrequencyDomain:
    cutoff_range_hz: tuple[float, float] = (20.0, 20000.0)
    morph_axis_semantic: str = "cutoff_sweep"
    q_axis_semantic: str = "resonance_intensity"
    min_cutoff_hz: float = 20.0
    max_cutoff_hz: float = 20000.0

@dataclass
class ResonanceBudget:
    max_resonance_db: float = 24.0
    q0_default_resonance_db: float = 6.0
    corner_reg_weight: float = 0.1

@dataclass
class QAttitudeRules:
    allowed_behaviours: list[str] = field(default_factory=lambda: ["flat", "arm", "defuse"])
    max_arm_delta_r: float = 0.3
    q_axis_max_octave_shift: float = 2.0

@dataclass
class VoiceRoles:
    required: list[str] = field(default_factory=list)
    bands: dict[str, tuple[float, float]] = field(default_factory=dict)

@dataclass
class GuidePoseConstraints:
    min_cutoff_hz: float = 20.0
    max_cutoff_hz: float = 20000.0
    max_targets_per_guide: int = 4

@dataclass
class Verification:
    min_cutoff_travel_octaves: float = 2.0
    max_bass_loss_db: float = 6.0
    min_peak_count_morph_range: tuple[int, int] = (0, 20)

@dataclass
class TrenchProfile:
    profile: str = "trenchprofile-v1"
    family: str = "generic"
    description: str = ""
    frequency_domain: FrequencyDomain = field(default_factory=FrequencyDomain)
    resonance_budget: ResonanceBudget = field(default_factory=ResonanceBudget)
    q_attitude_rules: QAttitudeRules = field(default_factory=QAttitudeRules)
    voice_roles: VoiceRoles = field(default_factory=VoiceRoles)
    guide_pose_constraints: GuidePoseConstraints = field(default_factory=GuidePoseConstraints)
    verification: Verification = field(default_factory=Verification)

    @classmethod
    def from_dict(cls, data: dict) -> "TrenchProfile":
        fd = data.get("frequency_domain", {})
        rb = data.get("resonance_budget", {})
        qa = data.get("q_attitude_rules", {})
        vr = data.get("voice_roles", {})
        gp = data.get("guide_pose_constraints", {})
        vf = data.get("verification", {})

        return cls(
            profile=data.get("profile", "trenchprofile-v1"),
            family=data.get("family", "generic"),
            description=data.get("description", ""),
            frequency_domain=FrequencyDomain(
                cutoff_range_hz=tuple(fd.get("cutoff_range_hz", [20.0, 20000.0])),
                morph_axis_semantic=fd.get("morph_axis_semantic", "cutoff_sweep"),
                q_axis_semantic=fd.get("q_axis_semantic", "resonance_intensity"),
                min_cutoff_hz=fd.get("min_cutoff_hz", 20.0),
                max_cutoff_hz=fd.get("max_cutoff_hz", 20000.0),
            ),
            resonance_budget=ResonanceBudget(
                max_resonance_db=rb.get("max_resonance_db", 24.0),
                q0_default_resonance_db=rb.get("q0_default_resonance_db", 6.0),
                corner_reg_weight=rb.get("corner_reg_weight", 0.1),
            ),
            q_attitude_rules=QAttitudeRules(
                allowed_behaviours=qa.get("allowed_behaviours", ["flat", "arm", "defuse"]),
                max_arm_delta_r=qa.get("max_arm_delta_r", 0.3),
                q_axis_max_octave_shift=qa.get("q_axis_max_octave_shift", 2.0),
            ),
            voice_roles=VoiceRoles(
                required=vr.get("required", []),
                bands=vr.get("bands", {}),
            ),
            guide_pose_constraints=GuidePoseConstraints(
                min_cutoff_hz=gp.get("min_cutoff_hz", 20.0),
                max_cutoff_hz=gp.get("max_cutoff_hz", 20000.0),
                max_targets_per_guide=gp.get("max_targets_per_guide", 4),
            ),
            verification=Verification(
                min_cutoff_travel_octaves=vf.get("min_cutoff_travel_octaves", 2.0),
                max_bass_loss_db=vf.get("max_bass_loss_db", 6.0),
                min_peak_count_morph_range=tuple(
                    vf.get("min_peak_count_morph_range", [0, 20])),
            ),
        )

    def to_dict(self) -> dict:
        return {
            "profile": self.profile,
            "family": self.family,
            "description": self.description,
            "frequency_domain": {
                "cutoff_range_hz": list(self.frequency_domain.cutoff_range_hz),
                "morph_axis_semantic": self.frequency_domain.morph_axis_semantic,
                "q_axis_semantic": self.frequency_domain.q_axis_semantic,
                "min_cutoff_hz": self.frequency_domain.min_cutoff_hz,
                "max_cutoff_hz": self.frequency_domain.max_cutoff_hz,
            },
            "resonance_budget": {
                "max_resonance_db": self.resonance_budget.max_resonance_db,
                "q0_default_resonance_db": self.resonance_budget.q0_default_resonance_db,
                "corner_reg_weight": self.resonance_budget.corner_reg_weight,
            },
            "q_attitude_rules": {
                "allowed_behaviours": self.q_attitude_rules.allowed_behaviours,
                "max_arm_delta_r": self.q_attitude_rules.max_arm_delta_r,
                "q_axis_max_octave_shift": self.q_attitude_rules.q_axis_max_octave_shift,
            },
            "voice_roles": {
                "required": self.voice_roles.required,
                "bands": self.voice_roles.bands,
            },
            "guide_pose_constraints": {
                "min_cutoff_hz": self.guide_pose_constraints.min_cutoff_hz,
                "max_cutoff_hz": self.guide_pose_constraints.max_cutoff_hz,
                "max_targets_per_guide": self.guide_pose_constraints.max_targets_per_guide,
            },
            "verification": {
                "min_cutoff_travel_octaves": self.verification.min_cutoff_travel_octaves,
                "max_bass_loss_db": self.verification.max_bass_loss_db,
                "min_peak_count_morph_range": list(
                    self.verification.min_peak_count_morph_range),
            },
        }

def load_profile(family: str) -> TrenchProfile:
    path = PROFILES_DIR / f"{family}.trenchprofile.json"
    if not path.exists():
        raise FileNotFoundError(f"Profile not found: {path}")
    return TrenchProfile.from_dict(json.loads(path.read_text()))

def list_profiles() -> list[str]:
    profiles = []
    for p in PROFILES_DIR.glob("*.trenchprofile.json"):
        data = json.loads(p.read_text())
        profiles.append(data.get("family", p.stem))
    return sorted(profiles)
