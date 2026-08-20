import json, os, glob, shutil

AUTHORING_DIR = r"c:\Users\hooki\trench-authoring"
PLUGIN_DIR = r"c:\Users\hooki\trench-x3-clean"

# 1. Target bodies to bundle into TRENCH plugin
hero_presets = [
    # Vocal & Talking
    {"id": "talking_hedz",  "name": "TalkingHedz",    "cat": "Vocal & Talking", "src": "recipes/architectures/P2k_013_TalkingHedz.json"},
    {"id": "ooh_to_eee",    "name": "Ooh-To-Eee",     "cat": "Vocal & Talking", "src": "recipes/architectures/P2k_010_Ooh-To-Eee.json"},
    {"id": "ubu_orator",    "name": "UbuOrator",      "cat": "Vocal & Talking", "src": "recipes/architectures/P2k_021_UbuOrator.json"},
    {"id": "eeh_to_aah",    "name": "Eeh-To-Aah",     "cat": "Vocal & Talking", "src": "recipes/architectures/P2k_020_Eeh-To-Aah.json"},
    {"id": "multi_q_vox",   "name": "MultiQVox",      "cat": "Vocal & Talking", "src": "recipes/architectures/P2k_012_MultiQVox.json"},
    {"id": "deep_bouche",   "name": "DeepBouche",     "cat": "Vocal & Talking", "src": "recipes/architectures/P2k_022_DeepBouche.json"},
    {"id": "zoom_peaks",    "name": "ZoomPeaks",      "cat": "Vocal & Talking", "src": "recipes/architectures/P2k_014_ZoomPeaks.json"},
    
    # Acid & Bass Monsters
    {"id": "ace_of_bass",   "name": "Ace of Bass",    "cat": "Acid & Bass",     "src": "recipes/architectures/P2k_000_Ace_of_Bass.json"},
    {"id": "acid_ravage",   "name": "AcidRavage",     "cat": "Acid & Bass",     "src": "recipes/architectures/P2k_006_AcidRavage.json"},
    {"id": "tb_or_not_tb",  "name": "TB-OrNot-TB",    "cat": "Acid & Bass",     "src": "recipes/architectures/P2k_009_TB-OrNot-TB.json"},
    {"id": "boland_bass",   "name": "BolandBass",     "cat": "Acid & Bass",     "src": "recipes/architectures/P2k_011_BolandBass.json"},
    {"id": "bass_tracer",   "name": "BassTracer",     "cat": "Acid & Bass",     "src": "recipes/architectures/P2k_016_BassTracer.json"},
    {"id": "lucifers_q",    "name": "LucifersQ",      "cat": "Acid & Bass",     "src": "recipes/architectures/P2k_028_LucifersQ.json"},
    
    # Inharmonic Bells & Chimes
    {"id": "dead_ringer",   "name": "DeadRinger",     "cat": "Metallic & Bell", "src": "recipes/architectures/P2k_031_DeadRinger.json"},
    {"id": "klang_kling",   "name": "KlangKling",     "cat": "Metallic & Bell", "src": "recipes/architectures/P2k_032_KlangKling.json"},
    {"id": "rogue_hertz",   "name": "RogueHertz",     "cat": "Metallic & Bell", "src": "recipes/architectures/P2k_017_RogueHertz.json"},
    {"id": "razor_blades",  "name": "RazorBlades",    "cat": "Metallic & Bell", "src": "recipes/architectures/P2k_018_RazorBlades.json"},
    
    # All-Pass Phasers & Combs
    {"id": "tooth_comb",    "name": "ToothComb",      "cat": "Phaser & Comb",   "src": "recipes/architectures/P2k_030_ToothComb.json"},
    {"id": "ear_bender",    "name": "EarBender",      "cat": "Phaser & Comb",   "src": "recipes/architectures/P2k_029_EarBender.json"},
    {"id": "fuzzi_face",    "name": "FuzziFace",      "cat": "Phaser & Comb",   "src": "recipes/architectures/P2k_007_FuzziFace.json"},
    
    # Acoustic Bodies & Ladders
    {"id": "meaty_gizmo",   "name": "MeatyGizmo",     "cat": "Acoustic Ladder", "src": "recipes/architectures/P2k_004_MeatyGizmo.json"},
    {"id": "millennium",    "name": "Millennium",     "cat": "Acoustic Ladder", "src": "recipes/architectures/P2k_003_Millennium.json"},
    {"id": "klub_klassik",  "name": "KlubKlassik",    "cat": "Acoustic Ladder", "src": "recipes/architectures/P2k_005_KlubKlassik.json"},
    {"id": "cruz_pusher",   "name": "CruzPusher",     "cat": "Acoustic Ladder", "src": "recipes/architectures/P2k_008_CruzPusher.json"},
    
    # Spatial HRTF & Tone Shapers
    {"id": "radio_craze",   "name": "RadioCraze",     "cat": "Spatial & EQ",    "src": "recipes/architectures/P2k_019_RadioCraze.json"},
    {"id": "mega_sweepz",   "name": "MegaSweepz",     "cat": "Spatial & EQ",    "src": "recipes/architectures/P2k_001_MegaSweepz.json"},
    {"id": "dj_alkaline",   "name": "DJAlkaline",     "cat": "Spatial & EQ",    "src": "recipes/architectures/P2k_015_DJAlkaline.json"}
]

print(f"Exporting {len(hero_presets)} curated Hero Presets to TRENCH Plugin...")

# Ensure destination dirs exist
bodies_dst_dir = os.path.join(PLUGIN_DIR, "bodies")
os.makedirs(bodies_dst_dir, exist_ok=True)

# Copy matching .body240 binaries
exported = []
for p in hero_presets:
    # Find matching body240 in plots/inspector/
    prefix = p["src"].split("/")[-1].split(".")[0].lower() # e.g. p2k_013_talkinghedz
    # Search for .body240 in plots/inspector
    matches = glob.glob(f"plots/inspector/{prefix[:7]}*.body240")
    if matches:
        src_bin = matches[0]
        dst_bin = os.path.join(bodies_dst_dir, f"{p['id']}.body240")
        shutil.copyfile(src_bin, dst_bin)
        p["body_file"] = f"bodies/{p['id']}.body240"
        exported.append(p)
        print(f"  [OK] Exported {p['name']:20s} ({p['cat']:18s}) -> {p['id']}.body240")
    else:
        print(f"  [WARN] Missing binary for {p['name']}")

# Update catalogue metadata in plugin repo
catalogue_out = os.path.join(PLUGIN_DIR, "catalogue_hero_presets.json")
with open(catalogue_out, "w", encoding="utf-8") as f:
    json.dump({
        "contract": "TRENCH Shipped Preset Catalogue v1.0",
        "total_presets": len(exported),
        "presets": exported
    }, f, indent=2)

print(f"\nSUCCESS: Exported {len(exported)} verified hero bodies to {PLUGIN_DIR}\\bodies\\")
print(f"Catalogue manifest written to {catalogue_out}")
