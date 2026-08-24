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

**The zero layer is not an anti-skeleton.** Matching each corner's in-band sharp zero
set (>= 3 zeros, r > 0.9) against every other body's pole posture: 1 of 59 under 50
cents (a three-zero set), 3 under 100, median 187 cents. The zeros do not trace another
body's poles; they are placed against their own skeleton, freely. (2026-08-23)

**The zeros are intentional, by filter type.** Zero jobs (paired within 0.5 oct of any
pole / parked >= 10 kHz / free notch / real-axis step / none) by the manual's type: VOW
73% paired; EQ+ 47% parked; EQ- 25% notch with the deepest zeros (median r 0.984); REZ
51% paired; DST 83% none (a skeleton with one trench); SFX 75% parked; PHA 42% none; FLG
50% parked, 0% notch. Five different uses of the same six zeros, matching the panel's
word for the filter. Q, on the bank's Q axis, is pole width: every lane narrows on
median with frequency held; on vocal and REZ bodies to the ceiling word. (2026-08-23)

**Pole bandwidth is set in Hz, not octaves, and per pole.** Over 608 live conjugate poles
(r > 0.9, 100-15000 Hz): bandwidth in Hz is nearly flat across four decades (medians 97 /
145 / 121 / 191 Hz for 100-300 / 300-1k / 1-3k / 3-15k Hz) while bandwidth in octaves
falls 0.70 -> 0.29 -> 0.11 -> 0.045; correlation of log bandwidth-Hz with log frequency
+0.17. Klatt's convention. 114 distinct pole radius words, median 15 per body, no body
on a single word: bandwidth was tuned per pole. One word dominates: 0x6FFC, r 0.9990,
13.7 Hz, 55 uses from 198 Hz to 13 kHz - the ceiling class, the "radii to the ceiling"
Q100 move. The compiled vowel classes use one radius per rate (29 Hz at 44.1 k) for all
three formants. (2026-08-23)

**Poles-only PCA over 140 corners (132 bank + 8 compiled vowels).** PC1 81.2%, PC2
11.6%, PC3 2.0%: the pole layer is one slope family plus one axis of resonance placement.
Clustering is weak beyond two groups (k=2 silhouette 0.50, k=5 0.29); at k=5 the
families are: a low posture (peaks 220-950 Hz, n=47: the 303/bass bodies and most vocal
corners), a mid posture (500-2200 Hz, n=25, where one compiled corner of each vowel
class lands), a high posture (2-10 kHz, n=13: sweeps and klang_kling), a frame-only
posture (n=22: poles at the 64 Hz floor and 14-15 kHz, the bodies whose corners are
pure slope), and a vowel posture (250-1700 Hz, n=33: MultiQVox all four corners, six of
the eight compiled vowel corners). `dev/pca/poles_only_families.png`. (2026-08-23)

**Poles only: no P2K corner comes from a cube, a .4, or a compiled class; the vowel
bodies come from each other.** Symmetric mean-cents distance between strong pole sets
(r > 0.95, 150-9000 Hz). Against the 2,312 Morpheus corners at 39,062.5 Hz: 1 of 132
P2K corners under 50 cents (rogue_hertz c2 vs PWMTrans.4 c6, 24 cents, a 2-3 pole set),
17 under 100, median 193; played untransposed at 44.1 k: 3 / 13 / median 199. Against the
8 compiled vowel corners: none under 150, median 462. Against other P2K bodies: 22
corners under 50 cents, 28 pairs in all, most at 0 cents. E-mu reused pole postures
across bodies: Ooh-to-Eee c0 = Eeh-to-Aah c1 = Dead Ringer c1 (0 cents, 6 poles), within
20 cents of Ubu Orator c0; Ooh-to-Eee c1 = MultiQVox c0 = Eeh-to-Aah c0 (0 cents);
Talking Hedz c0 = Ubu Orator c1 (0 cents); Fuzzi Face c0 = Cruz Pusher c0 (0 cents, 6
poles); Millennium c1 = Meaty Gizmo c1; TB-or-not c1 = Boland Bass c2; Klub Klassik c2 =
Tooth Comb c2 (3 cents). A small library of pole postures - four or five vowels, a 303
posture, a fuzz posture - with the zeros, frame and corner assignment drawn per body.
(2026-08-23)

