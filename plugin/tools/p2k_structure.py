from __future__ import annotations

import hashlib
import json
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from pyruntime.packed_interp import decode  # noqa: E402
ARCHITECTURES = ROOT / "recipes" / "architectures"
INDEX_RELATIVE_PATH = "recipes/tables/recipe_index_v1.json"
INDEX_PATH = ROOT / INDEX_RELATIVE_PATH
INDEX_FORMAT = "trench-workstation-recipe-index-v1"
INDEX_SHA256 = "1c8164837b4348b0f5815cbd9f5efcaef03a0f6d839744c477de96fdf03f8b50"
PRESET_MANIFEST_PATH = ROOT / "ref" / "presets" / "P2K_MANIFEST.json"
RATE_MANIFEST_RELATIVE_PATH = "evidence/emulatorx_binary_filter_rip_20260729/P2K_RATE_BANK_MANIFEST.json"
RATE_MANIFEST_PATH = ROOT / RATE_MANIFEST_RELATIVE_PATH
MODEL = "isolated_p2k_conditioners_measured_lanes_v1"
METHOD = "isolated_p2k_conditioning_biquad"
POSES = ("M0_Q0", "M100_Q0", "M0_Q100", "M100_Q100")
AUTHORING_DATUM_RATE = 44_100.0
P2K_BANK0_RATE = 44_100.0
HZ_MAX = AUTHORING_DATUM_RATE * 0.49
POLE_R_MAX = 0.9999

def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def unresolved_structure(status: str = "unresolved") -> dict:
    if status not in ("unresolved", "legacy_import"):
        raise ValueError("structure status must be unresolved or legacy_import")
    return {
        "model": MODEL,
        "status": status,
        "authority": None,
        "authority_sha256": None,
        "numeric_authority": None,
        "numeric_authority_sha256": None,
        "rate_authority": RATE_MANIFEST_RELATIVE_PATH,
        "rate_authority_sha256": _sha256(RATE_MANIFEST_PATH),
        "recipe_index": INDEX_RELATIVE_PATH,
        "recipe_index_sha256": INDEX_SHA256,
        "conditioning_slots": [],
        "measured_slots": [],
        "source_datum_rate_hz": None,
        "authoring_datum_rate_hz": AUTHORING_DATUM_RATE,
    }

