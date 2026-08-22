from __future__ import annotations
import json
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
RECIPE_INDEX = ROOT / "recipes" / "tables" / "recipe_index_v1.json"

FAMILY_CATEGORIES: dict[str, tuple[str, list[str]]] = {
    "R001": ("SWEEP",     ["parallel", "air-frame", "lpf"]),
    "R002": ("SWEEP",     ["parallel", "bass-anchor", "lpf"]),
    "R003": ("CHARACTER", ["contrary", "scoop-skeleton", "lpf"]),
    "R004": ("CHARACTER", ["contrary", "spectral-negative", "rez"]),
    "R005": ("TONE+EQ",   ["parallel", "formant-cluster", "lpf"]),
    "R006": ("BASS+ACID", ["parallel", "harmonic-ladder", "sub-engine", "lpf"]),
    "R007": ("CHARACTER", ["parallel", "unison-stack", "dst"]),
    "R008": ("PHASER+COMB",["contrary", "ring-selector", "rez"]),
    "R009": ("BASS+ACID", ["oblique", "register-jumper", "bass-cluster", "eq+"]),
    "R010": ("VOCAL",     ["oblique", "vowel", "mouth-formants"]),
    "R011": ("BASS+ACID", ["parallel", "vocabulary-reuse", "eq+"]),
    "R012": ("VOCAL",     ["oblique", "multi-formant", "negative-q"]),
    "R013": ("VOCAL",     ["oblique", "anchor-relay", "vowel-oui"]),
    "R014": ("PHASER+COMB",["contrary", "nasal-comb", "framing-relay"]),
    "R015": ("TONE+EQ",   ["oblique", "q-as-tuner", "isolator"]),
    "R016": ("BASS+ACID", ["oblique", "single-bloomer", "self-player"]),
    "R017": ("TONE+EQ",   ["oblique", "mid-bump", "smooth-q"]),
    "R018": ("CHARACTER", ["parallel", "all-negative-q", "subtraction"]),
    "R019": ("CHARACTER", ["parallel", "device-portrait", "negative-q"]),
    "R020": ("VOCAL",     ["oblique", "vowel-ee-ah", "reversal"]),
    "R021": ("VOCAL",     ["oblique", "vowel-ah-uuh"]),
    "R022": ("VOCAL",     ["oblique", "descending-formants", "french-vowels"]),
    "R023": ("CHARACTER", ["contrary", "freak-shifta"]),
    "R024": ("CHARACTER", ["oblique", "shared-pose"]),
    "R025": ("CHARACTER", ["contrary", "angelz-hairz"]),
    "R026": ("SWEEP",     ["oblique", "q-as-pitch", "dream-weava"]),
    "R027": ("SWEEP",     ["parallel", "re-pairing", "uniform-q"]),
    "R028": ("BASS+ACID", ["oblique", "lurker", "real-pole-cliff"]),
    "R029": ("CHARACTER", ["contrary", "adversarial-wiring", "lurker-birth"]),
    "R030": ("PHASER+COMB",["parallel", "re-pairing", "harmonic-comb"]),
    "R031": ("CHARACTER", ["contrary", "beating-pairs", "wah-vowel"]),
    "R032": ("CHARACTER", ["parallel", "klang-kling"]),
    "R033": ("WORKHORSE", ["parallel", "generic-lpf"]),
}

def load_index(path: Path | None = None) -> dict:
    p = path or RECIPE_INDEX
    with open(p) as f:
        return json.load(f)

def list_families(idx: dict | None = None) -> list[tuple[str, str, list[str]]]:
    if idx is None:
        idx = load_index()
    result = []
    for recipe in idx["recipes"]:
        rid = recipe["recipeId"]
        cat, tags = FAMILY_CATEGORIES.get(
            rid, ("WORKHORSE", ["unclassified"]))
        result.append((rid, cat, tags))
    return result