**The 12th-order vowel bodies were not built from the compiled vowel classes.** Matching
each vocal body's corner (strong poles, r > 0.95, 150-9000 Hz) against the eight compiled
corners: mean distance from each compiled formant to the nearest body pole is 150-280
cents for the six vocal bodies (deep_bouche c0/c2 the closest at 81-89 cents), against
418 cents for non-vocal bodies. Same neighbourhood, not the same numbers: a copied table
would sit under 30 cents. The classes and the bodies are two separate drawings of the
same vowels - the minimal three-formant statement and the six-section one with frame and
zeros. (2026-08-23)

**E-mu's two compiled vowel classes are three-formant all-pole cascades — recovered.**
`FUN_1802c57f0` (CPhantomVocal1, AahAyEeh) and `FUN_1802c58d0` (CPhantomVocal2,
Ooh-To-Aah) copy a table of 3 rows x 4 corners x 5 packed words per sample-rate family
(RVA 0x1806d6fe0 and 0x1806d71c0, 480 bytes each) into the corner banks and set the row
count to 3. Decoded at 44.1 kHz: the zeros are absent (S2/S3 zero radius 0.016; S1 a mild
real zero at +0.47), every pole shares one radius per rate (0.9979 at 44.1 k in Vocal1;
frequencies identical across the four rates to 0.1 Hz), and one scale per corner rises
0.02 -> 0.17 along the corners. Formants (Hz, sorted): Vocal1 M0Q0 503/879/2067,
M100Q0 257/2417/3770, M0Q100 982/1733/4081, M100Q100 503/4823/7607; Vocal2 M0Q0
251/640/2803 (ooh), M100Q0 640/1621/2255 (aah), M0Q100 490/1244/5574, M100Q100
1243/3203/4479. Poles only, Klatt-style, formant tables in the open.
`ref/x3_vocal_classes.json`, `ref/x3_vocal_classes.bin`. (2026-08-23)

**The pole layer and the zero layer, over all 132 corners.** Each layer alone is more
one-dimensional than the whole: poles-only PC1 81.6% (a resonant-lowpass slope family,
two clusters, silhouette 0.52), zeros-only PC1 88.0% (a rising correction, two clusters,
0.58), the full cascade PC1 72.7%. The layers are built against each other: corner rms
of the pole layer correlates +0.55 with rms of the zero layer (a steeper all-pole top
gets a steeper zero correction). Between corners, zero layers are closer to one another
(median nearest-neighbour 3.7 dB) than pole layers (6.1 dB) or whole corners (6.7 dB):
the zeros are the shared correction, the poles carry more of what differs from corner
to corner. dB additivity of the split verified to 1e-13. (2026-08-23)

