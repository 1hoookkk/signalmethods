#!/usr/bin/env python3
from __future__ import annotations

import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.trenchsrc import TrenchSrc, Voice, GuidePose, QAttitude
from tools.trench_profile import load_profile, list_profiles
from tools.guide_projection import (
    lint_guides, auto_fix, apply_fixes, project_guides,
    fault_map, LintCache, ProjectionReport,
)
from tools.surface_evaluator import evaluate_surface, _probe_state

DEFAULT_SR = 48000.0
OUTPUT_DIR = ROOT / "bodies" / "candidates"

GUIDE_BLUEPRINTS: dict[str, list[dict]] = {
    "sinkhole": [
        {"morph": 0.0, "q": 0.0, "label": "open",
         "cutoff_hz": 8000, "resonance_db": 2.0},
        {"morph": 1.0, "q": 0.0, "label": "closed",
         "cutoff_hz": 300, "resonance_db": 12.0},
        {"morph": 0.8, "q": 1.0, "label": "loud",
         "cutoff_hz": 400, "resonance_db": 20.0},
        {"morph": 0.2, "q": 1.0, "label": "smooth-open",
         "cutoff_hz": 5000, "resonance_db": 4.0},
    ],

    "tadpole": [
        {"morph": 0.0, "q": 0.0, "label": "sub-foundation",
         "cutoff_hz": 40, "resonance_db": 6.0},
        {"morph": 1.0, "q": 0.0, "label": "acid-open",
         "cutoff_hz": 2000, "resonance_db": 10.0},
        {"morph": 0.5, "q": 0.0, "label": "mid-sweep",
         "cutoff_hz": 300, "resonance_db": 8.0},
        {"morph": 0.0, "q": 1.0, "label": "sub-squelch",
         "cutoff_hz": 50, "resonance_db": 22.0},
        {"morph": 0.5, "q": 1.0, "label": "acid-squelch",
         "cutoff_hz": 600, "resonance_db": 24.0},
    ],

    "smalltalk": [
        {"morph": 0.0, "q": 0.0, "label": "vowel-aw",
         "cutoff_hz": 400, "resonance_db": 4.0},
        {"morph": 1.0, "q": 0.0, "label": "vowel-oui",
         "cutoff_hz": 200, "resonance_db": 6.0},
        {"morph": 0.5, "q": 0.0, "label": "mid-word",
         "cutoff_hz": 300, "resonance_db": 5.0},
        {"morph": 0.0, "q": 1.0, "label": "aw-spit",
         "cutoff_hz": 450, "resonance_db": 12.0},
        {"morph": 1.0, "q": 1.0, "label": "oui-spit",
         "cutoff_hz": 250, "resonance_db": 14.0},
    ],

    "drive_thru": [
        {"morph": 0.0, "q": 0.0, "label": "full-range",
         "cutoff_hz": 5000, "resonance_db": 2.0},
        {"morph": 1.0, "q": 0.0, "label": "cheap-radio",
         "cutoff_hz": 2500, "resonance_db": 8.0},
        {"morph": 0.5, "q": 1.0, "label": "broken",
         "cutoff_hz": 1500, "resonance_db": 15.0},
    ],

    "speaker_knockerz": [
        {"morph": 0.0, "q": 0.0, "label": "bass-cut",
         "cutoff_hz": 200, "resonance_db": 2.0},
        {"morph": 1.0, "q": 0.0, "label": "bass-boost",
         "cutoff_hz": 80, "resonance_db": 6.0},
        {"morph": 0.5, "q": 0.0, "label": "flat-mid",
         "cutoff_hz": 120, "resonance_db": 3.0},
        {"morph": 0.5, "q": 1.0, "label": "resonant-body",
         "cutoff_hz": 150, "resonance_db": 14.0},
    ],

    "fire_escape": [
        {"morph": 0.0, "q": 0.0, "label": "held-low",
         "cutoff_hz": 80, "resonance_db": 4.0},
        {"morph": 1.0, "q": 0.0, "label": "classic-open",
         "cutoff_hz": 4000, "resonance_db": 6.0},
        {"morph": 0.5, "q": 0.0, "label": "mid-rise",
         "cutoff_hz": 600, "resonance_db": 5.0},
        {"morph": 0.0, "q": 1.0, "label": "hot-sub",
         "cutoff_hz": 100, "resonance_db": 18.0},
        {"morph": 1.0, "q": 1.0, "label": "hot-open",
         "cutoff_hz": 3500, "resonance_db": 14.0},
    ],

    "nosebleed": [
        {"morph": 0.0, "q": 0.0, "label": "safe-zone",
         "cutoff_hz": 200, "resonance_db": 6.0},
        {"morph": 1.0, "q": 0.0, "label": "danger-zone",
         "cutoff_hz": 600, "resonance_db": 10.0},
        {"morph": 0.5, "q": 1.0, "label": "40-90-danger",
         "cutoff_hz": 400, "resonance_db": 35.0},
    ],

    "basement": [
        {"morph": 0.0, "q": 0.0, "label": "ou-front",
         "cutoff_hz": 500, "resonance_db": 4.0},
        {"morph": 1.0, "q": 0.0, "label": "est-back",
         "cutoff_hz": 250, "resonance_db": 6.0},
        {"morph": 0.0, "q": 1.0, "label": "ou-dark",
         "cutoff_hz": 450, "resonance_db": 12.0},
        {"morph": 1.0, "q": 1.0, "label": "est-dark",
         "cutoff_hz": 200, "resonance_db": 14.0},
    ],
}

