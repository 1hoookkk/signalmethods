from __future__ import annotations
import copy
import hashlib
import json
import time
from dataclasses import dataclass, field

import numpy as np
from scipy.optimize import minimize

from tools.trenchsrc import TrenchSrc, Voice, GuidePose, QAttitude
from tools.surface_evaluator import _probe_state
from tools.trench_profile import TrenchProfile

DEFAULT_SR = 48000.0

R_MIN, R_MAX = 0.0, 0.999
HZ_HARD_MIN, HZ_HARD_MAX = 20.0, 20000.0

def _resolve_bounds(profile: TrenchProfile | None
                    ) -> dict[str, float]:
    if profile is None:
        return {
            "max_resonance_db": 24.0,
            "corner_reg_weight": 0.1,
            "q_axis_max_octave_shift": 2.0,
            "min_cutoff_hz": 20.0,
            "max_cutoff_hz": 20000.0,
            "q0_default_resonance_db": 6.0,
        }
    return {
        "max_resonance_db": profile.resonance_budget.max_resonance_db,
        "corner_reg_weight": profile.resonance_budget.corner_reg_weight,
        "q_axis_max_octave_shift": profile.q_attitude_rules.q_axis_max_octave_shift,
        "min_cutoff_hz": profile.guide_pose_constraints.min_cutoff_hz,
        "max_cutoff_hz": profile.guide_pose_constraints.max_cutoff_hz,
        "q0_default_resonance_db": profile.resonance_budget.q0_default_resonance_db,
    }

ROLE_HZ_BOUNDS = {
    "anchor":   (30.0, 400.0),
    "mouth":    (400.0, 3500.0),
    "air":      (3500.0, HZ_HARD_MAX),
    "terminal": (3500.0, HZ_HARD_MAX),
}

CORNERS = [(0, 0), (1, 0), (0, 1), (1, 1)]
FAULT_MAP_GRID = 17

METRIC_KEYS = ["cutoff_hz", "resonance_db", "centroid_hz", "bass_db"]
HULL_MARGIN_LO = 0.7
HULL_MARGIN_HI = 1.3

@dataclass
class GuideResidual:
    guide: GuidePose
    achieved_cutoff_hz: float = 0.0
    achieved_resonance_db: float = 0.0
    achieved_centroid_hz: float = 0.0
    achieved_bass_db: float = 0.0
    cutoff_error: float = 0.0
    resonance_error: float = 0.0
    centroid_error: float = 0.0
    bass_error: float = 0.0
    rms_error: float = 0.0
    structural_failure: bool = False
    failure_reason: str = ""

@dataclass
class AutoFix:
    original: GuidePose
    fixed: GuidePose
    violations: list[str]
    description: str = ""

    def __repr__(self):
        return (f"AutoFix({self.original.label}: "
                + ", ".join(self.violations)
                + f" → {self.description})")

@dataclass
class ProjectionReport:
    converged: bool = False
    iterations: int = 0
    initial_loss: float = 0.0
    final_loss: float = 0.0
    elapsed_s: float = 0.0
    residuals: list[GuideResidual] = field(default_factory=list)
    structural_failures: int = 0
    feasible_count: int = 0
    auto_fixes_applied: int = 0
    message: str = ""

    def summarise(self) -> str:
        lines = [
            f"Projection: {'converged' if self.converged else 'did not converge'} "
            f"in {self.iterations} iters ({self.elapsed_s:.1f}s)",
            f"  loss: {self.initial_loss:.4f} -> {self.final_loss:.4f}",
        ]
        if self.auto_fixes_applied:
            lines.append(f"  auto-fixes applied: {self.auto_fixes_applied}")
        if self.structural_failures:
            lines.append(f"  structural failures: {self.structural_failures}")
        lines.append(f"  feasible/optimised: {self.feasible_count}")
        lines.append(f"  {self.message}")
        lines.append("")
        for r in self.residuals:
            label = r.guide.label or f"({r.guide.morph:.2f},{r.guide.q:.2f})"
            if r.structural_failure:
                lines.append(f"  FAIL {label}: {r.failure_reason}")
                continue
            targets = r.guide.active_targets()
            parts = []
            for t in targets:
                ach = getattr(r, f"achieved_{t}")
                err_key = f"{t.split('_')[0]}_error" if '_' in t else f"{t}_error"
                err = getattr(r, err_key)
                parts.append(f"{t}={ach:.1f} (err={err:.2f})")
            lines.append(f"  {label}: rms={r.rms_error:.3f}  " + "  ".join(parts))
        return "\n".join(lines)