**E-mu's own compilers couple radius to frequency by one affine law.** From
`trench-x3-clean/ref/ghidra_extracts/runtime_hacks.md`: the X3 three-row writer
(`FUN_1802c59b0`, fed by the four morph classes) sets `v = slope*byte + base` per rate and
`rad = (v >> 1) + 0x6400` - radius an affine function of frequency - and Morph Designer
uses the same law with different constants ("two independently written compilers, one
law"); neither carries a per-section volume term. That is the shelf<->bell line the
per-section PCA found in the 33 packed bodies (PC1 in every lane: pole down <-> wider
<-> zero further away): the coupling is built into E-mu's tooling, not just the hand.
The same note established the four-rate pre-warp of the 33 (bank ratios 0.91878 /
0.45939 / 0.22969, 31/31) and that the 17 primitives are closed-form compiled classes
with distinct designs per rate. The vowel classes `CPhantomVocal1` (`FUN_1802c57f0`) and
`CPhantomVocal2` (`FUN_1802c58d0`) are named there but their tables are not written up;
decompiling them needs Ghidra open on EmulatorX.dll. (2026-08-23)

**Poles-only and zeros-only, Talking Hedz.** With every zero removed the body is a
Klatt cascade: five resonators in slot order (S2 1.0 k, S3 1.8 k, S4 2.7 k, S5 5.2 k,
S1 10.5 kHz) over S6's 225 Hz lowpass, each falling -12 dB/oct above its peak so the
all-pole top collapses past 3 kHz. With every pole removed the body is the correction:
rising shelves from S1-S3 (+40 to +60 dB by 20 kHz) and three notches (S4 3.4 k, S5 8 k,
S6 7 kHz at -42 dB). The zeros are Fant's higher-pole correction plus the trench; the
poles are the vowel. `dev/pca/*_poles_zeros.png`, `*_inverted.png`. (2026-08-23)

**Slot order is frequency order in the vowel bodies, a five-times-chance habit elsewhere.**
Over the 117 corners with six conjugate poles: S2<S3<S4<S5 ascending in 22% (chance 4%),
descending 12%; S1 the highest pole 36%, S6 the lowest 31%, both 18%. Two bodies keep
S2..S5 ascending in all four corners: Talking Hedz and Ooh-to-Eee - the vowels, with
S2..S5 as F1..F4 and S1/S6 as the frame. The most common full order (18 corners) is
S2 S3 S5 S6 S1 S4 low to high. (2026-08-23)

**Coupled shapes do not survive a corner move.** Pairing each pole with its nearest zero
from any section (within 0.5 oct) gives 575 coupled shapes across the 132 corners, 27%
own-section. Following a pole lane to its partner corner: it keeps the same partner zero
in 25% of cases, takes a different section's zero in 36%, loses its partner in 11%,
gains one in 12%, and is unpaired at both ends in 15%. The coupled shapes themselves
cluster only as "bell" at two heights (k=2, silhouette 0.43: a 10 kHz pair, width 0.10,
zero on the pole, n=316; a 3.1 kHz pair, offset +0.15, n=209). Within a corner the roots
form bells from whichever sections are handy; across corners the pairing is re-formed.
The only thing conserved across corners is the lane index. (2026-08-23)

**Pole moves and zero moves are separately full-rank, and zeros do not follow poles.**
Delta-PCA with the blocks isolated: pole block (dlog f, dwidth x 6) PC1 27% on Morph /
20% on Q; zero block (doffset, dwidth x 6) 24% / 19%; zero block in absolute frequency
21% / 25%. Pole frequency alone is the most coordinated thing in the bank: PC1 46% on
Morph (two dimensions carry half of it) - a loose tendency for poles to shift together.
Across a move, the zero rides its pole (offset held within 0.25 oct while the pole moves
more than 0.5 oct) in only 9% of lanes on Morph and 4% on Q; it stays put while the pole
leaves in 19%. Per-lane pole/zero frequency correlation is +0.80 (S3) and +0.68 (S4) on
Morph - the bell slots - and near zero elsewhere. (2026-08-23)

**The zeros, censused.** 753 conjugate, 39 real-axis. Radius median 0.952 (quartiles
0.871-0.987): most zeros are soft shaping zeros, 30% below 0.9, 18% at the S6 floor class
(>= 0.999). The S6 zero radius is one value in all 132 corners, 0.9999980. 42% of zeros sit
above 10 kHz and 21% above 15 kHz (parked as tilt). Measured against any pole in the
corner, 70% of zeros sit within 0.36 oct of a pole (median 0.16 oct) - pairing partners,
mostly in another section; 24% have no pole within half an octave, and 70 of those are
deep (r > 0.99): free notches. The own-section offset histogram peaks at 0 (+/-0.5 oct,
271 of 753) with a long positive tail out to +5 oct (the shelf and trench idioms) and a
thin negative tail. Three jobs: pair, park, notch. (2026-08-23)

**The acoustic unit is the pole/zero pair, not the section.** Over all 132 corners, a
conjugate pole's nearest zero within half an octave sits in its own section for 20% of
poles, in another section for 54%, and nowhere for 26%. The most common cross pairings:
S1 pole with S5 zero (38), S5 pole with S3 zero (37), S4 pole with S3 zero (35), S3 pole
with S2 zero (35), S4 with S5 (31). 67 of 132 corners stack two or more poles inside one
ERB (52 pairs, 20 triples, 4 quads) for steeper or taller peaks. Corners carry five or
six strong resonances (r > 0.97) in 58 of 132 cases, three in 23, none in 16. The six
slots are bookkeeping for 24 roots that pair across them. (2026-08-23)

**The corner-to-corner move is full-rank: E-mu had no verbs.** Per body, the per-lane
change (dlog2 f, dwidth, dzero offset, dzero width x 6 lanes = 24 numbers) from M0 to
M100 and from Q0 to Q100, stacked over the bank: PC1 explains 22% of the Morph move and
16% of the Q move; eight components reach 76% of either. All six lanes move the same way
in frequency in only 19-23% of moves. Mean tendencies are small (Q: S1-S3 up ~0.5 oct,
widths narrow ~0.2 oct; Morph: S1 up 0.8 oct, S6 down 0.4). Each corner was drawn
independently per lane; only lane identity carries across corners. The authoring verbs
are ours to define. (2026-08-23)

**The S3/S6 scale cuts are not a headroom rule.** With every stage at unity scale, the
peak of the partial cascade S1..S3 is higher where a cut exists (median 76.6 dB vs 32.1 dB
without), but the cut does not follow the peak: cuts of -12 and -18 dB sit on corners whose
partial peak is 15 dB, cruz_pusher c3 has the highest no-cut peak in the bank (105.8 dB)
while its c0/c2 carry -6 dB, and within a body the cut is constant across corners while
the partial peak ranges 15-138 dB. The cut is a per-body level choice, not a computed
function of the signal at the block boundary. Order matters to the chip's fixed-point
partial products and to lane identity, not to the frequency response, which commutes.
(2026-08-23)

**Section vocabulary and slot roles, measured.** k-means over the 777 conjugate-pole
sections of the 33 bodies in (log2 pole Hz, pole width oct, zero offset oct, zero width
oct), standardised: silhouette peaks at k=4 (0.41, moderate structure, not crisp types).
The four recurring words: a low bell with its zero 2.8 oct above (736 Hz, n=175); a
narrow high bell with the zero on the pole (6.9 kHz, width 0.16, n=461 — 58% of all
sections); a very wide low section (width 3.8 oct, n=36); a mid pole with a wide zero
1.8 oct below (n=105). Slot index explains 13% of a section's type (mutual information
0.197 of 1.53 bits; chi-square p=2e-34, so the dependence is real but small); corner
explains 1% (0.016 bits). Two slot tendencies exist: S4 is the bell slot (88% type 1)
and S6 never carries a zero below its pole and its zero width never varies (the trench
floor). Otherwise the slots are bookkeeping. (2026-08-23, `dev/section_vocabulary.py`)

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

