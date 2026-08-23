# Measured evidence

Consolidated from two untracked handoff notes (2026-08-18 "SOL", 2026-08-19) so the
measurements survive without their stale narrative. Everything here reduces to bytes or
to a reproducible run. File:line references have been dropped — they were stale within
days. Where a claim is inference rather than measurement it says so.

Verify against the bytes before acting on anything load-bearing. If a check disagrees
with a fact here, stop and report the discrepancy rather than redesigning around it.

Sources: `ref/morpheus/cubes_decoded.json`, `ref/morpheus/bodies/*.body`,
`ref/presets/*.bin`, `ref/x3/*.bin`, `ref/md_templates/*.bin`,
`dev/cell_dictionary/decode_lib.py`, `dev/p2k_diagnostics.py`, `dev/p2k_primitives.py`.

## Structure of a corner

**Corners are independent, complete configurations.** No move grammar generates them:
1 of 198 P2K stage-quads is bilinear across (M,Q); Morpheus stage-params separable across
axes even at 20-cent / 2e-3 tolerance are ~15%. The stage-corner cell (pole pair, zero
pair, level) is the atom; the lane (storage slot × corners) is the unit.

**Corner index bits**: bit0 = Transform2, bit1 = Morph, bit2 = Frequency. Verified against
LPFlange.4's manual entry at the 39,062.5 Hz datum.

**The ".4 = C4–C7 copies of C0–C3" rule is false** for all 109 `.4` records. No axis
mirrors cell-exactly. The live plane is a per-object fact; the other four corners are
preserved baggage. Measured lean: the t=1 (odd corner) side is more active in 59/109,
equal in 48. Do not hard-code a rule for which plane is live — it remains open.

**82 of 289 cubes have corners 0, 2, 4, 6 fully inert** (bypass vertices).

## Gain

**Gain is one number per corner.** Morpheus stores it explicitly (decode: 1787 ≈ unity);
P2K bakes it evenly into per-stage scales — 105 of 132 corners have identical scale
across all six stages. Section scales multiply through the serial cascade, so dB adds.
177 of 289 cubes split gain across corners.

P2K additionally shows exact power-of-two headroom cuts at S3 (11×) and S6 (23×) with
**never** a compensating boost elsewhere — the signature of a compiler's headroom
management rather than a designer's choice.

## Vocabulary

**The named stage-archetype vocabulary has no basis in the bytes.** 7,363 distinct cells
of 16,184. The only corpus-wide recurring cells are three structural pads: bypass pair
`1450/2047`, idle pole `[1909, 2015, 0, 2047]`, off zero.

**Every P2K stage-quad is unique** (198/198). No quad was ever copied whole.

**51 distinct sections recur across P2K presets**, covering 113 of 792 cells (14%). Top is
×4. Family resemblance, not a vocabulary. Keying must be stage-agnostic and unordered —
slicing by stage index hid recurrence (n≥3 went 13 → 41 when stage was removed from the
key), and ordered pose matching found 5 shared poses where unordered found 10.

The earlier "cube alphabet 819 vs P2K 51" claim was a corpus-size artifact and is
withdrawn: at matched 33 objects P2K shares more (66.9 vs 20.9 cross-object clusters per
1000 cells).

## Stage roles attach to frequency rank, not slot index

Ranking each P2K corner's stages by pole frequency, across all 132 corners:

| pole rank in corner | n | median \|log2(zero/pole)\| | median tilt 40 Hz -> 16 kHz |
|---|---|---|---|
| lowest | 132 | 3.18 oct | -39.1 dB |
| middle | 475 | 0.66 oct | -5.8 dB |
| highest | 132 | 0.65 oct | +7.7 dB |

**The lowest pole in every corner is the body/tilt stage** — zero flung ~9x away, -39 dB
of broadband tilt, in 132 of 132 corners. The highest pole lifts the top (+7.7 dB) but
does it with a nearby zero; TalkingHedz S1 (pole 10523 Hz, zero 391 Hz) is an outlier
rather than the mechanism.

The roles are real but **emergent and positional in frequency, not in slot**: TalkingHedz
carries its lowest pole in S6, Ooh-To-Eee in S2, Eeh-To-Aah in S2. Same role, different
slot, every preset. This is why any fixed slot map (S1 = air, S6 = tilt, S7 = trim) fails
against the corpus, and it is consistent with the zeros being solver output: a comparator
minimising error over the sum always needs one stage to carry the tilt, assigns it to
whichever pole sits lowest, and places that stage's zero far away to stop it dragging the
whole spectrum down.