PROFILE_MAP = {
    "sinkhole": "acid-resonant",
    "tadpole": "acid-resonant",
    "smalltalk": "vocal-formant",
    "drive_thru": "cab-comb",
    "speaker_knockerz": "eq-shaper",
    "fire_escape": "acid-resonant",
    "nosebleed": "acid-resonant",
    "basement": "vocal-formant",
}

CATEGORY_MAP = {
    "sinkhole": "CHARACTER",
    "tadpole": "CHARACTER",
    "smalltalk": "CHARACTER",
    "drive_thru": "CHARACTER",
    "speaker_knockerz": "CHARACTER",
    "fire_escape": "CHARACTER",
    "nosebleed": "CHARACTER",
    "basement": "CHARACTER",
}

def author_body(name: str, *, profile_name: str | None = None,
                sr: float = DEFAULT_SR, max_iter: int = 300):
    name_key = name.lower().replace(" ", "_").replace("-", "_")
    profile_name = profile_name or PROFILE_MAP.get(name_key, "acid-resonant")
    profile = load_profile(profile_name)
    category = CATEGORY_MAP.get(name_key, "CHARACTER")

    print(f"Authoring: {name}")
    print(f"  profile: {profile_name} — {profile.description[:80]}")
    print(f"  category: {category}")

    rng = np.random.default_rng(int(time.time() * 1e6) % (2**31))
    src = TrenchSrc.random_seed(name, category, rng=rng)

    if profile.voice_roles.bands:
        role_list = list(profile.voice_roles.bands.keys())
        for i, v in enumerate(src.voices):
            if i < len(role_list):
                v.role = role_list[i]
            if profile.voice_roles.required:
                if v.role in profile.voice_roles.required or i < len(
                        profile.voice_roles.required):
                    pass

    blueprint = GUIDE_BLUEPRINTS.get(name_key)
    if blueprint is None:
        print(f"  WARNING: no guide blueprint for '{name}' — using defaults")
    else:
        src.guide_poses = [
            GuidePose(
                morph=bp["morph"], q=bp["q"], label=bp["label"],
                target_cutoff_hz=bp.get("cutoff_hz"),
                target_resonance_db=bp.get("resonance_db"),
                weight=bp.get("weight", 1.0),
            )
            for bp in blueprint
        ]
        print(f"  guides: {len(src.guide_poses)} poses set")

    print()
    cache = LintCache()
    passed, failed = lint_guides(src, sr=sr, cache=cache, profile=profile)
    print(f"  lint: {len(passed)} pass, {len(failed)} structural failures")

    if failed:
        print("  auto-fixing structural failures:")
        fixes = auto_fix(failed, src, sr=sr, profile=profile)
        for fix in fixes:
            print(f"    {fix}")
        src = apply_fixes(src, fixes)
        passed, failed = lint_guides(src, sr=sr, cache=cache, profile=profile)
        print(f"  after fix: {len(passed)} pass, {len(failed)} remaining failures")

    print()
    print(fault_map(src, sr=sr, profile=profile))
    print()

    print("  projecting guides onto 4-corner surface...")
    result, report = project_guides(
        src, sr=sr, max_iter=max_iter, auto_apply_fixes=True,
        cache=cache, profile=profile,
    )
    print(report.summarise())

    body = result.compile()
    print(f"  compiled: {len(body)} bytes")

    metrics = evaluate_surface(body, sr=sr)
    print(f"  surface: cutoff_travel={metrics.cutoff_travel_octaves:.1f} oct, "
          f"resonance_change={metrics.resonance_change:.1f} dB, "
          f"axis_independence={metrics.axis_independence:.2f}, "
          f"unstable={metrics.unstable_states}")

    return result, body, report

def main():
    if len(sys.argv) < 2:
        print("Usage: python tools/author_body.py NAME [--profile PROFILE]")
        print()
        print("Available recipes:")
        for k in sorted(GUIDE_BLUEPRINTS.keys()):
            print(f"  {k}")
        print()
        print("Available profiles:")
        for p in list_profiles():
            print(f"  {p}")
        sys.exit(1)

    name = sys.argv[1]
    profile_name = None
    args = sys.argv[2:]
    i = 0
    while i < len(args):
        if args[i] == "--profile" and i + 1 < len(args):
            profile_name = args[i + 1]
            i += 2
        else:
            i += 1

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    src, body, report = author_body(name, profile_name=profile_name)

    stem = name.lower().replace(" ", "_").replace("-", "_")
    trenchsrc_path = OUTPUT_DIR / f"{stem}.trenchsrc"
    body_path = OUTPUT_DIR / f"{stem}.body240"

    src.to_yaml(trenchsrc_path)
    body_path.write_bytes(body)

    print()
    print(f"  .trenchsrc → {trenchsrc_path}")
    print(f"  .body240   → {body_path}")
    print(f"  Ready for VST3 audition.")

if __name__ == "__main__":
    main()