**One skeleton, any mask: a proof by synthesis.** Talking Hedz corner 0, six poles held, zeros
replaced (`native/trench-core/tools/mask_proof.cpp`, `dev/e2e/mask_proof/`): bare skeleton
-131 dB at 10.5 kHz; a bell zero on each pole (+0.15 oct, 4x width) lifts that to -1.8 dB and
the 2.65 kHz formant from -19.8 to +10.5 dB; notches between poles give a third vowel; a zero
dropped exactly on a pole removes that resonance (-189 dB) with the pole lane untouched; a
shallow zero parked at 16 kHz does nothing (-163 dB) - a park must be deep to lift the top.
Creative zeros move loudness by tens of dB (one variant +46 dB at 2.6 kHz); one gain per
corner absorbs it but the app must show level when a ring moves. (2026-08-23)

**The zero layer has one axis and no library.** 753 conjugate zeros over the 33 bodies: per
zero, PC1 of (log2 hz, log(1-r)) is 89% of variance and points along depth (0.10, -0.995).
Per corner, the 12-number mask needs six components for 89%: masks are drawn per body. Of
311 in-band sharp zeros the own-section pole is the nearest pole 26% of the time (median
offset to own pole 1181 c, to nearest pole 220 c): zeros are placed against the whole
skeleton, the storing section is bookkeeping. (`dev/zero_pca.py`, 2026-08-23)