def _probe_corners_vectorized(src: TrenchSrc, sr: float
                              ) -> dict[str, np.ndarray]:
    body = src.compile()
    data = {k: np.zeros(4) for k in METRIC_KEYS}
    data["unstable"] = np.zeros(4, dtype=bool)
    for i, (m, q) in enumerate(CORNERS):
        state = _probe_state(body, float(m), float(q), sr)
        if state is None or state["unstable"]:
            data["unstable"][i] = True
            continue
        for k in METRIC_KEYS:
            data[k][i] = state[k]
    return data

def _vectorized_range_check(targets: np.ndarray,
                            corner_lo: np.ndarray,
                            corner_hi: np.ndarray,
                            ) -> np.ndarray:
    lo = corner_lo[np.newaxis, :] * HULL_MARGIN_LO
    hi = corner_hi[np.newaxis, :] * HULL_MARGIN_HI
    return (targets >= lo) & (targets <= hi)

def _build_target_matrix(guides: list[GuidePose]
                         ) -> tuple[np.ndarray, list[int], list[list[int]]]:
    rows = []
    guide_map = []
    metric_map = []

    for gi, g in enumerate(guides):
        active = g.active_targets()
        if not active:
            continue
        row = []
        col_indices = []
        for mk in METRIC_KEYS:
            target_attr = f"target_{mk}"
            val = getattr(g, target_attr, None)
            if val is not None:
                row.append(val)
                col_indices.append(METRIC_KEYS.index(mk))
        if row:
            rows.append(row)
            guide_map.append(gi)
            metric_map.append(col_indices)

    if not rows:
        return np.zeros((0, len(METRIC_KEYS))), [], []

    M = len(METRIC_KEYS)
    mat = np.full((len(rows), M), np.nan)
    for i, row in enumerate(rows):
        for j, col_idx in enumerate(metric_map[i]):
            mat[i, col_idx] = row[j]

    return mat, guide_map, metric_map

def auto_fix(failed: list[GuideResidual], src: TrenchSrc,
             sr: float = DEFAULT_SR,
             profile: TrenchProfile | None = None) -> list[AutoFix]:
    b = _resolve_bounds(profile)
    if not failed:
        return []

    corner_data = _probe_corners_vectorized(src, sr)
    corner_lo = np.array([corner_data[k].min() for k in METRIC_KEYS])
    corner_hi = np.array([corner_data[k].max() for k in METRIC_KEYS])

    band_floor = HZ_HARD_MAX
    for v in src.voices:
        if v.m0_r > 0.01 or v.m100_r > 0.01:
            band = ROLE_HZ_BOUNDS.get(v.role, (HZ_HARD_MIN, HZ_HARD_MAX))
            band_floor = min(band_floor, band[0])

    body = src.compile()
    q0_cutoffs = {}
    for morph in [0.0, 0.25, 0.5, 0.75, 1.0]:
        state = _probe_state(body, float(morph), 0.0, sr)
        if state:
            q0_cutoffs[morph] = state["cutoff_hz"]

    fixes = []
    for res in failed:
        g = res.guide
        fixed_dict = {
            "morph": g.morph, "q": g.q, "label": g.label, "weight": g.weight,
        }
        violations = []
        desc_parts = []

        if g.target_cutoff_hz is not None:
            lo = corner_lo[METRIC_KEYS.index("cutoff_hz")] * HULL_MARGIN_LO * 1.01
            hi = corner_hi[METRIC_KEYS.index("cutoff_hz")] * HULL_MARGIN_HI * 0.99
            clamped = float(np.clip(g.target_cutoff_hz, lo, hi))
            clamped = max(clamped, band_floor)

            if g.q > 0.1 and q0_cutoffs:
                nearest_m = min(q0_cutoffs.keys(),
                                key=lambda m: abs(m - g.morph))
                q0_cut = q0_cutoffs[nearest_m]
                max_shift = q0_cut * (2.0 ** b["q_axis_max_octave_shift"])
                min_shift = q0_cut / (2.0 ** b["q_axis_max_octave_shift"])
                clamped = float(np.clip(clamped, min_shift, max_shift))

            if abs(clamped - g.target_cutoff_hz) > 0.5:
                violations.append("cutoff")
                desc_parts.append(
                    f"cutoff {g.target_cutoff_hz:.0f}→{clamped:.0f} Hz")
            fixed_dict["target_cutoff_hz"] = clamped
        else:
            fixed_dict["target_cutoff_hz"] = None

        if g.target_resonance_db is not None:
            hi = min(corner_hi[METRIC_KEYS.index("resonance_db")] + 12.0,
                     b["max_resonance_db"]) * 0.99
            clamped = float(np.clip(g.target_resonance_db, -60.0, hi))
            if abs(clamped - g.target_resonance_db) > 0.1:
                violations.append("resonance")
                desc_parts.append(
                    f"resonance {g.target_resonance_db:.1f}→{clamped:.1f} dB")
            fixed_dict["target_resonance_db"] = clamped
        else:
            fixed_dict["target_resonance_db"] = None

        for mk in ["centroid_hz", "bass_db"]:
            key = f"target_{mk}"
            val = getattr(g, key, None)
            if val is not None:
                idx = METRIC_KEYS.index(mk)
                lo = corner_lo[idx] * HULL_MARGIN_LO
                hi = corner_hi[idx] * HULL_MARGIN_HI
                clamped = float(np.clip(val, lo, hi))
                if abs(clamped - val) > 0.01 * abs(val) + 0.1:
                    violations.append(mk)
                    desc_parts.append(f"{mk} {val:.1f}→{clamped:.1f}")
                fixed_dict[key] = clamped
            else:
                fixed_dict[key] = None

        fixed = GuidePose(**fixed_dict)
        fixes.append(AutoFix(
            original=g,
            fixed=fixed,
            violations=violations,
            description="; ".join(desc_parts) if desc_parts else "no changes needed",
        ))

    return fixes

