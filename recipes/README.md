# TRENCH Recipe System

Every body is built from three ingredients: a **frequency source**, a **stage
architecture**, and a **Q attitude**. This folder contains everything needed.

---

## Quick start — authoring a body

```
1. Pick a family     → tables/family_intents.json
2. Pick two intents  → same file (M0 and M100 poses)
3. Set bandwidths    → tables/vowel_formants.json or tables/klatt_1980_bandwidths.json
4. Author zeros      → INTENT.md (razor/valley/parked grammar)
5. Set Q attitude    → profiles/*.trenchprofile.json (per-family bounds)
6. Compile           → tools/trenchsrc.py → .body240
7. Ears              → the only shipping gate
```

---

## Files in this folder

```
recipes/
├── README.md
├── INTENT.md                          ← 33 ROM reference recipes + 8 hero recipes
│
├── tables/                            ← All measured data. No invented numbers.
│   ├── recipe_index_v1.json           ← 33 families R001-R033, per-lane statistical norms (2.7 MB)
│   ├── family_intents.json            ← 7 families × named intents × frequency slots
│   ├── vowel_formants.json            ← Peterson & Barney 1952 — F1/F2/F3 + bandwidths for 10 vowels
│   ├── klatt_1980_formants.json       ← Klatt 1980 + Mullen 2006 — F1-F4 for /a e o u/
│   ├── klatt_1980_bandwidths.json     ← Klatt 1980 Table I/II/III — B1/B2/B3, nasal pole/zero
│   ├── q_radius_table.json            ← Q index → pole radius (252 stages from 11 RE corpus files)
│   ├── p2k_filter_q_behavior.json     ← Per-filter Q behavior from E-MU P2K Owner's Manual pp. 105-107
│   ├── fundamentals_manifest.json     ← 17 X3F fundamentals — max_peak_db, max_pole_r, stability
│   ├── exact_skeletons.json           ← Analytic filter skeletons (Butterworth, Chebyshev, Elliptic)
│   ├── physical_models.py             ← Physics — Helmholtz, quarter-wave, membrane modes
│   ├── membrane_modes.json            ← Circular membrane modal frequencies (Bessel roots)
│   ├── metallic_modes.json            ← Rectangular plate modal frequencies (Chladni patterns)
│   ├── tube_resonances.json           ← Open/closed pipe resonance series
│   └── hrtf_pinna_P0001.json          ← HRTF pinna notch frequencies (measured IR)
│
├── profiles/                          ← Per-family validation constraints
│   ├── acid-resonant.trenchprofile.json    ← 40 dB ceiling, 20-12000 Hz, anchor required
│   ├── vocal-formant.trenchprofile.json    ← 30 dB ceiling, 100-5000 Hz, anchor+2×mouth required
│   ├── notch-zplane.trenchprofile.json     ← 12 dB ceiling, 50-15000 Hz, all optional
│   ├── eq-shaper.trenchprofile.json        ← 12 dB ceiling, 20-20000 Hz, all optional
│   └── cab-comb.trenchprofile.json         ← 10 dB ceiling, 30-12000 Hz, anchor required
│
└── reference/                         ← Legacy / reverse-engineering artifacts (not load-bearing)
    ├── emu_morph_designer_tool_law.json
    ├── emu_peakshelf_morph_fields.json
    ├── morph_designer_type_primitives.json
    ├── p2k_vocal_law.json
    ├── exact_skeletons.verification.json
    ├── ship_iron_mouth.profile-plan.json
    ├── ship_riot_plate.profile-plan.json
    ├── SHIP_ROSTER_PROFILE_REPORT.md
    └── README.md
```

---

## The recipe index — `tables/recipe_index_v1.json`

33 families (R001–R033) extracted from the P2K ROM corpus. Each family provides
per-lane statistical norms:

- **zero_to_pole_octaves** per pose — where zeros sit relative to their poles
- **pole_octaves_on_morph** — how far poles travel on the morph axis
- **scale_db_delta** per edge — how gain changes between corners
- **removed_rms_db** per pose — lane ablation impact
- **within_pose_rms_rank** — which lanes dominate at each corner

Contract: `recipeDoesNotSupplyPoleScaffolds: true` — families describe statistical
behavior, never prescribe exact Hz. Poles are frozen after fitting; zeros carry
all motion.

---

## The intent system — `tables/family_intents.json`

Seven families, each with intent slots and named frequency sets:

