# TRENCH Corpus Findings

Only claims supported by decoded data from the 33 verified P2K dossiers
(`dossiers/characters/P2k_*.json`, datum 39062.5 Hz). Every numerical
claim names the generating script or data file.

---

## 1. Shared pose vocabulary — verbatim byte reuse

**Data**: `scratchpad/rom_music_theory.py` over `dossiers/characters/P2k_*.json`

Identical section rows recur across characters at the packed-word level:

| Shared element | Appears verbatim in |
|---|---|
| 79 Hz sub pole | BolandBass, LucifersQ, BassTracer, BassBox-303 (M0) |
| 479 + 13367 Hz pole pair | BolandBass, LucifersQ, BassTracer, BassBox-303 (M0) |
| 17.6 kHz air pole | BolandBass, LucifersQ, BassTracer, BassBox-303 (M0) |
| Entire Q0 corners | Millennium = MeatyGizmo (word-identical) |
| Entire M0 pose | KlubKlassik = AcidRavage (word-identical) |
| Entire M0 pose | BolandBass = LucifersQ (word-identical) |
| Entire M0 pose | CruzPusher = FuzziFace (word-identical) |
| TalkingHedz soft mouth | = UbuOrator destination corner |
| DeadRinger M100 | lands on Ooh mouth formant structure |

## 2. Register census

**Data**: `scratchpad/rom_music_theory.py` band classification

130 pole-zero pairs measured across all 33 M0 corners:

| Band | Hz range | Count | % |
|---|---|---|---|
| Below 400 Hz | anchor | 22 | 16.9% |
| 400 Hz – 3.5 kHz | mouth | 57 | 43.8% |
| Above 3.5 kHz | air | 84 | 64.6% |

Note: percentages exceed 100% because some stages have multiple voice
classifications.

## 3. Adjacent-voice spacing (M0 corners)

**Data**: `scratchpad/rom_music_theory.py` interval histogram

Spacing between adjacent sorted pole frequencies across all 33 M0 corners.
Mass sits at 1, 3, 4, and 5 semitones. Octave (12 st) is the consonant
fallback. Perfect fifths exist but do not dominate.

Median worst distance from equal-tempered grid: ~50 cents.

## 4. Motion taxonomy — measured per-filter

**Data**: `scratchpad/rom_music_theory.py` lane-tracking over M0→M100

Classified per filter from index-paired pole travel in semitones:

**Contrary (18/33)**: Registers trade places. One voice rises while
another falls.

| Filter | Max rise | Max fall | Crossing pairs |
|---|---|---|---|
| Millennium | +77 st | −62 st | 12 |
| BolandBass | +83 st | −68 st | — |
| EarBender | +53 st | −51 st | 9 |
| TalkingHedz | +38 st | −26 st | moderate |
| LucifersQ | +83 st (S1) | −67 st (S2) | 8 |

**Parallel (7/33)**: Every voice slides the same direction.

| Filter | Range |
|---|---|
| MegaSweepz | −13 to −42 st |
| DeadRinger | ≈ −22 st |
| AcidRavage | +6 to +28 st |
| RazorBlades | −20 to −29 st |
| FuzziFace | exactly +12 st (5 of 5 stages) |
| Ace of Bass | −8 to −34 st |

**Oblique (5/33)**: Most voices hold, one or two speak.

| Filter | Held | Moving |
|---|---|---|
| DeepBouche | 4 of 6 | S1 +14 st |
| TalkingHedz | 3 of 6 | S6 +38 st |
| MultiQVox | 3 of 6 | S2 −32 st, S6 +27 st |

## 5. Zero-pole spacing — measured intervals

**Data**: `scratchpad/rom_music_theory.py` zero-pole pair analysis

130 pole-zero pairs measured. Zero interval from its stage's pole:

| Category | Interval | Count | % |
|---|---|---|---|
| Razor | within ±2 st | ~26 | ~20% |
| Valley | +3 to +10 st | majority | — |
| Parked | ≈ +58 st | 8 | ~6% |
| Deep-drop | −20 to −65 st | rare | — |

## 6. Q-axis radius changes

**Data**: `scratchpad/rom_music_theory.py` Q0 vs Q100 comparison

Per-stage radius changes from Q0 to Q100:

- 38% of stages: push pole radii toward ceiling (~66 dB R′)
- 62% of stages: re-voice to new notes, retune, stay flat, or back off

| Filter | Q behaviour (measured Δr) |
|---|---|
| Ace of Bass | S6 +0.15 only |
| MegaSweepz | S1 +0.23, S6 +0.22, S2 −0.66 |
| Millennium | S3 +0.21, S6 −0.62 |
| TB-Or-Not-TB | S1 −0.09, S2 −0.15 |
| RazorBlades | all negative −0.02 to −0.07 |
| BassTracer | S6 +0.36 (largest single lift) |
| TalkingHedz | +0.01 to +0.05 (uniform gentle) |
| AcidRavage | +0.05 to +0.09 (uniform positive) |

## 7. S6 unit-zero terminal law

**Data**: `tools/corpus_report.py` (2026-08-04)

127 of 132 measured stages have S6 zero at r ≈ 1.0 (unit circle), acting
as a broadband tilt platform rather than a voiced zero.

## 8. SCALE = corner gain

**Data**: `tools/corpus_report.py` (2026-08-04)

26 of 33 bodies: SCALE operates as corner-level broadband gain only.
Does not change spectral contrast.

## 9. Destination re-pairing — measured example

**Data**: KlubKlassik vs AcidRavage dossiers

Identical M0 pose. M100 destinations are the same set of pole frequencies
but wired to different sections. Result: KlubKlassik has crossings;
AcidRavage has zero crossings (smooth parallel travel through M50).

## 10. Corner reversal

**Data**: Ooh-To-Eee vs Eeh-To-Aah dossiers

Eeh-To-Aah M0 = Ooh-To-Eee M100, and vice versa. Literal corner swap,
not re-authored.
