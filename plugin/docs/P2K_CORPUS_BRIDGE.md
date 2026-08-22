# P2K Corpus Bridge — Measured Hardware → Profile Bounds

Measured 2026-08-04. 50 P2K ROM presets decoded through `trench_core` at 44.1 kHz.
All data from `analysis/factory_topology_20260804/factory_preset_summary.csv`.

## Resonance ceiling by family

| Family | P2K range (dB) | P2K max preset | Profile cap |
|---|---|---|---|
| Acid / resonant LPF | 5–39 | LucifersQ (39.0) | 40 dB |
| Vocal / formant | 8–28 | TalkingHedz (28.0) | 30 dB |
| EQ shape / bass boost | 3–30 | RadioCraze (29.7) | 30 dB |
| Phaser / notch / comb | 2–17 | CruzPusher (17.0) | 20 dB |
| Workhorse LPF/HPF/BPF | 0–6 | — | 12 dB |

The 24 dB TRENCH engine cap is a **safety layer**, not a musical limit.
Profiles permit targets above 24 dB; the engine soft-clamps at runtime.

## Pole radius

All 12th-order presets: max pole radius **0.997–0.999**.
Workhorse filters: r=1.0 for identity/near-identity sections.
Profile clamp: r_max = 0.999 (stable, never on unit circle).

## Cutoff travel (morph axis)

| Family | Min cutoff | Max cutoff | Travel |
|---|---|---|---|
| Bass processors (AceOfBass, BassTracer, BolandBass) | 20 Hz | 20 Hz | 0 oct |
| Sweeping LPF (MegaSweepz, EarlyRizer, Millennium) | 20 Hz | 650 Hz | 5 oct |
| Classic 4-pole LPF | 21 Hz | 20000 Hz | 10 oct |
| Vocal formant | 20 Hz | 20 Hz | 0 oct |

Bass processors and vocal formants have **zero cutoff travel** — morph changes
character (peak count, spectral tilt), not cutoff frequency. The morph axis
semantic is NOT "cutoff sweep" for these families.

## Peak counts

| Family | Peaks (mid-morph, mid-Q) |
|---|---|
| Vocal formant (6-stage) | 6–7 |
| Ring ladder / metallic | 2–4 |
| Wide sweep LPF | 3–4 |
| Bass processors | 2–4 |
| Workhorse 2/4/6-pole | 2–7 (varies by order) |

## Bass energy (30–200 Hz at mid-morph, mid-Q)

Most presets: −2 to +1 dB (flat bass).
Notable exceptions: MultiQVox (+1.3 dB), AceOfBass (−0.04 dB at mid-Q — cuts bass at high Q).

## S6 terminal law (verified bank-wide)

Every P2K preset has S6 zero at radius 1.000 across all four corners.
This is not a preference — it's the architectural rule of the ROM.
Our authoring profiles encode this as a linter invariant.

## Bridge to authoring

These measured ranges feed directly into `profiles/*.trenchprofile.json`:

- **resonance_budget.max_resonance_db** ← P2K max + 1 dB margin
- **frequency_domain.cutoff_range_hz** ← P2K min/max cutoff across corners
- **q_attitude_rules.q_axis_max_octave_shift** ← measured Q travel per section
- **verification.min_peak_count_morph_range** ← P2K peak count range
- **voice_roles.bands** ← measured per-section frequency ranges from lane grammar

When authoring a new preset:
1. Pick the profile family matching your recipe
2. Set guide-pose targets within the profile's validated bounds
3. The linter will validate against hardware-proven ranges
4. Targets outside bounds get auto-fixed to the nearest valid point
