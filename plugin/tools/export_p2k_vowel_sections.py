#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

from pyruntime.packed_interp import decode
from tools import p2k_structure as ps

ROOT = Path(__file__).resolve().parents[1]
OUT_JSON = ROOT / "dev" / "reference" / "p2k_vowel_isolated_sections_44100.json"
OUT_MD = ROOT / "dev" / "reference" / "p2k_vowel_isolated_sections_44100.md"

def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def rounded(value: float) -> float:
    return round(float(value), 9)

def pair_from_words(dm_word: int, dr_word: int) -> dict:
    dm, dr = decode(dm_word), decode(dr_word)
    product = 1.0 - dr
    linear = 4.0 * dm + dr - 2.0
    if linear == 0.0 and product == 0.0:
        return {"topology": "degenerate"}
    discriminant = linear * linear - 4.0 * product
    if discriminant >= 0.0:
        root = math.sqrt(discriminant)
        return {"topology": "real_pair",
                "roots": [rounded((-linear + root) / 2.0),
                          rounded((-linear - root) / 2.0)]}
    radius = math.sqrt(product)
    cosine = max(-1.0, min(1.0, -linear / (2.0 * radius)))
    return {"topology": "conjugate_pair",
            "hz": rounded(math.acos(cosine) / (2.0 * math.pi) * ps.P2K_BANK0_RATE),
            "r": rounded(radius)}

def pair_label(pair: dict) -> str:
    if pair["topology"] == "conjugate_pair":
        return f"{pair['hz']:.1f} Hz r {pair['r']:.6f}"
    if pair["topology"] == "real_pair":
        return f"real roots {pair['roots'][0]:+.6f}, {pair['roots'][1]:+.6f}"
    return "degenerate"

def main() -> None:
    presets = []
    for architecture_path in sorted(ps.ARCHITECTURES.glob("P2k_*.json")):
        architecture = json.loads(architecture_path.read_text(encoding="utf-8"))
        if architecture.get("x3_type") != "VOW":
            continue
        verified_path, architecture = ps.load_architecture(architecture_path)
        numeric_path, numeric_hash, rate_hash = ps._numeric_authority(architecture)
        rows = ps._body_rows(numeric_path)
        sections = []
        for stage in range(1, 7):
            corners = {}
            for corner in ps.POSES:
                words = rows[ps.POSES.index(corner)][stage - 1]
                scale = 4.0 * decode(words[4])
                corners[corner] = {
                    "words_u16": words,
                    "pole": pair_from_words(words[2], words[3]),
                    "zero": pair_from_words(words[0], words[1]),
                    "scale_linear": rounded(scale),
                    "scale_db": rounded(20.0 * math.log10(scale))
                }
            sections.append({"stage": stage, "corners": corners})
        order = {}
        for corner in ps.POSES:
            conjugate = [stage for stage in range(1, 7)
                         if sections[stage - 1]["corners"][corner]["pole"]["topology"] == "conjugate_pair"]
            non_conjugate = [stage for stage in range(1, 7) if stage not in conjugate]
            order[corner] = {
                "conjugate_stages_low_to_high": sorted(
                    conjugate,
                    key=lambda stage: sections[stage - 1]["corners"][corner]["pole"]["hz"]),
                "non_conjugate_stages_not_frequency_ranked": non_conjugate
            }
        dossier = ROOT / architecture["source"]
        presets.append({
            "id": f"P2k_{int(architecture['index']):03d}",
            "name": architecture["name"],
            "x3_type": architecture["x3_type"],
            "architecture": {
                "path": verified_path.relative_to(ROOT).as_posix(),
                "sha256": sha256(verified_path),
                "descriptive_dossier_datum_rate_hz": architecture["datum_sr_hz"]
            },
            "dossier": {
                "path": dossier.relative_to(ROOT).as_posix(),
                "sha256": sha256(dossier)
            },
            "numeric_authority": {
                "path": numeric_path.relative_to(ROOT).as_posix(),
                "sha256": numeric_hash,
                "bank": 0,
                "datum_rate_hz": ps.P2K_BANK0_RATE
            },
            "rate_authority": {
                "path": ps.RATE_MANIFEST_RELATIVE_PATH,
                "sha256": rate_hash
            },
            "stage_frequency_order_low_to_high_by_corner": order,
            "sections": sections
        })

    if len(presets) != 6:
        raise SystemExit(f"expected six VOW presets, found {len(presets)}")
    export = {
        "schema": "trench-p2k-isolated-sections-v1",
        "scope": "all presets whose canonical architecture x3_type is VOW",
        "corner_order": list(ps.POSES),
        "numeric_datum_rate_hz": ps.P2K_BANK0_RATE,
        "stage_identity_rule": (
            "Authored S1..S6 identity is preserved verbatim. Frequency-order lists are "
            "diagnostic only and must not be used to renumber or match stages."
        ),
        "isolation_rule": (
            "Each record is one decoded pole pair, zero pair, and SCALE from one stage/corner; "
            "the serial preset response is the product of its six section responses."
        ),
        "preset_count": len(presets),
        "isolated_section_corner_count": len(presets) * 6 * 4,
        "presets": presets
    }
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(json.dumps(export, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# P2K vowel presets — isolated 44.1 kHz sections",
        "",
        "Machine-readable authority: `p2k_vowel_isolated_sections_44100.json`.",
        "",
        "Stage numbers below are the authored stage identities. They are not frequency ranks. The",
        "low-to-high order is included only to make that fact inspectable.",
        "",
    ]
    for preset in presets:
        lines.extend([f"## {preset['id']} {preset['name']}", ""])
        lines.append("| Stage | M0_Q0 pole/zero | M100_Q0 pole/zero | M0_Q100 pole/zero | M100_Q100 pole/zero |")
        lines.append("|---:|---|---|---|---|")
        for section in preset["sections"]:
            cells = []
            for corner in ps.POSES:
                row = section["corners"][corner]
                cells.append(
                    f"P {pair_label(row['pole'])}; Z {pair_label(row['zero'])}; "
                    f"G {row['scale_db']:+.2f} dB"
                )
            lines.append(f"| S{section['stage']} | " + " | ".join(cells) + " |")
        lines.extend(["", "Low-to-high pole-stage order by corner:", ""])
        for corner, order in preset["stage_frequency_order_low_to_high_by_corner"].items():
            ranked = " < ".join(f"S{s}" for s in order["conjugate_stages_low_to_high"])
            real = ", ".join(f"S{s}" for s in order["non_conjugate_stages_not_frequency_ranked"])
            lines.append(f"- `{corner}` conjugate poles: {ranked or 'none'}"
                         + (f"; non-frequency-ranked: {real}" if real else ""))
        lines.append("")
    OUT_MD.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT_JSON.relative_to(ROOT)}")
    print(f"wrote {OUT_MD.relative_to(ROOT)}")
    print(f"{len(presets)} presets; {len(presets) * 6 * 4} isolated stage-corner records")

if __name__ == "__main__":
    main()