**E-mu's Q100 corners re-posture, they do not sharpen.** Per lane, audible poles (< 8 kHz,
bw < 1500 Hz, 227 lanes) Q0 -> Q100: 86% move more than 20 c, median 806 c; widths narrow
202 -> 97 Hz median in 143/227. Nearest Q100 pole in any slot is within 20 c for 17%, median
220 c. The CHARACTER dial (same poles, narrower) is our instrument, not a reproduction.
(`dev/zero_pca.py`, 2026-08-23)

**The 289 cubes share the bank's tuning grid, not its frames.** 13,196 poles, 9,304 zeros
(`dev/cube_poles_plot.py`, `dev/cube_cascade_plot.py`, `dev/cube_skeleton_library.py`,
`dev/cube_primitives.py`). Audible cube poles sit within 20 c of a bank pole 57%, within
50 c 78%, median 16 c; frame-for-frame (20 c and width within 2x) only 83/459 cube primitives
exist in the bank and 125/465 bank poles in the cubes. 1,980 non-empty corners: poles only,
the 8-16 kHz octave is -132 dB median re 100 Hz; with zeros -6 dB. Whole skeletons shared
across cubes (audible poles within 50 c): 83 postures over 397 corners, 637 one-offs, in
families - octave ladders (2089/4170/6270 Hz, 20 cubes), harmonic combs (64..388 Hz at the
r 0.999 ceiling), vowel sets (777/2059/3451/4838), bell tops (5682, 6940). There is no small
primitive set the cubes are made of; the cube corpus is taste evidence, not a parts list.
(2026-08-23)

**M100 is a different frame, not a tweak.** Lane by lane, of the 106 audible poles at M0 across
the 33 bodies, 0 are held at M100 (within 50 c and width within 2x); 32/33 bodies move every
resonance. Three kinds of move: transposition (Fuzzi Face +1160..+1315 c on all six; Tooth
Comb, Acid Ravage +1400..+2800), tweak (Deep Bouche +60..+250 c; TB or not TB +-400..800),
re-deal (Ooh-to-Eee / Eeh-to-Aah swap between two shelf postures in opposite directions;
Hedz and Ubu trade poles between slots). Separately, M0 corners share 5 postures (13 bodies),
M100 corners share 4 (8 bodies). A zero set transfers across skeletons in the same register
(Hedz zeros under Dead Ringer, Klub Klassik, a measured mouth: same 7.2 kHz notch, top at
-22..-33 dB) and is nonsense outside it (DJ Alkaline's high poles +114 dB, the cello's low
poles -300 dB). (`dev/skeleton_library_m0_m100.py`, `dev/skeleton_swap.py`, 2026-08-24)

