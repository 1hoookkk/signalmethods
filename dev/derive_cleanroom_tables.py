import json, os, math

TABLES_DIR = "recipes/tables"
os.makedirs(TABLES_DIR, exist_ok=True)

# 1. Phonetic Vowel & Consonant Formant Table (Clean-Room Fant 1960 / Peterson-Barney / Klatt)
phonetic_table = {
    "schema": "trench-cleanroom-phonetic-v1",
    "description": "Standard IPA vowels and consonant formants derived from Fant (1960), Klatt (1980), and Peterson & Barney (1952)",
    "sampling_rate_hz": 44100.0,
    "vowels": {
        "i_fleece":  {"ipa": "/i/", "f1": 270,  "b1": 50,  "f2": 2290, "b2": 70,  "f3": 3010, "b3": 110, "f4": 3500, "b4": 180, "f5": 4500, "b5": 250},
        "e_dress":   {"ipa": "/e/", "f1": 530,  "b1": 60,  "f2": 1840, "b2": 90,  "f3": 2480, "b3": 120, "f4": 3600, "b4": 200, "f5": 4500, "b5": 250},
        "ae_trap":   {"ipa": "/æ/", "f1": 660,  "b1": 70,  "f2": 1720, "b2": 100, "f3": 2410, "b3": 140, "f4": 3600, "b4": 220, "f5": 4500, "b5": 250},
        "a_father":  {"ipa": "/a/", "f1": 730,  "b1": 80,  "f2": 1090, "b2": 90,  "f3": 2440, "b3": 130, "f4": 3500, "b4": 200, "f5": 4500, "b5": 250},
        "aw_thought":{"ipa": "/ɔ/", "f1": 570,  "b1": 70,  "f2": 840,  "b2": 80,  "f3": 2410, "b3": 120, "f4": 3500, "b4": 200, "f5": 4500, "b5": 250},
        "o_goat":    {"ipa": "/o/", "f1": 450,  "b1": 65,  "f2": 800,  "b2": 75,  "f3": 2300, "b3": 115, "f4": 3500, "b4": 200, "f5": 4500, "b5": 250},
        "u_goose":   {"ipa": "/u/", "f1": 300,  "b1": 55,  "f2": 870,  "b2": 80,  "f3": 2240, "b3": 110, "f4": 3500, "b4": 200, "f5": 4500, "b5": 250},
        "uh_strut":  {"ipa": "/ʌ/", "f1": 640,  "b1": 75,  "f2": 1190, "b2": 95,  "f3": 2390, "b3": 130, "f4": 3500, "b4": 200, "f5": 4500, "b5": 250},
        "er_nurse":  {"ipa": "/ɜː/","f1": 490,  "b1": 60,  "f2": 1350, "b2": 85,  "f3": 1690, "b3": 100, "f4": 3500, "b4": 200, "f5": 4500, "b5": 250},
        "y_french_u":{"ipa": "/y/", "f1": 270,  "b1": 50,  "f2": 1900, "b2": 80,  "f3": 2300, "b3": 110, "f4": 3500, "b4": 200, "f5": 4500, "b5": 250}
    },
    "slaved_valleys": {
        "rule": "Valley zero placed between F1 and F2 to carve inter-formant separation",
        "zero_r": 0.965
    }
}
with open(os.path.join(TABLES_DIR, "phonetic_formants.json"), "w", encoding="utf-8") as f:
    json.dump(phonetic_table, f, indent=2)