## Idle cells — two lineage forms

Both decode to H(z) = 1 exactly.

| lineage | files | cell | count |
|---|---|---|---|
| Morpheus (8×7, 560 B) | 289 | `DFFF FFFF DFFF FFFF DFFF` | 3,182 of 16,184, all 7 positions |
| X3 (4×6, 240 B) | 17 | `EFFC FFFD EFFC FFFD DFFF` | 6, at S1–S3 |
| P2K (4×6) | 33 | — | 0 |
| MD templates (4×6) | 69 | — | 0 |

The Morpheus form is `trench_core::minifloat::IDENTITY_STAGE`. It is `b=[1,0,0] a=[1,0,0]`
— flat unity with no residue. The X3 form is identity *by cancellation*: b and a are both
`[1, 0.99902, 0.000244]`, i.e. a pole at r ≈ 0.9998 with a coincident zero.

This corrects the 2026-08-19 note's claim that no factory body carries an idle stage.

## Real-axis root pairs are 5% of the corpus and a third of the bodies

Measured 2026-08-20 over every body in the repository — `recipes/hero` (12),
`recipes/extrusions` (4), `ref/morpheus/bodies` (289), `ref/presets` (33), `ref/x3` (17),
`ref/md_templates` (69) — each decoded at its own datum.

| | count |
|---|---|
| cells | 23,744 |
| cells whose pole is a real-axis pair | 288 |
| cells whose zero is a real-axis pair | 1,046 |
| cells with either | **1,220 (5.14%)** |
| of those, cells `roots_from_words_at` returns `None` for | **1,220 — all of them** |
| bodies containing at least one | **158 of 424 (37%)** |
| fully degenerate (empty) cells | 6,343 |

`StageRoots` is conjugate-only, so it cannot address any of those 1,220 cells: there is no
(frequency, radius) pair for a root on the real axis. A conjugate-only authored
representation therefore corrupts **more than a third of the corpus**, which is the
mechanism behind the `vocal_ah_ay_ee` S1 report — a conjugate pole was deleted along with
its real zero because the section was matched as a unit.

`StageGeometry` carries `RootPair::RealPair` and round-trips all 23,744 cells bit-exactly
(`words_from_geometry_at ∘ geometry_from_words_at` is the identity on every one).

Consequence for fitting: `arma_endpoint` seeds from `[StageRoots; 7]`, so for those 158
bodies at least one section cannot enter the solver at all. It must be excluded and its own
dB contribution subtracted from the target, or it silently vanishes from the comparator and
the remaining sections are fitted against the wrong curve.

`recipes/hero/4-zero-strider.body` S4 is the clearest specimen: a real pole pair
`(0.45996, 0.0)` and a real zero pair `(-0.09989, -0.65011)`, identical in all eight corners.

## P2K skins: the measurements

- **Sections cancel 3.24×** — Σ per-section dB span ÷ summed-cascade span. Morph Designer
  XML is 1.46, Morpheus cubes 5.04. High cancellation means the sections are not
  individually meaningful; they are the output of something fitting the sum.
- **Every P2K preset carries a unit-circle zero at S6 in all four corners** (132/132),
  swept a median 11.3 semitones across morph in 28 of 33. Absent from XML (0/20) and from
  cubes (~0%).
- **Lanes are unsorted** — 11% of corners monotonic in frequency, 24% of lane pairs cross,
  in 22 of 33 presets. Correspondence was never preserved.
- **The 132 corner spectra are 3-dimensional**: PC1 tilt 72.7%, PC2 2–5 kHz presence 14.0%,
  PC3 formant balance 4.6% — 91.4% for PC1–3.
- **Q direction is derivable from type** (VOW narrows 92%, EQ− widens 87%); magnitude and
  frequency are not. **M is not derivable from type** and is not a transpose (23% variance).

## Reconstruction: how the P2K skins were made

Inference, but every step is anchored to a measurement above.

**A person chose four target spectra per preset.** Corners are complete independent
configurations (1 of 198 quads bilinear across M,Q); corner spectra are 3-dimensional
(91.4% in three PCs); Q is note-on only, so the four corners are two morph pairs.

**Not an all-pole method.** Cancellation (sum of per-section dB spans / cascade span) is
3.41 as authored but only 1.93 for the same poles with zeros stripped. A factorised
all-pole filter's sections agree with the sum far more than these do; the extra
cancellation is the independently placed zeros. That rules out AR/ARX as the origin,
independently of the fact that an AR estimate produces no zeros at all while 143 of 144
vowel cells carry one.

