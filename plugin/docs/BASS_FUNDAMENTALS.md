# Trench Audio Engine: Empirical Baseline Specification

**Reference Document — Direct Measurement & Architectural Bridge**

---

## 1. Empirical Findings: Reference Captures & IR Analysis

Parsed impulse responses (IRs) and neural captures yield explicit frequency
boundaries that replace abstract assumptions with measured hardware physics.

**Speaker Cabinet IRs (Output Stage Dynamics):**
- **General Guitar Cabs:** Main acoustic energy centered between **100 Hz and 250 Hz**, with natural −3 dB low-end roll-off initiating around **100 Hz**.
- **"Tube Color" IR:** Extended low-end response, −3 dB band spanning **30 Hz to 165 Hz** — heavy-cabinet resonance profile optimized for thick lower-mid tracking.
- **"Solid State Color" IR:** Nearly flat, full-range response, −3 dB band spanning **105 Hz to 23,000 Hz** — transparent linear transfer.

**303 Acid Basslines & Synth Spectrum:**
- Fundamental frequency range spans **C1 (32 Hz)** up to **A6 (1,723 Hz)**.
- Core operating zone strictly **40 Hz to 100 Hz (E1–G2)**, carrying primary energy foundation.
- Signature "squelch" generated as 4-pole (24 dB/octave) lowpass filter resonance sweeps dynamically through harmonic overtone series from **1 kHz to 5 kHz**.

---

## 2. Physical Instrument Frequency Map

Target specifications mapped against real-world acoustic boundaries:

| Register / Component | Frequency Range | Functional Role in Preset Design |
|---|---|---|
| **Sub-Bass Region** | 20 Hz – 60 Hz | Infrasonic floor; controlled to prevent phase cancellation and headroom collapse. |
| **Bass Low E / Low B** | 30.9 Hz – 41.2 Hz | Absolute fundamental root frequency for 4- and 5-string basses. |
| **Bass Body & Weight** | 60 Hz – 250 Hz | Primary warmth and punch; aligned with cabinet peak resonance zones (100–250 Hz). |
| **Presence & Attack** | 800 Hz – 2 kHz | Mechanical string snap, pick attack, and nasal filter formants. |
| **Resonant Squelch** | 1 kHz – 5 kHz | Acid filter sweep territory; high-radius pole cluster activity. |

---

## 3. Six-Stage Cascade Architecture for Bass Presets

Translating physical fundamentals into multi-stage biquad architecture:

1. **Sub-Anchor Stage (S1):** Locked permanently to **40–80 Hz** corridor. Moderate pole radius to maintain unyielding sub-retention and prevent energy dropouts during morph sequences.
2. **Body & Growth Stages (S2–S4):** Allocated to lower-mid spectrum (**100–800 Hz**), capturing cabinet resonance characteristics and foundational body weight.
3. **Character / Squelch Stages (S5–S6):** Positioned for **1–5 kHz** window, executing high-radius resonance peaks to replicate active filter sweeps of classic acid synth hardware.

---

## 4. Guide-Pose Authoring & Linter Target Metrics

The guide-projection and pre-flight linter (`guide_projection.py`) consume
these empirical targets to validate authoring spaces before optimization runs:

- **Cutoff Target Matrix (cutoff_hz):** Key authoring nodes at **40 Hz, 100 Hz, 500 Hz, 2,000 Hz, and 5,000 Hz** across morph coordinates (M0 to M1).
- **Resonance Budgeting (resonance_db):** Ranging from **0 dB** (clean linear transfer) up to hardware engine safety cap (**24 dB**).
- **Bass Retention Guardrail (bass_retention_db):** Evaluated at M0, enforcing strict rule that lower-band energy loss must not exceed **−3 dB** relative to dry input signal.

### Guide-Pose Target Tables

**Bass body:**
| Label | Morph | Q | Cutoff Hz | Res dB | Purpose |
|---|---|---|---|---|---|
| sub-bass-floor | 0.0 | 0.0 | 40 | 2.0 | Deepest clean bass |
| bass-body | 0.3 | 0.0 | 100 | 3.0 | Bass note region |
| low-mid-presence | 0.5 | 0.0 | 500 | 4.0 | String articulation |
| mid-growl | 0.7 | 0.0 | 1500 | 6.0 | Aggressive mid push |
| high-sheen | 1.0 | 0.0 | 4000 | 2.0 | Top-end air |
| q-resonant-sub | 0.0 | 1.0 | 50 | 8.0 | Resonant bass boost |
| q-acid-squelch | 0.5 | 1.0 | 800 | 15.0 | 303 acid character |
| q-scream | 1.0 | 1.0 | 3000 | 20.0 | Full resonance sweep |

**303:**
| Label | Morph | Q | Cutoff Hz | Res dB | Purpose |
|---|---|---|---|---|---|
| closed | 0.0 | 0.0 | 80 | 8.0 | Filter closed, sub rumble |
| half-open | 0.4 | 0.0 | 400 | 12.0 | Classic acid mid-sweep |
| open | 0.7 | 0.0 | 2000 | 16.0 | Open filter, harmonic rich |
| wide-open | 1.0 | 0.0 | 8000 | 4.0 | Fully open, bright |
| accent-closed | 0.0 | 1.0 | 120 | 18.0 | Accent + closed filter |
| accent-squelch | 0.4 | 1.0 | 600 | 22.0 | Peak acid squelch |
| accent-scream | 0.7 | 1.0 | 3000 | 24.0 | Maximum resonance |
| accent-bright | 1.0 | 1.0 | 5000 | 10.0 | Bright accent decay |

### Voice Role Allocation

| Lane | Role | Band | Purpose |
|---|---|---|---|
| S1 | anchor | 30–400 Hz | Sub-bass foundation, cutoff floor |
| S2 | mouth | 400–3500 Hz | Primary resonant character |
| S3 | mouth | 400–3500 Hz | Second formant / harmonic emphasis |
| S4 | mouth | 400–3500 Hz | Midrange articulation |
| S5 | air | 3500–20000 Hz | Top-end presence |
| S6 | terminal | 3500–20000 Hz | High shelving / rolloff |

### Verification Gates

- Cutoff travel must span ≥ 4 octaves (40–4000 Hz minimum)
- Bass retention at morph=0, q=0 ≥ −3 dB at 40–200 Hz
- No corner unstable
- 303 presets: peak count must increase from 1 (closed) to 5+ (open) as morph sweeps
- Q axis must audibly increase resonance without shifting cutoff > 1 octave
- Resonance budget: 24 dB engine cap enforced by linter invariant I3