def apply_fixes(src: TrenchSrc, fixes: list[AutoFix]) -> TrenchSrc:
    s = copy.deepcopy(src)
    for fix in fixes:
        for i, g in enumerate(s.guide_poses):
            if (g.morph == fix.original.morph and g.q == fix.original.q
                    and g.label == fix.original.label):
                s.guide_poses[i] = fix.fixed
                break
    return s

class LintCache:

    def __init__(self):
        self._cache: dict[str, tuple[list, list, dict]] = {}
        self._hits = 0
        self._misses = 0

    def _hash_src(self, src: TrenchSrc) -> str:
        h = hashlib.sha256()
        for v in src.voices:
            h.update(f"{v.id}:{v.role}:{v.m0_hz:.4f}:{v.m0_r:.6f}:"
                     f"{v.m100_hz:.4f}:{v.m100_r:.6f}:{v.scale:.6f}:"
                     f"{v.m0_zero_hz:.4f}:{v.m0_zero_r:.6f}:"
                     f"{v.m100_zero_hz:.4f}:{v.m100_zero_r:.6f};".encode())
        for g in src.guide_poses:
            h.update(f"G:{g.morph:.6f}:{g.q:.6f}:{g.label}:"
                     f"{g.target_cutoff_hz}:{g.target_resonance_db}:"
                     f"{g.target_centroid_hz}:{g.target_bass_db}:"
                     f"{g.weight:.4f};".encode())
        for qa in src.q_attitudes:
            h.update(f"Q:{qa.lane}:{qa.behaviour}:{qa.arm_delta_r:.6f};".encode())
        return h.hexdigest()

    def get(self, src: TrenchSrc
            ) -> tuple[list[GuidePose], list[GuideResidual], dict] | None:
        key = self._hash_src(src)
        if key in self._cache:
            self._hits += 1
            return self._cache[key]
        self._misses += 1
        return None

    def put(self, src: TrenchSrc,
            passed: list[GuidePose],
            failed: list[GuideResidual],
            corner_data: dict):
        key = self._hash_src(src)
        self._cache[key] = (passed, failed, corner_data)

    def stats(self) -> str:
        total = self._hits + self._misses
        rate = self._hits / max(total, 1) * 100
        return f"LintCache: {self._hits}/{total} hits ({rate:.0f}%)"

    def clear(self):
        self._cache.clear()
        self._hits = 0
        self._misses = 0