def list_architectures() -> list[tuple[str, str]]:
    out = []
    for path in sorted(ARCHITECTURES.glob("P2k_*.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        out.append((f"P2k_{int(data['index']):03d}", data["name"]))
    return out

def _architecture_path(ref: str | Path) -> Path:
    p = Path(ref)
    candidate = p if p.is_absolute() else ROOT / p
    if candidate.exists():
        path = candidate.resolve()
    else:
        token = str(ref).split()[0]
        hits = sorted(ARCHITECTURES.glob(f"{token}_*.json"))
        if len(hits) != 1:
            raise ValueError(f"conditioning architecture {ref!r} did not resolve uniquely")
        path = hits[0].resolve()
    try:
        path.relative_to(ARCHITECTURES.resolve())
    except ValueError as exc:
        raise ValueError("conditioning authority must live in recipes/architectures") from exc
    return path

def load_architecture(ref: str | Path) -> tuple[Path, dict]:
    path = _architecture_path(ref)
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("schema") != "trench-architecture-v1":
        raise ValueError(f"{path.name} is not a trench-architecture-v1 document")
    if len(data.get("sections", [])) != 6:
        raise ValueError(f"{path.name} must contain six registered sections")
    if not isinstance(data.get("datum_sr_hz"), (int, float)) or data["datum_sr_hz"] <= 0:
        raise ValueError(f"{path.name} has no valid source datum rate")
    return path, data

def _load_index() -> dict:
    if _sha256(INDEX_PATH) != INDEX_SHA256:
        raise ValueError("canonical recipe index hash changed")
    index = json.loads(INDEX_PATH.read_text(encoding="utf-8"))
    if index.get("format") != INDEX_FORMAT or len(index.get("recipes", [])) != 33:
        raise ValueError("canonical recipe index contract changed")
    return index

def _numeric_authority(architecture: dict) -> tuple[Path, str, str]:
    manifest = json.loads(PRESET_MANIFEST_PATH.read_text(encoding="utf-8"))
    rate_manifest = json.loads(RATE_MANIFEST_PATH.read_text(encoding="utf-8"))
    ref = f"P2k_{int(architecture['index']):03d}"
    entry = next((p for p in manifest.get("entries", []) if p.get("id") == ref), None)
    if entry is None or entry.get("main_variant") != 0:
        raise ValueError(f"P2K manifest has no canonical variant-0 bank for {ref}")
    path = (PRESET_MANIFEST_PATH.parent / entry["file"]).resolve()
    if len(path.read_bytes()) != 240:
        raise ValueError(f"{path.name} is not a 240-byte P2K corner bank")
    digest = _sha256(path)
    if digest != entry.get("sha256"):
        raise ValueError(f"{path.name} does not match the P2K manifest hash")
    if rate_manifest.get("rate_index_mapping", {}).get("0") != P2K_BANK0_RATE:
        raise ValueError("P2K rate manifest no longer maps bank 0 to 44.1 kHz")
    rate_entry = next((p for p in rate_manifest.get("entries", [])
                       if p.get("skin_index") == int(architecture["index"])), None)
    bank = next((b for b in (rate_entry or {}).get("banks", [])
                 if b.get("rate_index") == 0 and b.get("sample_rate_hz") == P2K_BANK0_RATE), None)
    if bank is None or bank.get("sha256") != digest:
        raise ValueError(f"P2K rate manifest does not identify {ref} bank 0 at 44.1 kHz")
    return path, digest, _sha256(RATE_MANIFEST_PATH)

def _topology(index: dict, architecture_index: int, slot: int, pose: str) -> tuple[str, str]:
    recipe_id = f"R{architecture_index + 1:03d}"
    recipe = next((r for r in index["recipes"] if r.get("recipeId") == recipe_id), None)
    if recipe is None:
        raise ValueError(f"recipe index has no {recipe_id} for architecture {architecture_index}")
    lane = recipe["lanes"][slot - 1]
    record = next((r for r in lane["topologyByPose"] if r.get("pose") == pose), None)
    if record is None:
        raise ValueError(f"{recipe_id} S{slot} has no topology record for {pose}")
    return record["pole"]["mode"], record["zero"]["mode"]

def _pair_from_words(dm: float, dr: float) -> tuple[str, float, float]:
    q = 1.0 - dr
    c = 4.0 * dm + dr
    p = c - 2.0
    if p == 0.0 and q == 0.0:
        return "degenerate", 0.0, 0.0
    disc = p * p - 4.0 * q
    if disc >= 0.0:
        return "real_pair", 0.0, 0.0
    radius = math.sqrt(q)
    cosine = max(-1.0, min(1.0, -p / (2.0 * radius)))
    hz = math.acos(cosine) / (2.0 * math.pi) * P2K_BANK0_RATE
    return "conjugate_pair", hz, radius

def _body_rows(path: Path) -> list[list[list[int]]]:
    words = struct.unpack("<120H", path.read_bytes())
    return [[list(words[(corner * 6 + slot) * 5:(corner * 6 + slot + 1) * 5])
             for slot in range(6)] for corner in range(4)]

def _section_geometry(rows: list[list[list[int]]], slot: int, pose: str) -> dict:
    words = rows[POSES.index(pose)][slot - 1]
    zero_mode, zero_hz, zero_r = _pair_from_words(decode(words[0]), decode(words[1]))
    pole_mode, pole_hz, pole_r = _pair_from_words(decode(words[2]), decode(words[3]))
    if (pole_mode, zero_mode) != ("conjugate_pair", "conjugate_pair"):
        raise ValueError(
            f"S{slot} {pose} is {pole_mode}/{zero_mode}; canonical geometry is "
            "conjugate-only, so it cannot be imported")
    geometry = {
        "pole": {"hz": pole_hz, "r": pole_r},
        "zero": {"hz": zero_hz, "r": zero_r},
        "scale": 4.0 * decode(words[4]),
        "words": words,
    }
    if not (0 <= geometry["pole"]["hz"] <= HZ_MAX
            and 0 <= geometry["zero"]["hz"] <= HZ_MAX
            and 0 <= pole_r <= POLE_R_MAX and 0 <= zero_r <= 1.0
            and 0 < geometry["scale"] <= 4.0):
        raise ValueError(f"S{section['slot']} {pose} cannot be represented at 44.1 kHz")
    return geometry

def apply_conditioners(doc: dict, architecture_ref: str | Path,
                       slots: list[int]) -> int:
    slots = sorted(set(int(slot) for slot in slots))
    if not slots or any(slot < 1 or slot > 6 for slot in slots):
        raise ValueError("conditioning slots must be one or more stage numbers S1-S6")
    path, architecture = load_architecture(architecture_ref)
    index = _load_index()
    numeric_path, numeric_sha256, rate_authority_sha256 = _numeric_authority(architecture)
    rows = _body_rows(numeric_path)
    source_rate = P2K_BANK0_RATE
    proposals = []
    for slot in slots:
        plan = doc["stage_plan"][slot - 1]
        assignments = doc["lanes"][plan["lane_id"]]["assignments"]
        if any(a.get("state") != "identity" for a in assignments.values()):
            raise ValueError(
                f"S{slot} already carries measured/authored geometry; conditioning "
                "donors never overwrite it")
        section = architecture["sections"][slot - 1]
        if section.get("slot") != slot:
            raise ValueError(f"{path.name} section order is not S1-S6")
        corner_rows = {}
        for pose in POSES:
            modes = _topology(index, int(architecture["index"]), slot, pose)
            if modes != ("conjugate_pair", "conjugate_pair"):
                raise ValueError(
                    f"{path.stem} S{slot} {pose} is {modes[0]}/{modes[1]}; "
                    "canonical geometry is conjugate-only, so it cannot be imported")
            corner_rows[pose] = _section_geometry(rows, slot, pose)
        proposals.append((slot, plan, assignments, corner_rows))

    for slot, plan, assignments, corner_rows in proposals:
        plan["role"] = f"{architecture['name']} S{slot} conditioning donor"
        plan["law"] = "free"
        plan["pole_zero_relation"] = "isolated P2K conditioning section; numeric scope limited to this lane"
        plan["limits"] = {"max_pole_octave_step": None,
                          "max_radius_step": None,
                          "max_scale_step_db": None}
        for pose in POSES:
            row = corner_rows[pose]
            assignments[pose] = {
                "state": "active",
                "candidate": {
                    "catalog_record": None,
                    "selector": {"architecture": path.name, "slot": slot, "corner": pose,
                                 "words_u16": row["words"]},
                },
                "pole": row["pole"],
                "zero": row["zero"],
                "scale": row["scale"],
                "provenance": {
                    "source": numeric_path.relative_to(ROOT).as_posix(),
                    "architecture": path.relative_to(ROOT).as_posix(),
                    "rate_authority": RATE_MANIFEST_RELATIVE_PATH,
                    "method": METHOD,
                    "source_datum_rate_hz": source_rate,
                    "authoring_datum_rate_hz": AUTHORING_DATUM_RATE,
                    "rate_treatment": "exact 44100 bank; no rate or radius conversion",
                },
            }

    measured_slots = []
    for plan in doc["stage_plan"]:
        slot = plan["slot"] + 1
        if slot in slots:
            continue
        assignments = doc["lanes"][plan["lane_id"]]["assignments"]
        if any(a.get("state") == "active" for a in assignments.values()):
            measured_slots.append(slot)
    doc["structure"] = {
        "model": MODEL,
        "status": "applied",
        "authority": path.relative_to(ROOT).as_posix(),
        "authority_sha256": _sha256(path),
        "numeric_authority": numeric_path.relative_to(ROOT).as_posix(),
        "numeric_authority_sha256": numeric_sha256,
        "rate_authority": RATE_MANIFEST_RELATIVE_PATH,
        "rate_authority_sha256": rate_authority_sha256,
        "recipe_index": INDEX_RELATIVE_PATH,
        "recipe_index_sha256": INDEX_SHA256,
        "conditioning_slots": slots,
        "measured_slots": measured_slots,
        "source_datum_rate_hz": source_rate,
        "authoring_datum_rate_hz": AUTHORING_DATUM_RATE,
    }
    return len(proposals) * len(POSES)

def validation_errors(doc: dict) -> list[str]:
    structure = doc.get("structure")
    if not isinstance(structure, dict) or structure.get("status") != "applied":
        return []
    errors = []
    try:
        path, architecture = load_architecture(structure.get("authority"))
        if structure.get("authority_sha256") != _sha256(path):
            errors.append("structure.authority_sha256: conditioning authority changed")
        if structure.get("recipe_index_sha256") != INDEX_SHA256:
            errors.append("structure.recipe_index_sha256: canonical recipe index changed")
        numeric_path, numeric_sha256, rate_authority_sha256 = _numeric_authority(architecture)
        if structure.get("numeric_authority") != numeric_path.relative_to(ROOT).as_posix():
            errors.append("structure.numeric_authority: wrong P2K numeric bank")
        if structure.get("numeric_authority_sha256") != numeric_sha256:
            errors.append("structure.numeric_authority_sha256: P2K numeric bank changed")
        if structure.get("rate_authority") != RATE_MANIFEST_RELATIVE_PATH:
            errors.append("structure.rate_authority: wrong P2K rate evidence")
        if structure.get("rate_authority_sha256") != rate_authority_sha256:
            errors.append("structure.rate_authority_sha256: P2K rate evidence changed")
        rows = _body_rows(numeric_path)
        for slot in structure.get("conditioning_slots", []):
            plan = doc["stage_plan"][slot - 1]
            assignments = doc["lanes"][plan["lane_id"]]["assignments"]
            section = architecture["sections"][slot - 1]
            for pose in POSES:
                want = _section_geometry(rows, slot, pose)
                got = assignments[pose]
                path_label = f"lanes.{plan['lane_id']}.assignments.{pose}"
                if got.get("state") != "active" or got.get("pole") != want["pole"] \
                        or got.get("zero") != want["zero"] \
                        or not math.isclose(got.get("scale", 0), want["scale"], rel_tol=0, abs_tol=1e-12):
                    errors.append(f"{path_label}: isolated conditioning geometry changed")
                if got.get("provenance", {}).get("method") != METHOD:
                    errors.append(f"{path_label}.provenance: conditioning donor method missing")
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
        errors.append(f"structure: cannot verify conditioning authority ({exc})")
    return errors

def require_applied(doc: dict) -> None:
    structure = doc.get("structure")
    if not isinstance(structure, dict) or structure.get("status") != "applied":
        status = structure.get("status") if isinstance(structure, dict) else "missing"
        raise ValueError(
            f"conditioning structure is {status!r}: explicitly add the isolated "
            "P2K conditioning lane(s) before geometry emission or packing")
    errors = validation_errors(doc)
    if errors:
        raise ValueError("conditioning structure is stale or incompatible:\n" + "\n".join(errors))