# 2. Inharmonic Metallic Bell & Resonator Ratios (Clean-Room Fletcher & Rossing / Risset)
bell_table = {
    "schema": "trench-cleanroom-bell-ratios-v1",
    "description": "Inharmonic modal frequency ratios for acoustic bells, chimes, and plates (Fletcher & Rossing)",
    "archetypes": {
        "church_carillon": {
            "name": "Church Carillon Bell",
            "intervals_st": [0.0, 12.0, 15.6, 19.2, 24.0, 28.5, 36.0],
            "ratios": [1.0, 2.0, 2.47, 3.03, 4.0, 5.2, 8.0],
            "mode_names": ["Hum", "Prime", "Tierce (Minor 3rd)", "Quint (Fifth)", "Nominal (Octave)", "Decim", "Double Octave"],
            "default_radii": [0.9996, 0.9992, 0.9988, 0.9982, 0.9975, 0.9965, 0.9950]
        },
        "gamelan_bronze_pot": {
            "name": "Gamelan Bonang Bronze Pot",
            "intervals_st": [0.0, 7.2, 13.1, 18.2, 21.3, 24.5, 27.2],
            "ratios": [1.0, 1.52, 2.14, 2.87, 3.42, 4.10, 4.88],
            "mode_names": ["Fund", "Mode 2", "Mode 3", "Mode 4", "Mode 5", "Mode 6", "Mode 7"],
            "default_radii": [0.9995, 0.9990, 0.9985, 0.9980, 0.9972, 0.9965, 0.9955]
        },
        "tubular_chime": {
            "name": "Tubular Orchestral Chime",
            "intervals_st": [0.0, 17.5, 29.2, 37.8, 44.8, 50.4, 55.2],
            "ratios": [1.0, 2.76, 5.40, 8.93, 13.34, 18.64, 24.8],
            "mode_names": ["Mode 1", "Mode 2", "Mode 3", "Mode 4", "Mode 5", "Mode 6", "Mode 7"],
            "default_radii": [0.9997, 0.9993, 0.9988, 0.9980, 0.9970, 0.9960, 0.9950]
        },
        "circular_plate_anvil": {
            "name": "Circular Steel Plate / Anvil",
            "intervals_st": [0.0, 12.7, 21.3, 27.6, 32.8, 37.1, 40.8],
            "ratios": [1.0, 2.08, 3.41, 4.95, 6.67, 8.56, 10.6],
            "mode_names": ["(0,1)", "(1,1)", "(2,1)", "(0,2)", "(3,1)", "(1,2)", "(4,1)"],
            "default_radii": [0.9994, 0.9988, 0.9982, 0.9975, 0.9968, 0.9960, 0.9950]
        }
    }
}
with open(os.path.join(TABLES_DIR, "inharmonic_bell_ratios.json"), "w", encoding="utf-8") as f:
    json.dump(bell_table, f, indent=2)

# 3. Acoustic Instrument Body Resonances Table (Clean-Room Wood / Chamber Physics)
body_table = {
    "schema": "trench-cleanroom-instrument-body-v1",
    "description": "Acoustic instrument body resonances, cavity modes, and bridge transfer functions",
    "instruments": {
        "cello_wooden_chamber": {
            "name": "Cello Chamber Body",
            "modes_hz": [
                {"name": "A0 Helmholtz Cavity", "fp": 120, "rp": 0.985, "fz": 150, "rz": 0.980},
                {"name": "T1 Top Plate Wood",   "fp": 180, "rp": 0.992, "fz": 210, "rz": 0.985},
                {"name": "B1 Back Plate Mode",  "fp": 240, "rp": 0.990, "fz": 285, "rz": 0.982},
                {"name": "C3 Chamber Air Mode", "fp": 350, "rp": 0.988, "fz": 410, "rz": 0.980},
                {"name": "Bridge Hill Peak",    "fp": 2200,"rp": 0.920, "fz": 2800,"rz": 0.900},
                {"name": "Wood Sheen Air",      "fp": 4500,"rp": 0.880, "fz": 6000,"rz": 0.850},
                {"name": "High Air Cap",        "fp": 16000,"rp":0.850, "fz": 0,   "rz": 0.000}
            ]
        },
        "acoustic_guitar_dreadnought": {
            "name": "Dreadnought Guitar Body",
            "modes_hz": [
                {"name": "A0 Soundhole Air",    "fp": 102, "rp": 0.990, "fz": 140, "rz": 0.985},
                {"name": "T1 Top Plate Flex",   "fp": 195, "rp": 0.994, "fz": 220, "rz": 0.990},
                {"name": "B1 Back Plate Flex",  "fp": 228, "rp": 0.992, "fz": 260, "rz": 0.985},
                {"name": "Soundboard Cross",    "fp": 380, "rp": 0.980, "fz": 450, "rz": 0.970},
                {"name": "Pick Presence Peak",  "fp": 3200,"rp": 0.910, "fz": 4200,"rz": 0.890},
                {"name": "Body Air Shelf",      "fp": 8500,"rp": 0.850, "fz": 11000,"rz":0.800},
                {"name": "High Air Cap",        "fp": 16000,"rp":0.850, "fz": 0,   "rz": 0.000}
            ]
        }
    }
}
with open(os.path.join(TABLES_DIR, "acoustic_body_resonances.json"), "w", encoding="utf-8") as f:
    json.dump(body_table, f, indent=2)