**Equal Morph steps are not equal changes.** 24 equal steps of the 2019 law, spectral change
per step on a log grid (`dev/morph_step_uniformity.py`): Dead Ringer x1.3 max/min, Hedz x2.1,
Ooh-to-Eee x3.0, BassBox 303 x5.4 with the travel crammed into 60-75% of the dial. Rossum's
law evens the re-deal bodies; transposition bodies still bunch. Debugging signal for the
bisection sessions, not acceptance. (2026-08-24)

**The postures recur at the byte level.** Across the 9 shared postures (41 aligned lanes,
packed domain, `dev/posture_packed_audit.py`, plot `posture_packed_words.png`): 27/41 lanes
have IDENTICAL pole frequency words across every member body; the rest differ by one or two
minifloat steps (256/512 units, 11-28 c), except one loose match in posture 3 and a
lane-shift artifact in posture 5. Radius words do not follow: within a posture they move
with the corner (DJ Alkaline c0->c2: every mag word held, every rsq word 42236->28668) -
E-mu's Q is "keep the frequency word, swap the radius word". Zeros recur conditional on the
skeleton only inside the vocal family: postures 1 and 2 share 3-4 zero words across all
members ((45564,49404) etc., the vowel cavity made exact); elsewhere shared skeleton has
each body's own mask. Millennium and Meaty Gizmo c1 are one corner copied verbatim, every
word. The bank was authored copy-corner-then-edit at the word level. (2026-08-24)

**Exact duplicate corners, fingerprinted without clustering.** Every corner hashed in packed-
word space at three levels (`dev/corner_fingerprints.py`): Level 1 (sorted pole frequency
words) 10 duplicate groups, 22/132 corners, all cross-body. Level 2 (frequency+radius) the
SAME 10 groups - when a frequency skeleton is copied exactly, its radius words come with it;
radius varies across a body's own Q corners, not across bodies sharing a corner. Level 3
(every packed word) only Millennium c0=MeatyGizmo c0 and c1=c1 - masks always differ except
that one verbatim pair. The exact-copy network: klub/acid/tooth c0 (3-way), dead_ringer c1/
ooh c0/eeh c1 (3-way), hedz c0/ubu c1, fuzzi/cruz c0, boland c0/lucifer c0, boland c1/
bass_tracer c1, ooh c1/eeh c0, klub c1/acid c1, millennium/meaty c0+c1. (2026-08-24)

**The Q0 library, five fingerprints, no clustering.** c0/c1 only (66 corners,
`dev/library_q0_analysis.py`): pole-half duplicates = the same 10 groups / 22 corners as the
all-corner run (Q100 contributes nothing). Exact full lanes (all 5 words) recur across
bodies in only 12 cases and 10 of them are the Millennium=MeatyGizmo verbatim body - lane-
level copying essentially does not exist outside whole-corner copying. Partial lanes (pole
half only) recur 61 ways over 157 lane-slots - the real library is the pole half of a lane.
Morph-pair reuse: only Millennium=MeatyGizmo and KlubKlassik=AcidRavage ship the same c0+c1
pair; and 4 cross-role reuses exist where one body's c0 is another's c1 verbatim
(ooh c0 = dead_ringer c1 = eeh c1; hedz c0 = ubu c1; eeh c0 = ooh c1) - E-mu pointed the
same pose in opposite morph directions. (2026-08-24)

**The words were the computation domain, not just storage.** From EOS firmware analysis
(Tyson, 2026-08-24): the engine derives radius from the stored rsq word entirely in packed
space — `rad = (v >> 1) + 0x6400`. In the 4.12 minifloat the exponent occupies the top bits,
so a right-shift halves the exponent (a square root) and 0x6400 is the rebias; verified
against corpus words, the decoded output tracks ~0.3*sqrt(decoded v) across the range (0.29
to 0.31, mantissa piecewise-linearity). Same design decision as interpolate-in-encoded-
domain, seen twice: shifts and adds on packed words replace exp/sqrt at runtime. The lattice
is a number system engineered so the DSP's operations are integer arithmetic on the encoding.
(2026-08-24)