**A solver produced the sections, per corner, independently.** Sections cancel 3.24x;
198/198 quads unique; zeros show sd 9.99 in ratio and 0.0% follow-rule compliance; roles
attach to frequency rank rather than slot. Lanes cross in 24% of pairs because nothing
tied one corner's slot to the next — correspondence was never in scope rather than
abandoned.

**Gain was a post-pass.** 105 of 132 corners carry one figure divided six ways, and the
power-of-two headroom cuts at S3 and S6 never come with a compensating boost.

**One element looks placed rather than solved:** the unit-circle zero at S6, present in
132/132 corners and swept a median 11.3 semitones in 28 of 33 presets. Probably a fixed
part of whatever template the solver started from.

**It was not Morph Designer** (closed-form, no search) and not the shipping binary
(EmulatorX.dll stores the bank as a static table). The tool that did this did not ship.

**Reproducible here:** choose four targets, fit each corner against the complete cascade,
spread gain per corner. The one option E-mu did not take is constraining lane
correspondence across corners; their interiors work without it, so it is a choice rather
than a requirement.

## E-mu tooling, from EmulatorX.dll

Extracts at `../trench-x3-clean/ref/ghidra_extracts/`.

**The P2K bank is embedded data, not compiler output.** Talking Hedz resolves as
`DAT_1806d762e + (skin_index * 4 + variant) * 0xf0`, with `0xf0` = 240 bytes = one P2K
body. The binary stores the bank as a static table and never generates it — which is why
no factoring algorithm is findable in it.

**The table holds 50 skins × up to 4 variants, not 33.** Indices `P2k_033`..`P2k_049` are
exactly the 17 fixed-class filters, contiguous in the same table, corroborating the
contiguous SysEx ID block.

**Morph Designer does not fit anything.** `FUN_1802c6590` compiles six 6-byte records
(type, freqA, gainA, freqB, gainB) through closed-form integer arithmetic:

```
rad = ((freq * 0x7c) >> 8) + 0x76
w1  = clamp(rad + gain, 0, 255) << 8      # pole radius
w3  = clamp(rad - gain, 0, 255) << 8      # zero radius
```

Radius is a linear function of frequency, and "gain" is an **opposed radius split at a
shared frequency** — pole up, zero down. Level from geometry, never from a gain term.
E-mu's own higher-level authoring tool had no musical model underneath it; it had geometry
with a friendlier front end.

Four other corner-bank writers are named but not yet extracted: `FUN_1802c59b0` and
callers `c5d60` / `c5e40` / `c5f10` / `c6020`, two of which select from tables of 16
six-word or three-word profiles.

## Import and datum

**All 289 cubes are imported as null-verified native 560-byte bodies** at
`ref/morpheus/bodies/`. Re-encode is idempotent; worst stage-coefficient error 3.3e-4;
scale within 0.001 dB. Axes Morph→M, Freq→Q, Transform2→Z; per-corner gain spread evenly
across active sections.

**No P2K corner is a Morpheus corner, by response.** Every one of the 132 P2K corners
against all 2,312 Morpheus corners (289 cubes x 8), ERB grid, level removed, both with
the cube designed at 39,062.5 Hz and with its words played untransposed at 44,100 Hz:
nearest-corner distance min 3.4 dB rms, median 7.3-7.6 dB — no closer than the median
nearest corner of an unrelated P2K body (7.7 dB). The closest pairs are generic lowpass
shapes (klub_klassik c3 vs BrickWaLP2 c6, 3.7 dB). A port would sit near 0 dB.

**Millennium and MeatyGizmo share their Q0 corners byte-for-byte** (c0 and c1 identical);
only their Q100 corners differ. E-mu reused a Morph pair and authored the Q corners as
the second shape. (2026-08-23)

**The packed bank is exactly the manual's 33 twelfth-order filters.** The Mo'Phatt
manual's filter table (pp. 133-135, `ref/mophatt_filter_types.json`) gives every filter
an Order: 33 are order 12 and map one-to-one onto `ref/presets`; the other 17 (Smooth 02,
Classic 04, Steeper 06, Shallow, Deeper, Band-pass1/2, ContraBand, Swept1-3>1oct,
AahAyEeh, Ooh-To-Aah, PhazeShift1/2, FlangerLite, BlissBatz) are 2nd/4th/6th order —
computed classes, not packed bodies — which is why their table slots decode to unit-circle
roots and zero scales. The only borrowing the manual admits is BlissBatz, "Bat phaser
from the Emulator 4", a 6th-order class. (2026-08-23)

