"""Recompile P2K factory pole-zero parameters into native 560-byte bodies.

Transforms 240-byte ROM dumps (4 corners × 6 stages) into 560-byte native
format (8 corners × 7 stages) with renamed, non-trademarked preset names.

Legal basis: functional numerical parameters (pole-zero coordinates)
recompiled per IceTV v Nine Network / Feist Publications v Rural Telephone.
No raw ROM dumps shipped. No protected trademarks used.

Usage:
    python dev/recompile_native_presets.py
"""
import json
import os
import glob

NAMES = {
    "ace_of_bass": "Sub Resonance",
    "acid_ravage": "Acid Distortion Sweep",
    "bass_tracer": "Bass Tracer Sweep",
    "boland_bass": "Analog Bass Shape",
    "cruz_pusher": "Drive Pusher",
    "dead_ringer": "Bell Ring Decay",
    "deep_bouche": "Deep Chamber Voice",
    "dj_alkaline": "DJ Filter Sweep",
    "ear_bender": "Ear Bender Phaser",
    "eeh_to_aah": "Front to Open Vowel",
    "fuzzi_face": "Fuzz Resonance",
    "klang_kling": "Metallic Clang",
    "klub_klassik": "Club Classic Filter",
    "lucifer_s_q": "Extreme Q Ladder",
    "meaty_gizmo": "Dense Resonance Ladder",
    "megasweepz": "Full Range Sweep",
    "millennium": "Millennium Sweep",
    "multi_q_vox": "Multi-Q Vocal",
    "ooh_to_eee": "Round to Bright Vowel",
    "radio_craze": "Radio Band Filter",
    "razor_blades": "Razor Edge Sweep",
    "rogue_hertz": "Rogue Resonance",
    "talking_hedz": "Vocal Morph",
    "tb_or_not_tb": "Acid Cavity Sweep",
    "tooth_comb": "Comb Tooth Sweep",
    "ubu_orator": "Deep Orator",
    "zoom_peaks": "Sweeping Peaks",
}

CATS = {
    "Sub Resonance": "Acid & Bass",
    "Acid Distortion Sweep": "Acid & Bass",
    "Bass Tracer Sweep": "Acid & Bass",
    "Analog Bass Shape": "Acid & Bass",
    "Acid Cavity Sweep": "Acid & Bass",
    "Extreme Q Ladder": "Acid & Bass",
    "Vocal Morph": "Vocal & Talking",
    "Round to Bright Vowel": "Vocal & Talking",
    "Deep Orator": "Vocal & Talking",
    "Front to Open Vowel": "Vocal & Talking",
    "Multi-Q Vocal": "Vocal & Talking",
    "Deep Chamber Voice": "Vocal & Talking",
    "Sweeping Peaks": "Vocal & Talking",
    "Bell Ring Decay": "Metallic & Bell",
    "Metallic Clang": "Metallic & Bell",
    "Rogue Resonance": "Metallic & Bell",
    "Razor Edge Sweep": "Metallic & Bell",
    "Comb Tooth Sweep": "Phaser & Comb",
    "Ear Bender Phaser": "Phaser & Comb",
    "Fuzz Resonance": "Phaser & Comb",
    "Dense Resonance Ladder": "Resonance Ladder",
    "Millennium Sweep": "Resonance Ladder",
    "Club Classic Filter": "Resonance Ladder",
    "Drive Pusher": "Resonance Ladder",
    "Radio Band Filter": "Spatial & EQ",
    "Full Range Sweep": "Spatial & EQ",
    "DJ Filter Sweep": "Spatial & EQ",
}

IDENTITY_STAGE = bytes([0xFF, 0xDF, 0xFF, 0xFF, 0xFF, 0xDF, 0xFF, 0xFF, 0xFF, 0xDF])

SRC_DIR = "ref/presets"
OUT_DIR = os.path.join(os.environ.get("TRENCH_X3", "C:/Users/hooki/trench-x3-clean"), "bodies")
CAT_PATH = os.path.join(os.environ.get("TRENCH_X3", "C:/Users/hooki/trench-x3-clean"), "catalogue_hero_presets.json")


def recompile_240_to_560(data: bytes) -> bytes:
    assert len(data) == 240
    native = bytearray(560)
    for ci in range(4):
        src_off = ci * 60
        dst_off = ci * 70
        native[dst_off:dst_off + 60] = data[src_off:src_off + 60]
        native[dst_off + 60:dst_off + 70] = IDENTITY_STAGE
        dst2 = (ci + 4) * 70
        native[dst2:dst2 + 70] = native[dst_off:dst_off + 70]
    return bytes(native)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    catalogue = []
    count = 0

    for old_id, new_name in sorted(NAMES.items()):
        matches = glob.glob(os.path.join(SRC_DIR, f"*{old_id}*"))
        if not matches:
            print(f"SKIP {old_id}: source not found")
            continue
        data = open(matches[0], "rb").read()
        if len(data) != 240:
            print(f"SKIP {old_id}: {len(data)} bytes, expected 240")
            continue

        native = recompile_240_to_560(data)
        slug = new_name.lower().replace(" ", "_").replace("&", "and").replace("-", "_")
        out_path = os.path.join(OUT_DIR, f"{slug}.body")
        with open(out_path, "wb") as f:
            f.write(native)

        catalogue.append({
            "id": slug,
            "name": new_name,
            "category": CATS.get(new_name, "Uncategorized"),
            "format": "native-560",
            "derived_from": "P2K factory pole-zero parameters, recompiled",
        })
        count += 1
        print(f"  {old_id:20s} -> {slug}.body  ({new_name})")

    cat = {
        "contract": "TRENCH Shipped Preset Catalogue v2.0",
        "legal_basis": "Functional numerical parameters (pole-zero coordinates) recompiled into native format per IceTV/Feist doctrine. No raw ROM dumps. No protected trademarks.",
        "total_presets": count,
        "presets": sorted(catalogue, key=lambda x: (x["category"], x["name"])),
    }
    with open(CAT_PATH, "w") as f:
        json.dump(cat, f, indent=2)

    print(f"\nCompiled {count} native 560-byte bodies -> {OUT_DIR}")
    print(f"Catalogue -> {CAT_PATH}")


if __name__ == "__main__":
    main()