# 4. HRTF & Pinna Elevation Notches (MIT KEMAR / SONICOM Open Science)
hrtf_table = {
    "schema": "trench-cleanroom-hrtf-pinna-v1",
    "description": "Binaural elevation notches and ear canal resonances derived from MIT KEMAR & SONICOM open datasets",
    "elevation_grid_deg": [-30, -15, 0, 15, 30, 45, 60, 75, 90],
    "features": {
        "pinna_elevation_notch_1": {
            "description": "First pinna notch frequency tracks elevation theta",
            "formula_hz": "6200.0 + 45.0 * elevation_deg",
            "r_zero": 0.988
        },
        "pinna_elevation_notch_2": {
            "description": "Second pinna notch frequency",
            "formula_hz": "11500.0 + 35.0 * elevation_deg",
            "r_zero": 0.975
        },
        "ear_canal_resonance": {
            "frequency_hz": 3400.0,
            "r_pole": 0.935,
            "boost_db": 12.0
        },
        "torso_reflection_shelf": {
            "frequency_hz": 450.0,
            "r_pole": 0.880
        }
    }
}
with open(os.path.join(TABLES_DIR, "hrtf_pinna_notches.json"), "w", encoding="utf-8") as f:
    json.dump(hrtf_table, f, indent=2)

# 5. Phaser & Comb Filter Lattices Table
comb_table = {
    "schema": "trench-cleanroom-comb-lattices-v1",
    "description": "Lattice frequency placements for octave, harmonic, and all-pass phasers",
    "configurations": {
        "reciprocal_octave_comb": {
            "name": "Octave Comb Ladder",
            "stages": [
                {"pole_hz": 60,   "zero_hz": 120,  "r_zero": 1.0},
                {"pole_hz": 120,  "zero_hz": 240,  "r_zero": 1.0},
                {"pole_hz": 240,  "zero_hz": 480,  "r_zero": 1.0},
                {"pole_hz": 480,  "zero_hz": 960,  "r_zero": 1.0},
                {"pole_hz": 960,  "zero_hz": 1920, "r_zero": 1.0},
                {"pole_hz": 1920, "zero_hz": 3840, "r_zero": 1.0},
                {"pole_hz": 3840, "zero_hz": 7680, "r_zero": 1.0}
            ]
        },
        "circular_allpass_phaser": {
            "name": "12-Pole All-Pass Phaser",
            "stages": [
                {"pole_hz": 250,  "zero_hz": 500,  "r_zero": 0.995},
                {"pole_hz": 500,  "zero_hz": 1000, "r_zero": 0.995},
                {"pole_hz": 1000, "zero_hz": 2000, "r_zero": 0.995},
                {"pole_hz": 2000, "zero_hz": 4000, "r_zero": 0.995},
                {"pole_hz": 4000, "zero_hz": 8000, "r_zero": 0.995},
                {"pole_hz": 8000, "zero_hz": 16000,"r_zero": 0.995},
                {"pole_hz": 16000,"zero_hz": 0,    "r_zero": 0.000}
            ]
        }
    }
}
with open(os.path.join(TABLES_DIR, "phaser_comb_lattices.json"), "w", encoding="utf-8") as f:
    json.dump(comb_table, f, indent=2)

print("SUCCESS: Derived and generated 5 clean-room scientific tables in recipes/tables/")