**The P2K bodies are authored in Hz and compiled per sample rate — not a 39,062.5 Hz
port.** Each skin has four DAT variants in `EmulatorX.dll`; read at 44,100 / 48,000 /
96,000 / 192,000 Hz respectively, every pole lands on the same frequency to 0.1 Hz and the
radius scales as a fixed bandwidth in Hz (Hedz S2: 1005.6 Hz in all four). Variant 0 is
the 44.1 kHz compile; no variant is 39,062.5. 792 live P2K sections share zero exact words
with any section of the 289 Morpheus cubes; the closest geometric relative shares 6 poles
of 24 and is `257_Overblow`. The capture null alone only proved playback at the host rate.
(2026-08-23, `ref/sections.tsv`, `trench-x3-clean/ref/p2k_variants`.)

**The P2K datum is 44,100 Hz — confirmed against captured audio.** Four TalkingHedz corner
captures were deconvolved against the bypassed pink-noise excitation
(`trench-s6-slot-law/ref/inputs/bypassed-pinknoise.wav`) and compared to the decoded
cascade. Each capture identifies its own corner, 4 of 4, with 0.28–0.95 dB rms residual
over 1–15 kHz and a 7–20x margin over the next-best corner. The same comparison decoded at
39,062.5 Hz gives 6.6–12.4 dB, about ten times worse.

This simultaneously confirms the decoder, the response math, and the corner index law
(bit0 = M, bit1 = Q: m0q0→C0, m1q0→C1, m0q1→C2, m1q1→C3).

Boundary: the captures are from the shipping runtime, which stores this bank as a static
table, so the coefficients share a source with ours. What is validated is the decode, the
response evaluation and the datum — not the byte table independently.

**Supporting, indirect:** Convergent evidence: the E-IV runs 44.1/48k and
owns the 17 fixed-class filters; the vowel match improves 2.70 → 1.56 st; cross-era family
correspondence appears only at matched 44.1k (63 joint root matches vs null mean 27.9,
p95 42) and is below chance at mismatched rates. The 39,062.5 Hz validation covers
**Morpheus only** (LPFlange.4).

## Response verification values

Peaks that null against an independent Python decode of the factory bytes. Use these to
prove no behaviour change after a refactor:

```
LPF  +4.5 dB @ 9.7k
VOW  +15.4 dB @ 3.6k
SFX  +61.6 dB @ 4.0k
PHA  corners 12.7 / 15.5 / 32.6 / 32.2 dB
```

Two traps when checking. `curves` caches by **array identity** (WeakMap), so a words array
must be replaced, never mutated in place — a stale-curve bug is invisible except by
comparing peak values across loads. And `DRAW_POINTS` is 1024 for a reason: at 256 the
grid steps ~40 cents and KlangKling S1 (21 Hz bandwidth) reads 46.5 dB instead of 57.2.

## Method rules that came out of errors

- Judge against a null built from the corpus, never a synthetic one. A synthetic
  log-uniform null gave a false negative on the vowel test that reversed to a strong
  positive (P = 0.849) under a corpus control.
- Enumerate rather than summarise; slicing destroyed the signal three separate times.
- Never publish a median of absolute values as a direction. Signed, REZ median is
  +3.98 st with IQR 28; the absolute median made unsystematic scatter look systematic.
- One documented liveness predicate, `LIVE_R = 0.45`. Three different thresholds
  (0, 0.05, 0.45) once published `interval_median` as both 10.14 and 12.34 under one
  label. **10.14 is correct.**
- The notch-depth mechanism reported earlier was fabricated. Measured, all PHA/FLG presets
  get *shallower* at Q100 (FreakShifta 57.8 → 35.5 dB).

## Open

- Which four corners are a `.4` cube's live plane, per cube. Measured lean only.
- Safety gates vs factory practice: 243 of 289 imports exceed the gates. The gated writer
  stays as is; do not "fix" either side.
- The XML "hand-authored control" behind the 1.46 cancellation figure rests on derived
  4-corner bodies, but Morph Designer authors only 2 endpoints — the control may be wrong.
- The semitone-snap claim was never tested against the 7-bit lattice null; XML frequency is
  a 0–127 integer, so snapping may be forced by the encoding.
- Seven un-extracted writer tables (`c5d60` / `c5e40` / `c5f10` / `c6020` profile banks)
  remain in the DLL.