| Family | Slots | Intent count | Source |
|---|---|---|---|
| `vocal` | F1, F2, F3 | 10 vowels (ee/ih/eh/ae/ah/aw/uh/oo/uu/er) | Peterson & Barney 1952 |
| `cavity` | fundamental, second_mode, scoop_notch, upper_mode | 8 objects (bottles, jug, jar, can, pipe, tub) | Physics — Helmholtz + pipe modes |
| `resonant` | low/mid/hi/top_mode, scoop_notch | 7 metals (iron, glass, bell, scream, rattle, cluster, copper) | Physics — shell modes |
| `knock` | knock, body, grit, scoop_notch, tear | 7 percussive | Physics — impact spectra |
| `comb` | peak_low/high, notch_1–4 | 7 combs | Physics — delay/comb |
| `cut` | body_low/mid, scoop_notch, tear_lo/hi | 7 razor families | Physics — edge diffraction |
| `violence` | body, scream, scoop, tear, top_notch | 7 aggressive | Physics — distortion spectra |

Each intent is an exact frequency set with physical provenance (e.g.,
`vocal/ee` = [270, 2290, 3010] Hz from Peterson & Barney adult-male averages).

---

## Bandwidth — pole radius from Q

Three ways to set bandwidth (pole radius):

1. **Vowel bandwidths** — `tables/vowel_formants.json` has `bw1`/`bw2`/`bw3` (Hz)
   and `tables/klatt_1980_bandwidths.json` has `B1`/`B2`/`B3` (Hz).
   Convert: `r = exp(-π × BW / SR)`.

2. **Q radius table** — `tables/q_radius_table.json` maps Q index (0–104) to
   exact pole radius, measured from 252 stages across 11 RE corpus files.
   Most common: index 0 → r=0.986 (132 hits).

3. **Profile defaults** — each `.trenchprofile.json` has `q0_default_resonance_db`
   as a starting point (e.g., vocal-formant: 6.0 dB, acid-resonant: 8.0 dB).

---

## Q attitudes — from the ROM corpus

Eight documented behaviors, every one cited to exact measured Δr
(from `tools/q_attitudes.py` and INTENT.md):

| Profile | Δr per lane | Evidence |
|---|---|---|
| `flat` | All 0.0 | FuzziFace, DeepBouche, DJAlkaline |
| `single-bloomer` | One lane +0.15 to +0.36 | BassTracer S6 +0.36, Ace of Bass S6 +0.15 |
| `asymmetric-relay` | Arm lanes +0.22, disarm −0.66 | MegaSweepz S1/S6 +0.22, S2 −0.66 |
| `spare-the-air` | Mouth +0.04, air 0.0 | TalkingHedz, air pole at 4607 Hz spared |
| `uniform-gentle` | All +0.05 to +0.09 | AcidRavage (only uniform-positive Q in the set) |
| `all-negative` | All −0.02 to −0.07 | RazorBlades (more Q = deeper cuts) |
| `lurker-snap` | One lane r=0.65→1.0 at Q100 | BassTracer S6, BassOMatic S4, LucifersQ S6 |
| `revoice-tuner` | Poles shift Hz, radii unchanged | DJAlkaline, DreamWeava, RogueHertz |

---

## The binding rules (from design/)

- **Stage law** (`design/STAGE_LAW.md`) — 6 serial slots, lane identity preserved
  corner-to-corner, S6 unit-zero law, SCALE law
- **Slot grammar** (`design/SLOT_GRAMMAR.md`) — S1=HF frame, S2=main voice,
  S3–S4=mouth, S5=color, S6=terminal
- **ROM music theory** (`docs/ROM_MUSIC_THEORY.md`) — register architecture,
  voicing law, motion taxonomy, zero grammar
- **Bass fundamentals** (`docs/BASS_FUNDAMENTALS.md`) — cutoff targets,
  resonance budgets, voice role allocation, verification gates for bass bodies
- **P2K corpus bridge** (`docs/P2K_CORPUS_BRIDGE.md`) — measured resonance
  ceilings, cutoff travel, pole radius bounds per family

---

## The toolchain (in `tools/`)

| Tool | What it does |
|---|---|
| `recipe_pipeline.py` | End-to-end: measure → fit poles → assign lanes → author zeros → apply Q → certify → lint → compile |
| `recipe_families.py` | Load recipe index, suggest stage roles, classify targets |
| `q_attitudes.py` | Named Q profiles as per-lane Δr, CLI-parsable specs |
| `trenchsrc.py` | `.trenchsrc` format — named voices with pole/zero Hz+radius, compiles to `.body240` |
| `taste_linter.py` | 5 axioms: register architecture, intervallic tuning, motion taxonomy, zero grammar, Q attitude |
| `author_body.py` | Guide-pose → profile → project → compile |
| `make_body.py` | WAV/SOFA → certified `.body240` + audition renders |
| `inspect_body.py` | Full 6-stage cascade inspect plate |
