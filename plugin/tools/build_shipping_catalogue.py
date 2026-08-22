from __future__ import annotations
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BODIES = ROOT / "presets_ship_v1" / "bodies"
OUT = ROOT / "presets_ship_v1" / "SHIPPING_CATALOGUE.json"

def sha256_hex(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

entries = []
order = 0

def add(id_, name, family, path, provenance, rate_policy, stages, validation,
        ships=True, exclusion=None):
    global order
    order += 1
    h = sha256_hex(path) if path and path.exists() else None
    entries.append({
        "id": id_,
        "name": name,
        "menu_order": order,
        "source_family": family,
        "source_path": str(path.relative_to(ROOT).as_posix()) if path else None,
        "source_sha256": h,
        "provenance": provenance,
        "sample_rate_policy": rate_policy,
        "active_stages": stages,
        "validation_level": validation,
        "ships": ships,
        "reason_for_exclusion": exclusion,
    })
    return entries[-1]

HERO = [
    ("sinkhole",       "Sinkhole",        "MegaSweepz recipe",           False),
    ("tadpole",        "Tadpole",         "BassBox 303 / TB or Not TB",  False),
    ("smalltalk",      "Smalltalk X→Y / Y→X", "Talking Hedz derivative", True),
    ("drive_thru",     "Drive Thru",      "Radio Craze recipe",          False),
    ("speaker_knockerz","Speaker Knockerz","Ace of Bass recipe",         False),
    ("moth",           "Moth",            "wah pedal",                   True),
    ("fire_escape",    "Fire Escape",     "Early Rizer recipe",          False),
    ("hollow",         "Hollow",          "Millennium",                  True),
    ("nosebleed",      "Nosebleed",       "Razor Blades / Lucifer's Q",  False),
    ("cul_de_sac",     "Cul-De-Sac",      "Bat Phaser ride (captured)",  True),
]

HERO_BODIES = {
    "smalltalk":   "shipv3_smalltalk.body240",
    "moth":        "MOTH.body240",
    "hollow":      "HOLLOW.body240",
}

DERIVATIVES = [
    ("deep",     "deep",     "Deep Bouche derivative",  True),
    ("tb",       "tb",       "TB-Or-Not-TB derivative", True),
    ("trillium", "trillium", "Millennium derivative",   True),
]

DERIVATIVE_BODIES = {
    "deep":     "shipv3_deep.body240",
    "tb":       "shipv3_tb.body240",
    "trillium": "shipv3_trillium.body240",
}

for id_, name, provenance, built in HERO:
    body_file = HERO_BODIES.get(id_)
    if body_file:
        path = ROOT / "bodies" / "candidates" / body_file
        if not path.exists():
            path = BODIES / body_file
        if not path.exists():
            path = None
    else:
        path = None

    if built and path:
        validation = "certified, ear-unproven"
        ships = True
    elif built and not path:
        validation = "body missing"
        ships = False
    else:
        validation = "recipe in INTENT, distance gate pending"
        ships = False

    add(id_, name, "HERO", path, provenance,
        "datum-anchored at 44,100 Hz (factory law)", 6,
        validation, ships=ships,
        exclusion=None if ships else "body not yet authored")

for id_, name, provenance, built in DERIVATIVES:
    body_file = DERIVATIVE_BODIES.get(id_)
    path = BODIES / body_file if body_file else None
    if path and not path.exists():
        path = None
    add(f"derived_{id_}", name, "DERIVATIVE", path, provenance,
        "datum-anchored at 44,100 Hz (factory law)", 6,
        "certified, ear-unproven" if path else "body missing",
        ships=built and path is not None,
        exclusion=None if (built and path) else "awaiting body/ears")

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps({
    "format": "trench-shipping-catalogue-v1",
    "note": "THE authoritative shipping manifest. Plugin menu reads ONLY from this file. "
            "Names verdicted 2026-08-03, binding authority: presets_ship_v1/DEFINITIVE_RAIL.md.",
    "total_entries": len(entries),
    "shipping_count": sum(1 for e in entries if e["ships"]),
    "families": {
        "HERO": "Hand-authored or captured artistic body, hero rail slot",
        "DERIVATIVE": "Derived from a ROM body via voice replacement (BUILD)",
    },
    "presets": entries,
}, indent=2))
print(f"Wrote {OUT}")
print(f"  {sum(1 for e in entries if e['source_family'] == 'HERO'):3d} Hero rail ({sum(1 for e in entries if e['source_family'] == 'HERO' and e['ships'])} shipping)")
print(f"  {sum(1 for e in entries if e['source_family'] == 'DERIVATIVE'):3d} Derivatives ({sum(1 for e in entries if e['source_family'] == 'DERIVATIVE' and e['ships'])} shipping)")
print(f"  {len(entries):3d} total ({sum(1 for e in entries if e['ships'])} shipping)")