def fault_map(src: TrenchSrc, sr: float = DEFAULT_SR,
              grid: int = FAULT_MAP_GRID,
              profile: TrenchProfile | None = None) -> str:
    body = src.compile()
    corner_data = _probe_corners_vectorized(src, sr)
    corner_lo = np.array([corner_data[k].min() for k in METRIC_KEYS])
    corner_hi = np.array([corner_data[k].max() for k in METRIC_KEYS])

    band_floor = HZ_HARD_MAX
    for v in src.voices:
        if v.m0_r > 0.01 or v.m100_r > 0.01:
            band = ROLE_HZ_BOUNDS.get(v.role, (HZ_HARD_MIN, HZ_HARD_MAX))
            band_floor = min(band_floor, band[0])

    sparse = max(1, grid // 4)
    grid_state = np.full((grid, grid), ".", dtype="<U4")

    for mi in range(0, grid, sparse):
        morph = mi / (grid - 1) if grid > 1 else 0.0
        for qi in range(0, grid, sparse):
            q = qi / (grid - 1) if grid > 1 else 0.0
            state = _probe_state(body, float(morph), float(q), sr)

            if state is None:
                grid_state[mi, qi] = "?"
                continue
            if state["unstable"]:
                grid_state[mi, qi] = "!"
                continue

            violations = []
            if state["cutoff_hz"] < corner_lo[0] * HULL_MARGIN_LO or \
               state["cutoff_hz"] > corner_hi[0] * HULL_MARGIN_HI:
                violations.append("C")
            if state["resonance_db"] > corner_hi[1] + 12.0:
                violations.append("R")
            if q > 0.1 and state["cutoff_hz"] < band_floor * 0.8:
                violations.append("B")

            if violations:
                grid_state[mi, qi] = violations[0]
            else:
                grid_state[mi, qi] = "·"

    for g in src.guide_poses:
        mi = int(g.morph * (grid - 1))
        qi = int(g.q * (grid - 1))
        mi = max(0, min(grid - 1, mi))
        qi = max(0, min(grid - 1, qi))
        label = (g.label or "G")[:4]
        grid_state[mi, qi] = label

    lines = ["Fault map (morph →, Q ↓):", ""]
    lines.append("  ·=feasible  C=cutoff  R=resonance  Q=Q-axis  B=band  !=unstable  ?=probe-fail")
    lines.append("  Labeled cells = guide pose positions")
    lines.append("")

    for qi in range(grid - 1, -1, -1):
        row = "  "
        for mi in range(grid):
            row += f"{grid_state[mi, qi]:4s}"
        if qi == grid - 1:
            row += "  Q=1.0"
        elif qi == grid // 2:
            row += "  Q=0.5"
        elif qi == 0:
            row += "  Q=0.0"
        lines.append(row)

    footer = "  "
    for mi in range(grid):
        if mi == 0:
            footer += "M0  "
        elif mi == grid - 1:
            footer += "M1  "
        elif mi == grid // 2:
            footer += "0.5 "
        else:
            footer += "    "
    lines.append(footer)

    violation_chars = {"C", "R", "Q", "B", "!", "?"}
    probed_cells = (grid // sparse) ** 2
    violation_cells = int(np.sum(np.isin(grid_state, list(violation_chars))))
    unstable_cells = int(np.sum(grid_state == "!"))
    lines.append("")
    lines.append(f"  {probed_cells} cells probed, {violation_cells} violations "
                 f"({unstable_cells} unstable)")

    return "\n".join(lines)

def lint_guides(src: TrenchSrc, sr: float = DEFAULT_SR,
                cache: LintCache | None = None,
                profile: TrenchProfile | None = None,
                ) -> tuple[list[GuidePose], list[GuideResidual]]:
    b = _resolve_bounds(profile)
    if cache is not None:
        cached = cache.get(src)
        if cached is not None:
            return cached[0], cached[1]

    corner_data = _probe_corners_vectorized(src, sr)
    if corner_data["unstable"].all():
        failures = [GuideResidual(
            guide=g, structural_failure=True,
            failure_reason="All corners unstable — cannot evaluate feasibility")
            for g in src.guide_poses]
        if cache:
            cache.put(src, [], failures, corner_data)
        return [], failures

    corner_lo = np.array([corner_data[k][~corner_data["unstable"]].min()
                          for k in METRIC_KEYS])
    corner_hi = np.array([corner_data[k][~corner_data["unstable"]].max()
                          for k in METRIC_KEYS])

    targets, guide_map, metric_map = _build_target_matrix(src.guide_poses)

    passed = []
    failed = []

    if len(targets) > 0:
        feasible_mask = _vectorized_range_check(targets, corner_lo, corner_hi)
        guide_pass = np.ones(len(src.guide_poses), dtype=bool)
        for row_idx, gi in enumerate(guide_map):
            active_cols = metric_map[row_idx]
            if active_cols:
                all_ok = all(feasible_mask[row_idx, c] for c in active_cols)
                if not all_ok:
                    guide_pass[gi] = False
                elif not active_cols:
                    pass

        band_floor = HZ_HARD_MAX
        role_names = set()
        for v in src.voices:
            if v.m0_r > 0.01 or v.m100_r > 0.01:
                band = ROLE_HZ_BOUNDS.get(v.role, (HZ_HARD_MIN, HZ_HARD_MAX))
                band_floor = min(band_floor, band[0])
                role_names.add(v.role)

        body = src.compile()
        q0_states = {}
        for morph in [0.0, 0.25, 0.5, 0.75, 1.0]:
            state = _probe_state(body, float(morph), 0.0, sr)
            if state:
                q0_states[morph] = state

        for gi, g in enumerate(src.guide_poses):
            if not guide_pass[gi]:
                reasons = []
                if g.target_cutoff_hz is not None:
                    ci = METRIC_KEYS.index("cutoff_hz")
                    if g.target_cutoff_hz < corner_lo[ci] * HULL_MARGIN_LO:
                        reasons.append(
                            f"I1 convex-hull: target cutoff={g.target_cutoff_hz:.0f} Hz "
                            f"below corner min {corner_lo[ci]:.0f} Hz")
                    elif g.target_cutoff_hz > corner_hi[ci] * HULL_MARGIN_HI:
                        reasons.append(
                            f"I1 convex-hull: target cutoff={g.target_cutoff_hz:.0f} Hz "
                            f"above corner max {corner_hi[ci]:.0f} Hz")
                if g.target_resonance_db is not None:
                    ri = METRIC_KEYS.index("resonance_db")
                    if g.target_resonance_db > corner_hi[ri] + 12.0:
                        reasons.append(
                            f"I1 convex-hull: target resonance={g.target_resonance_db:.1f} dB "
                            f"far above corner max {corner_hi[ri]:.1f} dB")
                failed.append(GuideResidual(
                    guide=g, structural_failure=True,
                    failure_reason="; ".join(reasons)))
                continue

            reasons = []

            if g.q > 0.1 and g.target_cutoff_hz is not None and q0_states:
                nearest_m = min(q0_states.keys(),
                                key=lambda m: abs(m - g.morph))
                q0_state = q0_states[nearest_m]
                octave_shift = abs(np.log2(
                    max(g.target_cutoff_hz, 1.0) / max(q0_state["cutoff_hz"], 1.0)))
                if octave_shift > b["q_axis_max_octave_shift"]:
                    reasons.append(
                        f"I2 Q-axis: cutoff shift {octave_shift:.1f} oct from q=0 "
                        f"(q=0 cutoff={q0_state['cutoff_hz']:.0f} Hz). "
                        f"Q attitude adjusts radius only — frequency is shared.")

            if g.target_resonance_db is not None and g.target_resonance_db > b["max_resonance_db"]:
                reasons.append(
                    f"I3 resonance-budget: target {g.target_resonance_db:.1f} dB "
                    f"exceeds engine cap of {b['max_resonance_db']:.0f} dB.")

            if g.target_cutoff_hz is not None and g.target_cutoff_hz < band_floor * 0.8:
                reasons.append(
                    f"I4 role-band: target cutoff={g.target_cutoff_hz:.0f} Hz "
                    f"below band floor {band_floor:.0f} Hz. "
                    f"Active roles: {role_names}.")

            if reasons:
                failed.append(GuideResidual(
                    guide=g, structural_failure=True,
                    failure_reason="; ".join(reasons)))
            else:
                passed.append(g)
    else:
        passed = list(src.guide_poses)

    if cache is not None:
        cache.put(src, passed, failed, corner_data)

    return passed, failed

def _pack_params(src: TrenchSrc) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    x0, lb, ub = [], [], []
    for vi, v in enumerate(src.voices):
        if v.m0_r <= 0.01 and v.m100_r <= 0.01:
            continue
        role_bounds = ROLE_HZ_BOUNDS.get(v.role, (HZ_HARD_MIN, HZ_HARD_MAX))
        hz_lo = max(role_bounds[0], HZ_HARD_MIN)
        hz_hi = min(role_bounds[1], HZ_HARD_MAX)

        x0.extend([v.m0_hz, v.m0_r])
        lb.extend([hz_lo, R_MIN])
        ub.extend([hz_hi, R_MAX])

        x0.extend([v.m100_hz, v.m100_r])
        lb.extend([hz_lo, R_MIN])
        ub.extend([hz_hi, R_MAX])

        delta = 0.0
        for qa in src.q_attitudes:
            if qa.lane == vi + 1:
                delta = qa.arm_delta_r if qa.behaviour == "arm" else 0.0
                break
        x0.append(delta)
        lb.append(0.0)
        ub.append(0.3)

    return np.array(x0), np.array(lb), np.array(ub)

def _unpack_into(x: np.ndarray, src: TrenchSrc) -> TrenchSrc:
    s = copy.deepcopy(src)
    idx = 0
    for vi, v in enumerate(s.voices):
        if v.m0_r <= 0.01 and v.m100_r <= 0.01:
            continue
        v.m0_hz = float(x[idx])
        v.m0_r = max(R_MIN, min(R_MAX, float(x[idx + 1])))
        v.m100_hz = float(x[idx + 2])
        v.m100_r = max(R_MIN, min(R_MAX, float(x[idx + 3])))
        arm_delta = max(0.0, min(0.3, float(x[idx + 4])))
        idx += 5

        lane_num = vi + 1
        found = False
        for qa in s.q_attitudes:
            if qa.lane == lane_num:
                qa.behaviour = "arm" if arm_delta > 0.001 else "flat"
                qa.arm_delta_r = arm_delta
                found = True
                break
        if not found and arm_delta > 0.001:
            s.q_attitudes.append(QAttitude(
                lane=lane_num, behaviour="arm", arm_delta_r=arm_delta))
    return s

def _make_objective(src_template: TrenchSrc, feasible_guides: list[GuidePose],
                    sr: float, bounds: dict[str, float] | None = None):
    guide_specs = []
    for g in feasible_guides:
        specs = {}
        if g.target_cutoff_hz is not None:
            specs["cutoff_hz"] = g.target_cutoff_hz
        if g.target_resonance_db is not None:
            specs["resonance_db"] = g.target_resonance_db
        if g.target_centroid_hz is not None:
            specs["centroid_hz"] = g.target_centroid_hz
        if g.target_bass_db is not None:
            specs["bass_db"] = g.target_bass_db
        guide_specs.append((g, specs))

    def objective(x: np.ndarray) -> float:
        src = _unpack_into(x, src_template)
        body = src.compile()
        total = 0.0
        total_weight = 0.0

        for g, specs in guide_specs:
            if not specs:
                continue
            state = _probe_state(body, float(g.morph), float(g.q), sr)
            if state is None:
                return 1e9
            w = g.weight
            total_weight += w * len(specs)

            if "cutoff_hz" in specs:
                target = specs["cutoff_hz"]
                err = np.log2(max(state["cutoff_hz"], 1.0) / max(target, 1.0))
                total += w * err * err
            if "resonance_db" in specs:
                target = specs["resonance_db"]
                err = (state["resonance_db"] - target) / max(abs(target), 1.0)
                total += w * err * err
            if "centroid_hz" in specs:
                target = specs["centroid_hz"]
                err = np.log2(max(state["centroid_hz"], 1.0) / max(target, 1.0))
                total += w * err * err
            if "bass_db" in specs:
                target = specs["bass_db"]
                err = (state["bass_db"] - target) / max(abs(target), 1.0)
                total += w * err * err

        for m, q in CORNERS:
            state = _probe_state(body, float(m), float(q), sr)
            if state is None:
                return 1e9
            max_res = b["max_resonance_db"]
            excess = max(0.0, state["resonance_db"] - max_res)
            total += b["corner_reg_weight"] * (excess / max_res) ** 2
            total_weight += b["corner_reg_weight"]
            if state["unstable"]:
                total += 100.0
                total_weight += 1.0

        if total_weight > 0:
            total /= total_weight
        return total

    return objective

def project_guides(src: TrenchSrc, sr: float = DEFAULT_SR,
                   max_iter: int = 300, tol: float = 1e-4,
                   auto_apply_fixes: bool = False,
                   cache: LintCache | None = None,
                   profile: TrenchProfile | None = None,
                   ) -> tuple[TrenchSrc, ProjectionReport]:
    b = _resolve_bounds(profile)

    if not src.guide_poses:
        return copy.deepcopy(src), ProjectionReport(
            converged=True, message="No guide poses — nothing to project.")

    feasible, structural_failures = lint_guides(src, sr, cache=cache, profile=profile)

    fixes_applied = 0
    if auto_apply_fixes and structural_failures:
        fixes = auto_fix(structural_failures, src, sr, profile=profile)
        src = apply_fixes(src, fixes)
        fixes_applied = len(fixes)
        feasible, structural_failures = lint_guides(src, sr, cache=cache, profile=profile)

    if not feasible:
        return copy.deepcopy(src), ProjectionReport(
            converged=True,
            structural_failures=len(structural_failures),
            feasible_count=0,
            auto_fixes_applied=fixes_applied,
            residuals=structural_failures,
            message="All guide targets failed pre-flight lint — "
                    "bilinear surface cannot reach any of them.")

    x0, lb, ub = _pack_params(src)

    if len(x0) == 0:
        return copy.deepcopy(src), ProjectionReport(
            converged=True,
            structural_failures=len(structural_failures),
            feasible_count=len(feasible),
            auto_fixes_applied=fixes_applied,
            residuals=structural_failures,
            message="No active voices to optimise.")

    objective = _make_objective(src, feasible, sr, bounds=b)
    initial_loss = objective(x0)

    t0 = time.time()
    result = minimize(
        objective, x0,
        method="Powell",
        bounds=list(zip(lb, ub)),
        options={"maxiter": max_iter, "ftol": tol, "xtol": 1e-3},
    )
    elapsed = time.time() - t0

    optimised = _unpack_into(result.x, src)
    optimised.guide_poses = copy.deepcopy(src.guide_poses)

    body = optimised.compile()
    residuals = list(structural_failures)

    for g in feasible:
        if not g.active_targets():
            continue
        state = _probe_state(body, float(g.morph), float(g.q), sr)
        res = GuideResidual(guide=g)
        if state is None:
            res.rms_error = 999.0
            res.structural_failure = True
            res.failure_reason = "post-optimisation probe failed (unstable)"
            residuals.append(res)
            continue

        errs = []
        if g.target_cutoff_hz is not None:
            res.achieved_cutoff_hz = state["cutoff_hz"]
            res.cutoff_error = float(np.log2(
                max(state["cutoff_hz"], 1.0) / max(g.target_cutoff_hz, 1.0)))
            errs.append(res.cutoff_error ** 2)
        if g.target_resonance_db is not None:
            res.achieved_resonance_db = state["resonance_db"]
            res.resonance_error = float(
                (state["resonance_db"] - g.target_resonance_db)
                / max(abs(g.target_resonance_db), 1.0))
            errs.append(res.resonance_error ** 2)
        if g.target_centroid_hz is not None:
            res.achieved_centroid_hz = state["centroid_hz"]
            res.centroid_error = float(np.log2(
                max(state["centroid_hz"], 1.0) / max(g.target_centroid_hz, 1.0)))
            errs.append(res.centroid_error ** 2)
        if g.target_bass_db is not None:
            res.achieved_bass_db = state["bass_db"]
            res.bass_error = float(
                (state["bass_db"] - g.target_bass_db)
                / max(abs(g.target_bass_db), 1.0))
            errs.append(res.bass_error ** 2)

        res.rms_error = float(np.sqrt(np.mean(errs))) if errs else 0.0
        residuals.append(res)

    report = ProjectionReport(
        converged=result.success,
        iterations=result.nit,
        initial_loss=initial_loss,
        final_loss=float(result.fun),
        elapsed_s=elapsed,
        residuals=residuals,
        structural_failures=len(structural_failures),
        feasible_count=len(feasible),
        auto_fixes_applied=fixes_applied,
        message=result.message,
    )

    return optimised, report