def family_norms(idx: dict, family_id: str) -> dict:
    for recipe in idx["recipes"]:
        if recipe["recipeId"] == family_id:
            break
    else:
        raise KeyError(f"Family {family_id} not found")

    result = {"family_id": family_id, "observation_count": recipe["observationCount"], "lanes": []}
    for lane_data in recipe["lanes"]:
        norms = {
            "lane": lane_data["lane"],
            "topology": _summarize_topology(lane_data.get("topologyByPose", [])),
            "zero_to_pole_octaves": _by_pose(
                lane_data.get("zeroBehaviorByPose", []),
                "representativeZeroToPoleOctaves"),
            "nearest_root_interval_octaves": _by_pose(
                lane_data.get("zeroBehaviorByPose", []),
                "nearestRootIntervalOctaves"),
            "removed_rms_db": _by_pose(
                lane_data.get("audibleEffectByPose", []), "removedRmsDb"),
            "within_pose_rms_rank": _by_pose(
                lane_data.get("audibleEffectByPose", []), "withinPoseRmsRank"),
            "lane_alone_span_db": _by_pose(
                lane_data.get("audibleEffectByPose", []), "laneAloneSpanDb"),
        }
        movements = {}
        for m in lane_data.get("relativeMovement", []):
            edge = m["edge"]
            movements[edge] = {
                "scale_db_delta": _unpack_stat(m.get("scaleDbDelta")),
                "pole_octaves": _unpack_stat(m.get("poleOctaves")),
                "zero_octaves": _unpack_stat(m.get("zeroOctaves")),
            }
        norms["movement"] = movements
        result["lanes"].append(norms)
    return result

def suggest_template(idx: dict, family_id: str) -> dict[int, str]:
    norms = family_norms(idx, family_id)
    roles = {}
    for ln in norms["lanes"]:
        lane = ln["lane"]
        rank = ln["within_pose_rms_rank"].get("M0_Q0", {}).get("median", lane)
        span = ln["lane_alone_span_db"].get("M0_Q0", {}).get("median", 70)

        if rank is None:
            roles[lane] = "air"
        elif rank <= 1.5 and span > 80:
            roles[lane] = "air"
        elif rank <= 2.5:
            roles[lane] = "mouth"
        elif rank >= 5:
            roles[lane] = "anchor"
        else:
            roles[lane] = "mouth"
    return roles

def classify_target(freqs: np.ndarray, dbs: np.ndarray,
                    idx: dict | None = None) -> str:
    if idx is None:
        idx = load_index()

    tilt = _spectral_tilt(freqs, dbs)
    peaks = _count_peaks(freqs, dbs)
    low_energy = _band_energy(freqs, dbs, 20, 400)
    mid_energy = _band_energy(freqs, dbs, 400, 3500)
    high_energy = _band_energy(freqs, dbs, 3500, 18000)

    scores = {}
    for rid, cat, tags in list_families(idx):
        score = 0.0
        if "lpf" in tags and tilt < -6:
            score += 2
        if "vowel" in tags and 2 <= peaks <= 5:
            score += 2
        if "parallel" in tags and abs(tilt) < 3:
            score += 1
        if "contrary" in tags and abs(tilt) > 6:
            score += 1
        if mid_energy > high_energy and "mouth-formants" in tags:
            score += 1
        if low_energy > mid_energy and "bass" in tags:
            score += 1
        scores[rid] = score

    return max(scores, key=scores.get)

def _unpack_stat(stat: dict | None) -> dict | None:
    if stat is None or stat.get("support", 0) == 0:
        return None
    return {"median": stat["median"], "min": stat["minimum"],
            "max": stat["maximum"], "support": stat["support"]}

def _by_pose(pose_list: list, key: str) -> dict:
    result = {}
    for entry in pose_list:
        pose = entry.get("pose", "unknown")
        stat = entry.get(key)
        unpacked = _unpack_stat(stat)
        if unpacked:
            result[pose] = unpacked
    return result

def _summarize_topology(poses: list) -> dict:
    result = {}
    for entry in poses:
        pose = entry.get("pose", "unknown")
        pole = entry.get("pole", {})
        zero = entry.get("zero", {})
        result[pose] = {
            "pole_mode": pole.get("mode", "unknown"),
            "zero_mode": zero.get("mode", "unknown"),
        }
    return result

def _spectral_tilt(freqs, dbs):
    if len(freqs) < 4:
        return 0.0
    log_f = np.log2(np.clip(freqs, 20, None))
    return float(np.polyfit(log_f, dbs, 1)[0])

def _count_peaks(freqs, dbs):
    from scipy.signal import find_peaks
    peaks, props = find_peaks(dbs, prominence=3, height=-20)
    return len(peaks)

def _band_energy(freqs, dbs, lo, hi):
    mask = (freqs >= lo) & (freqs <= hi)
    if not mask.any():
        return -60.0
    return float(dbs[mask].mean())
