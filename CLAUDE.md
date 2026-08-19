# Project instructions

## Agent contract

This repository is evidence-driven reverse engineering. Preserve established
behaviour before improving structure.

- Do not infer architecture, semantics, or rules that are not established here
  or in the repository.
- Do not generalise observed factory data into a rule unless that rule is
  explicitly established.
- Do not reintroduce anything in Removed, even under a different name.
- Treat Canon and Rules as design constraints, not suggestions.
- Treat Proven here as acceptance evidence; preserve the tests that establish it.
- Treat Open items as unresolved. Do not silently solve them by assumption.
- When prose and executable evidence appear to disagree, investigate and report
  the discrepancy rather than choosing whichever interpretation seems cleaner.
- Preserve strange factory behaviour unless there is evidence that it is an
  implementation error.
- Prefer the smallest change that satisfies the requested task.
- Do not refactor unrelated code while implementing a requested change.
- Before changing a representation or import/export path, identify the relevant
  null/round-trip test and preserve it.

## The object

- A body is a serial cascade of second-order sections. Native bodies hold
  7 stages and 8 corners (560 bytes); the legacy corpus holds 6 stages and
  4 corners (240 bytes). Unused stages are the exact identity row.
- A section is a pole pair, a zero pair, and a scale. The body stores no
  filter types. The legacy import compiler (`trench-core/src/compiler.rs`)
  translates typed factory cards and RBJ-style recipes on the way in; nothing
  downstream of the packed words reads a type, and none may be assumed when
  authoring.
- Corners are addressed `m | q<<1 | z<<2`. The runtime interpolates packed
  words per lane between corners. Section identity is fixed across corners;
  sections may cross in frequency, and the corpus does so deliberately.
- The legacy corpus datum is 39,062.5 Hz. The authoring default is 44,100 Hz
  (`stage_law::DEFAULT_AUTHORING_SR`). Bodies recompile between rates by
  carrying root geometry, clamped at 0.49·fs.
- The packed grid holds resonant roots within about 10 cents, radii within
  5e-4, scale within 0.01 dB. A zero radius may be exactly 1.0 — a traveling
  null. A pole radius must stay below the encoder's contiguous ceiling.
- One gain per corner. The cascade fitter spreads it evenly across active
  sections; the formant endpoint embeds it in its first active section. Level
  comes from geometry — a tight pole against a distant unit-circle zero —
  never from per-section gain shaping.

## The pipeline

The workstation is the browser app: `cargo run --release -p author-server`
serves it at http://127.0.0.1:8787 (the server roots itself at the repo by
finding `recipes/vocal/dvtd` above the cwd or exe; the library dock reads
`recipes/`, bodies from `recipes/hero`). The surface keeps the topology's
flow as docks: LIBRARY, the cascade S1..S7 as per-section curves, RESPONSE,
ROOTS, CORNERS, the RIDE pad (audition through the parity-tested worklet —
`workstation/parity.html` and `author-server/src/parity.rs` hold the vectors),
and the audit VERDICT. The roots view is the documented log-polar mapping
laid flat — resonance in dB (R' = 20·log10(1/(1−R))) over log frequency, the
encoder ceiling drawn as a line, the traveling null at the top edge — and it
is an authoring surface. The egui editor is retired: `Trench Editor.exe` at
the repository root is a stale artifact and the `editor/` crate is out of the
workspace; the browser workstation is canonical.

The same pipeline runs headless as
`cargo run --release -p author -- <basis-dir> <out.body>`.

Both routes: envelopes → fixed 1024-point log grid, 40–16,000 Hz → PCA with
stored mean and rotation → 8 corner scores inside the data ellipsoid → inverse
PCA → one target spectrum per corner → corner 0 fit cold, the rest fitted in
Gray order seeded from their neighbour so lanes correspond → packed words →
560-byte body → safety audit over the 3D interpolated surface.

## Documented

- **Palette** — Martens, ICMC '85. Hear → adjust → record → fit → invert.
  Five points, simplex fit, re-spaced at equal perceptual distance.
- **Martens 1987** — ICMC. Banded spectra → PCA → score trajectories →
  inverse PCA → resynthesised spectra. Synthetic scores yield novel spectra.
- **Martens 2001** — ICAD. Rating model inverted to obtain control values.
- **Martens 2002a** — ICAD. Perceptual bisection by Higher/Lower adjustment →
  lookup table. Five settings; the median is the anchor.
- **Sandell & Martens** — prototype plus distinguishing deviations. Scores are
  meaningless without their mean and rotation matrix.
- **Burred, Röbel & Rodet** — LSAS 2006. Resample envelopes onto a fixed
  frequency grid before PCA.
- **US5170369 and the ARMAdillo encoding note.** Architecture, interpolation,
  log-polar display: `R' = 20·log10(1/(1−R))`, `θ' = π(10 + log2(θ/π))/10`,
  θ below π/1024 excluded. The patent's language is coefficients,
  interpolation, sweeps — no cube or frame vocabulary.
- **US10514883** — Rossum, 2019. Encoded parameter tables per frequency
  response; linear interpolation of encoded (log-space) values, then decode by
  exponentiation — per parameter, per section. A 14th-order filter is 28
  uniform parameters (14 frequency + 14 amplitude); no shelf section. Gain:
  conditional per-root DC stabilization accumulated into one aggregate factor
  plus one interpolated filter gain at the last section's output, then soft
  clipping. Interpolator data may be low-pass smoothed against clicks. Pole
  radius is corrected at runtime against a distortion threshold. Stability is
  by construction: the radius encoding cannot express unity or above.
- **Morpheus hardware manual** — only Morph varies continuously in real time;
  Frequency Tracking and Transform 2 change at note-on only. Transform 2's
  effect varies from filter to filter.
- **Morpheus module manual** — Rossum Electro-Music. The hierarchy is
  Frequency Response → Cube → Filter. A Frequency Response is "a single
  static configuration of Morpheus's 14 poles and zeros"; a Cube is "up to 8
  complex frequency response configurations … at the corners of a three
  dimensional cube"; a "`.4`" suffix marks a cube of four responses on a
  plane (no Transform axis; the knob defaults to distortion). Axes:
  Frequency, Morph, Transform. The original hardware morphed one axis in
  real time; the other two were set at note-on. The module ships 289 Cubes.
- **Kerkhoff & Boves** — Eurospeech '93. A serial cascade of six second-order
  pole-zero sections as a vocal tract. Time-varying poles can go unstable
  even when every stationary parameter set is stable — pole parameters must
  change smoothly; zeros are feedforward and may jump freely when
  DC-normalized. Poles and zeros cannot move independently through a
  transition: interpolating both between targets does not preserve the
  intended shape, and a zero track crossing a pole track cancels the formant.
  Zero travel is therefore authored: linear, stepwise at the boundary, or
  follow — the zero holds its relative place between its two neighbouring
  formants so it can never drift onto one.
- **Vulcan firmware disassembly** — Rossum Morpheus module OS (reported
  disassembly, third audit pass; each pass overturned parts of the last, so
  everything here is as-reported, not re-verified — but this pass carries
  independently checkable signatures: 0x15/0x75/0x5C on the FMC bus are the
  SSD1351 OLED's own CASET/RASET/RAMWR, so the earlier "hardware DSP bus"
  and its 127−x coordinate claims were the display driver, retracted whole.
  The audio engine runs in software on the Cortex-M4 (SAI DMA): a strict
  7-stage serial cascade, S1→S7, no re-ordering or re-pairing, with
  parallel-sounding cubes achieved inside the serial topology by geometry.
  The decode law is the trench-core family law: angle θ = (M·2^E)·π/2^27
  from an 11-bit mantissa and 4-bit exponent, radius R = 1 − (M·2^E)·2^−26
  (stability by construction), biquads −2Rcosθ, R². Runtime distortion is
  per-stage symmetric state clamping at ±Dist·1000 with pole-radius
  compression near the threshold — US10514883's runtime radius correction,
  found in the shipping binary — and morph coordinates slew through a
  one-pole filter (α ≈ 0.0109), the patent's click smoothing. Record
  layout (fourth pass, decoded from the unpacker disassembly at 0x080382FC
  and verified against the manual's filter descriptions): 12-byte name +
  320-byte payload — no header. The payload is 232 contiguous 11-bit fields
  (MSB-first within little-endian u32s): seven 44-byte stage blocks, each
  4 params × 8 corners in param-major order [pole angle, pole radius, zero
  angle, zero radius], then 8 per-corner cascade gains and 8 spare bits
  (bit 0 of the last byte is a runtime mode flag). A stored field is the
  top 11 bits (4-bit exponent + 7-bit mantissa) of the 15-bit runtime code;
  the unpacker refills the lost low 4 mantissa bits with 0xF. Decode:
  θ = ((M|0x800)<<E)·f32(π/2^27); R = 1 − denorm(M,E)·0x32800800f (field 0
  → R exactly 1.0, field 2047 → R = 0); gain = denorm(M,E)·0x338007FFf
  (1787 ≈ unity, 1535 = −12 dB). Interpolation is trilinear on the 15-bit
  codes as signed Q15, decode after. Corner index: bit0 = Transform2,
  bit1 = Morph, bit2 = Frequency — LPFlange.4's corners match its manual
  entry exactly at the 39,062.5 Hz datum (octave notches 49–1554 Hz,
  morph-max 11.8–18.5 kHz, freq-track +2 octaves). 54 of 109 ".4" cubes and
  28 full cubes hold the null corner across the t=0 plane; unused planes
  may carry leftover real data (Null Cube's odd corners are a mild filter).
  AllPoleDst2 stores two exact unit-circle poles — the one deliberate
  breach of stability-by-construction, tamed by the runtime clamp. All 289
  records decode cleanly under this law: ref/morpheus/cubes_decoded.json
  (`dev/decode_cubes_complete.py`). All 289 are imported as native 560-byte
  bodies at ref/morpheus/bodies/ (`author --bin import_morpheus`; axes
  Morph→M, Freq→Q, Transform2→Z; per-corner gain spread evenly across active
  sections; junk planes carried verbatim). The import nulls: re-encode
  idempotent, worst stage-coefficient error 3.3e-4, scale within 0.001 dB;
  roots at ~0 Hz with real radius are E-mu's DC-side shelf roots — real-axis
  pairs, same lesson as the v2 recipes. 243 of 289 exceed the safety gates
  (import_census.tsv) — more evidence on the open gates-versus-factory
  verdict; Morpheus leaned on runtime clamping the gates don't model.
  Earlier mode-dispatch, designer-record,
  and 32-byte-header readings of the payload were superseded by this
  layout.
  The transfer audio itself is verified here: biphase mark at 6 kbaud,
  _VCB1 at byte 749, 289 records of 332 bytes at 1090+332n, names in the
  first 12 bytes.
- **EmulatorX.dll** — the P2K/Proteus filter engine in software, re-disassembled
  here from the installed binary (`Program Files/Creative Professional/
  Emulator X/EmulatorX.dll`, PE base `0x180000000`), against the earlier reported
  extraction at `../trench-x3-clean/ref/ghidra_extracts/`.
  **The engine has exactly two ways to make a filter: compute it, or look it
  up.** The class dispatch table at `0x1806d5f80` holds 32-byte records whose
  MSVC RTTI names give E-mu's own hierarchy — 17 closed-form primitives
  (`CPhantomLP2Pole` … `CPhantomVocal2`), 5 user-programmable morph targets
  (`CPhantomMorph1`, `MorphLP`, `MorphLPX`, `Morph2`, `MorphDesigner`), and one
  table reader, `CPhantomFilterP2k`, which serves all 33 character filters. The
  17 and 5 match the ROM filter table and the manual's five PROG filters exactly.
  Recorded in `recipes/phantom_classes.json`, served on `/api/library` as
  `vocabulary.phantom_classes`; each stage-index member carries its `provenance`
  and `class`. Nothing in the binary fits or factors a response.
  **The 33 skins are stored, not compiled.** `CPhantomFilterP2k`
  (`FUN_1802d3ce0`) computes `0x1806d7610 + (skin*4 + rate)*0xf0` and copies
  five-word rows into the corner bank, no arithmetic on the words. `0xf0` is 240
  bytes — one P2K body.
  **The second index is the sample rate, not a variant.** `word[obj+0x0c]` is
  the same rate-family index every other class uses to select `base[]`/`slope[]`,
  and those tables hold four entries. So `../trench-x3-clean/ref/p2k_variants/`
  is 50 filters × 4 rates, not 50 skins × 4 variants — but what a rate slot
  holds differs by half of the table. **The 33 stored skins are one design
  pre-warped**: decoded at one fixed rate, every live pole of each bank is an
  exact scaling of bank 0's, and across all 31 skins with four comparable banks
  the ratios are `0.91878 / 0.45939 / 0.22969` against `44100/48000`,
  `44100/96000`, `44100/192000` — 31 of 31 on every bank, sd 1e-4 or better.
  The only base rate making that a real family is **44,100 Hz — the P2K datum**;
  bank 0 is the 44.1k bank, and the four slots exist so the runtime never warps
  z-domain geometry per rate. 39,062.5 Hz is Morpheus's datum and does not
  apply. The earlier X3 "xStream law" — banks are distinct designs per rate,
  selection not remapping — is therefore false of the skins; it describes the
  primitives, whose `base` and `slope` are not proportional across rates.
  **The table holds the 33 skins only.** `P2k_000`..`P2k_032` decode to crown
  spans of 73–376 dB; `033`..`049`, named for the 17 fixed classes, decode to
  172–637 dB — not filters. The reader's own arithmetic says why: it indexes
  `skin*4 + rate`, so 33 skins × 4 rates fills the bank and an extraction at
  33–49 runs past its end. Nothing needs to be there, because the 17 primitives
  are computed by their own `CPhantom*` classes and never table-read; their
  compiled output is the X3 frame bank, which does decode cleanly.
  **Radius is an affine function of frequency, in every compiler.** The writer
  the four morph classes share (`FUN_1802c59b0`) maps a descriptor byte by
  `v = slope[rate]*byte + base[rate]` (`base = [4896,4500,900,220]`,
  `slope = [442,440,405,350]` at `0x1806d73a0`/`0x1806d73b0`) and then takes
  `rad = (v >> 1) + 0x6400`. `CPhantomMorphLP` selects one of 16 twelve-byte
  profiles at `0x1806d73c0`; five of the six columns are strictly monotone —
  three falling, two rising — and the sixth wobbles by one lsb over four of its
  fifteen steps, so the profiles are an authored ladder. No compiler carries
  a per-section volume term — the shared writer emits the constant `0xdfff` as
  the fifth word. All cascade resonance and level derive from pole and zero
  polar coordinates alone; per-stage volume balancing is not used.
  **Morph Designer compiles by formula; it does not fit.** `FUN_1802c6590`
  reads six 6-byte records (type, freqA, gainA, freqB, gainB) and emits packed
  words by closed-form integer arithmetic. Frequency is sample-rate-family
  dependent, `freq = ((scale[family] * frequency_byte) >> 7) + base[family]`
  with `base = [18,18,4,1]`, `scale = [220,220,200,177]`; radius is a linear
  function of that frequency, `rad = ((freq * 0x7c) >> 8) + 0x76`. In Type 1 the
  gain term is an **opposed radius split at one shared frequency** — pole
  `clamp(rad + gain, 0, 255) << 8`, zero `clamp(rad - gain, 0, 255) << 8`, both
  at `freq`. Level from geometry rather than a gain term, as shipping
  arithmetic; it also explains the co-located coupling mode directly. Types 2
  and 3 subtract gain from the zero radius only and fix the pole word, so the
  symmetric split is Type 1's, not the grammar's. Only type IDs `1..3` compile;
  four or more valid rows force six rows, padding with
  `[0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000]`. Both endpoints are written twice,
  collapsing the Q axis. Because it is a formula and not a search, Morph
  Designer is no evidence for an internal factoring tool. Its law is the same
  one the morph writer uses: in word units `440*byte + 4608` against
  `442*byte + 4896`. Two independently written compilers, one law.
- **Not applicable.** EMU8000 programmer's guide; US5943427; US5952599; the
  NASA HRTF memorandum; Segers & Verhoeven 2005 (SLI perception study on the
  Kerkhoff synthesizer; its Table 1 carries published Dutch formant and
  bandwidth values for /o/ and /a/).

How the two lineages compose. The matching lineage (Fant/Mártony →
Bell/Fujisaki/Stevens 1961 → Kjellin → Kerkhoff → Klatt → ARMAdillo) reduces
one spectrum to the mechanism's coordinates: a production model, a
comparator, and a strategy in the loop — analysis by synthesis. The Martens
lineage reduces a corpus to the data's coordinates: PCA scores, inverse PCA,
perceptual bisection — the Palette loop is analysis by synthesis with the
ear as comparator. Bell 1961 closes by asking for a low-dimensional legal
search space above pole-zero space; score space inside the data ellipsoid is
that space, built statistically instead of articulatorily. Sandell &
Martens' prototype-plus-deviations, Bell's quasi-invariants, and the
factory's reused poses and held zero scaffolds are one principle. The
pipeline is the composition, a stack of three inversions: judgments ↔ knob
feel (rating models, bisection); corpus ↔ scores (inverse PCA, mean and
rotation); spectrum ↔ legal sections (the fit). A corner is score → curve →
sections → packed words. Taste enters only at the top, as judgments;
measurement in the middle; the hardware at the bottom.

## The breakthrough

In a serial cascade, a vowel is a lookup, not a search. Klatt 1980: "the
relative amplitudes of formant peaks for vowels come out just right without
the need for individual amplitude controls for each formant" — the amplitude
balance that makes a vowel read as itself is a property of the cascade
topology. Landing on a vowel therefore needs only formant frequencies and
bandwidths, and those are measured, not invented: the DVTD mouths' own peaks
(`recipes/tables/dvtd_formants.json`, regenerable via
`mouth_formants <dvtd dir> <out.json>`; peak-picking in `author::formants`).
In the response room, right-clicking a mouth seeds one pole lane per
measured formant (bandwidth → radius via the praat law) and sets that mouth
as the target — FIT then lands it, and the rms is the proof. Combined with
the measured factory economics (poses are built once, kept, and reused
across objects), each vowel is solved exactly once and becomes library
capital. This is the project's central working method: sections are not
built toward a vowel; the vowel's measured skeleton is placed and the
machine is held to it.

## The design form

The authoring abstraction above the recipes (`author/src/design.rs`,
`author --design <json> <out.body>` / `author --decompile <body> <json>`).
A design is up to seven lanes; each lane is an anchor (pole and zero as
conjugate `{hz, r}`, real `{pair}`, or `"@name"` from `recipes/alphabet.json`;
a zero may ride its pole at `interval_st`) plus moves per axis (M/Q/T:
`pole_st`, `zero_st`, `pole_r_to`, `zero_r_to`) — the four factory moves.
Corners are derived; a lane may instead carry verbatim `corners` (the
lossless escape hatch). Unresolved `@names` fail loudly. The alphabet
carries 78 anchors mined from the Morpheus cube corpus (bell
ladders, the octave notch ladder with its lowpass pole ladder, the Be-Ye
and Uhrrrah all-pole vowels, the ParaVowel A formant/control-zero scaffold,
the Vocal Cube comb, and the corpus's three structural roots — the idle
pole `[1909,2015,0,2047]` filling 1,124 stages across 160 cubes, the off
zero, and the 18.2 kHz top-shelf pole); every anchor cites its source
cube/corner inline, and the letters survive encode/decode at 0.21 cents /
3.1e-5 worst error. Its `radii` and `carves_st` books are **empty**, so any
design naming an `@name` radius or interval fails today; only anchors resolve.
All 51 factory presets round-trip bit-exact through the design form
(`design_null_check.rs`) — but `decompile` emits verbatim `corners` for every
lane, so that null exercises the escape hatch, not the anchor-plus-moves
grammar. What the grammar itself can express is unmeasured and is an open item.
Note: several factory objects exceed the audit
gates (TalkingHedz crown 43.5 dB, parity 37.2) — the gated writer refuses
them; the gates versus factory practice is an open verdict.

## Rules

Decisions, not findings.

- Assign lane ownership before fitting. The fitter never chooses lanes.
- Fit the complete serial cascade. Responses multiply and dB responses add, so
  error is measured on the sum of the section curves.
- Preserve correspondence. Pinning is what stops the optimiser from satisfying
  the sum by reseating a different section. Hold zeros as well as poles when a
  lane's job must not be re-dealt.
- Census the corpus unsliced, and judge it against a null built from the corpus
  rather than a synthetic one. Enumerate before summarising: keying on stage
  index, lane order or filter type has destroyed the signal every time it has
  been tried, and a synthetic log-uniform null gave a false negative that
  reversed under a corpus control. Report movement signed — a median of absolute
  values reads scatter as direction.
- Fix one liveness threshold per corpus study and state it. A conjugate root
  below it is the encoder writing "nothing here", not a root doing work;
  `dev/p2k_diagnostics.py` sets `LIVE_R = 0.45`, under which the P2K pole→zero
  interval median is 10.14 semitones.

## Proven here

Executable in `author/tests/canon.rs` unless noted.

- A factory corner rebuilds from its response curve alone: TalkingHedz M0_Q0,
  1024-point grid at its datum rate, cold fit under 0.5 dB RMS, rediscovering
  the unit-circle ceiling and the spanning shelf unprompted. An independent
  audit measured 0.42 dB under a different harness; both sub-half-dB.
  Character lives in the target curve, not in section rules.
- Pinning all poles without zero holds re-deals the zeros across lanes and
  worsens that fit to 0.93 dB (measured, not yet a pinned test).
- The pitch-corrected LTAS preparation subtracts structure broader than one
  octave — 23 of TalkingHedz's 82 dB, exactly the spanning-rise character.
  Never route a full-character target through it.
- Out-of-class targets: raw measured vocal-tract curves fit at ≈2.3 dB median;
  PCA reconstructions inside ±1σ fit at ≈1.1 dB; the built subject-1 cube's
  eight corners fit at 1.8–2.6 dB.
- The interior does not explode: over a 75-point interpolated 3D surface the
  crown exceeded the worst corner by +6.4 dB, within the crown band. With lane
  correspondence held, packed-word interpolation is tame.
- The factory workflow, measured from the null-verified bytes: ten full
  pole-sets are shared identically across preset families (TalkingHedz and
  UbuOrator share one entire six-pole /u/ pose; the vowel family shares /i/
  and /o/ poses; MeatyGizmo/Millennium share two) — a reused pose library,
  seated per object. Q100 corners are NOT rule-derived from Q0 (3 of 66
  corner-pairs show a uniform radius lift; 63 scattered) — the decoded
  q_lift/q_revoice fields were bookkeeping, not the method. Per-corner
  cascade gain is one shared figure at 105 of 132 corners and deliberately
  split at 27 (structured splits, e.g. exactly 12.0 dB). 22 of 33 presets
  cross lanes between M corners.
- The 33 P2K skins were fitted to whole responses, not assembled from sections.
  Measured over 132 corners and 792 non-identity sections, reproducible at both
  candidate datums: `dev/p2k_diagnostics.py` and `dev/p2k_primitives.py`, run
  against the `/api/library` payload. The sections cancel — Σ per-section dB
  span ÷ summed-cascade span has a median of 3.24, where the hand-authored
  Morph Designer XML control is 1.46; sections that cancel that hard are the
  output of something fitting the sum. Cascade gain is one figure divided six
  ways at 105 of 132 corners. Every preset carries a near-unit zero at S6 in all
  four corners (132/132, and 0% at every other stage) — a traveling notch,
  parked at 0.92·Nyquist in 48 of them; S6 is also the only stage whose zero is
  live in every corner. Its radius is **0.999998**, not 1.0: no P2K root is
  exactly on the circle (0 of 1530 conjugate roots), so the encoder's exact-1.0
  null is a capability the corpus never uses. The figure matters because
  `stages.js` clamps a dragged radius to 0.9999 and cannot reach it. Correspondence was never
  preserved: 104 of 441 lane pairs cross during the M0→M100 morph, in 22 of 33
  presets, and morph travel is unsystematic — signed median +0.38 st, rising in
  half of 187 lanes, magnitude median 14.5 st. Q narrows but is not a radius
  axis: over 377 pole pairs the bandwidth ratio median is 0.354 and 71% narrow,
  yet pole frequency also moves a median 6.85 st and holds within 10 cents in
  only 10%. Corners are not reused — level removed, at 1.5 dB RMS the 132
  corners give 128 clusters with a largest of 2, nearest-neighbour distance
  median 6.72 dB.
- The filter type is the only surviving statement of intent, and it is the key
  to the Q axis alone. The body stores no type, and the geometry cannot supply
  one — sections are factorization artifacts. But the catalogued type predicts
  which way Q moves the poles: measured over the 33 skins at 44,100 Hz, C0→C2
  narrows the pole in SFX 100%, VOW 94% (median bandwidth ratio 0.153, a 6.5×
  tightening), WAH 83%, REZ 76%, FLG 75%, LPF 73%, EQ+ 69% — while **EQ− widens
  92%** (ratio 3.87) and **DST widens 100%** (3.94). The corpus sorts into
  narrowing kinds and one widening pair. M does not work this way: over the same
  lanes the medians are LPF −13.1 st, EQ+ +4.7, REZ +7.9, VOW +1.6, but the IQRs
  are 41.6, 30.8, 39.7 and 14.7 — the spread swamps the direction, so morph
  travel is authored per object, not per kind. Type is therefore an authoring
  constraint on Q and an index for kinds; it is **not** an index for sections —
  slicing the section census by type is what made 51 real recurrences read as
  none.
- A P2K section vocabulary exists, is thin, and is visible only unsliced. Keyed
  stage-agnostically in 10-cent frequency and 85-cent bandwidth buckets, 792
  sections collapse to 728 (8.1%); 51 distinct sections recur across presets,
  covering 113 of 792, top count ×4 (pole 479 Hz r 0.976 with zero 13.4 kHz
  r 0.884, shared by BassBox-303, BassTracer, BolandBass and LucifersQ). The
  sharing is family resemblance, not a shipped alphabet — MeatyGizmo·Millennium
  share 10. Slicing hid it three separate times: keying on stage index took
  cross-preset pole clusters from 64 to 100 when dropped, ordered pose matching
  found 5 shared poses where unordered found 10, and 51 recurrences spread
  across 10 types × 6 stages read as none at all. Built and served by
  `author-server/src/vocab.rs` as `vocabulary.states` on `/api/library`.
- The null: all 33 architecture recipes recompile to their factory preset
  bytes bit-identically (`author/src/bin/null_test.rs`; bins recovered from
  git). The v1 conjugate-only schema had corrupted 21 of 33 — the factory's
  shelf and rolloff letters are real-axis root pairs it could not express
  (`real_pair_census.rs`; response error tracked the real-pair count).
  The v2 recipes carry `StageGeometry` (conjugate `{hz, r}` or real
  `{pair: [a, b]}`, exact `scale`), were regenerated from the bytes in the
  factory's own lane order, and geometry round-trips the entire 51-preset
  corpus bit-exactly (`roundtrip_check.rs`). The null is the acceptance test
  for any high-level form or import door: what goes through must null.

## Canon

- The editor is an interactive inverse-design system for constructing legal
  encoded serial-filter corner states. The whole response is the comparator;
  the ordered sections are the realization; selection and hold define the
  free variables; direct manipulation and FIT are two ways of changing those
  same variables; the user judges the result. Runtime interpolation is fixed
  and outside the authoring problem.
- Generators author target curves; the fitter meets them. Components never
  become lanes.
- PC space is an endpoint factory: corner scores → inverse PCA → one target
  spectrum per corner. Corner scores stay inside the data ellipsoid. Store the
  mean and rotation beside anything that stores scores.
- Corners are fit. Pairing and travel are authored. The interior is fully
  determined by corners and pairing: it is ridden and heard, never specified,
  scored against a path, or warped inside the body. Knob feel is calibrated by
  Higher/Lower bisection into a lookup table stored outside the body.
- Audits are safety gates only: stability, crown bounds (−3..36 dB), crown
  parity (≤30 dB).
- An object is judged by the cascade inspector's four views: riding sweeps,
  per-section jobs (travel · level), signal so far, and corners with lanes.

## Removed

Invented, unsupported, and not to be reintroduced: response-curve dragging
with an influence width; the two-segment morph warp; a `station.json` project
format; the laws vocabulary; the corner and section naming grammar;
golden-angle corner hues; a plain radius/angle pole view in place of the
documented mapping; interior conformance audits against PC-path midpoints;
per-section generator rules (a unit zero on every section, the valley rule,
synthetic radius-lift corners).

## Open

The WAV → envelope decoder is built and live (`author/src/wav.rs`, 577 lines,
untracked in git): Welch PSD with f0-aware smoothing, peak picking, AR(14)
seeding, and a time-slice window, served as RECORDINGS and driven by the
ANALYSIS dock. It does **not** route through `prepare_pitch_corrected_ltas`, so
the spanning-rise warning above does not apply to it. Note that `ar_poles` is
all-pole by construction — on material with real notches it will stack false
poles to fake a shear, so it seeds, it does not fit. The Palette
rating and respacing round — Martens 2002a Higher/Lower bisection into a knob
lookup, and the eight-at-a-time audition flow specified in
`trench-x3-clean/NEXT_SESSION_EDITOR.md`. Program-material audition (the
editor plays pink noise today).

**The P2K datum is settled at 44,100 Hz** by the rate-family proof in
Documented, and is no longer open. What remains open is the code: the analysis
rate for the P2K corpus still comes from `author::extrude::AUTHORING_SR`
(39,062.5, legacy despite its name) at `author-server/src/store.rs:204`, so every
displayed and keyed P2K frequency is low by 44100/39062.5 = 1.129. Changing it
moves the recorded verification peaks in frequency — dB values are
rate-invariant, the frequencies are not — and it re-keys the bandwidth buckets
in `vocab.rs`, so the census must be re-run after. The constant itself is load
bearing in `extrude.rs`; introduce a separate P2K rate rather than editing it.

There is no 4× variant corpus. That reading of the bank pointer was wrong; see
Documented. One cheap falsification still stands: does any P2K section satisfy
Morph Designer's `rad = ((freq * 0x7c) >> 8) + 0x76`, or the morph writer's
`rad = (v >> 1) + 0x6400`, in packed-word space? It should not — P2K and X3
already share 0 of 792 sections — and any subset that does was machine-generated
by that law rather than authored.

**X3 is a frame bank, not a section bank.** 17 filters at
`../trench-x3-clean/ref/x3_menu/runtime_blocks/`, 68 files = 17 × 4 rates named
`<filter>_<rate>.raw`, byte-identical to the `X3F_*.json` banks. Each file is
one complete compiled filter, because each is the output of a `CPhantom*`
closed-form compiler that writes a whole corner bank in one call. The unit is
the frame; its sections are what falls out of factoring the compiled result and
were never authored on their own. So the earlier reading — that X3's 124 → 79
distinct sections (36%, 4.5× P2K's) made it the source for a *section*
vocabulary — was wrong in kind: the collapse is high precisely because one
formula family with shared `base`/`slope` tables generates all 17. That is the
formula repeating, not a designer reusing a letter. Vendor it as frames.
Verified: `[4 corners][active_stages][5 words]`, corner order stated in the
`X3F_*.json` banks, raw blocks holding the same numbers, and 4 Pole Lowpass
being the 2 Pole Lowpass section twice. It **decodes under the ordinary
minifloat law** — feed `geometry_from_words_at` the bank values directly, with
no byte swap — and renders textbook curves: 2 Pole Lowpass reads −9.5/−43.4/
−84.4 dB at 100/1k/12k at M0_Q0 and flat at M100_Q0; 4 Pole Lowpass is exactly
double in dB, confirming order 04 = the order-02 section twice. Real-axis pairs
appear at the closed and wide-open corners, which is the shelf/rolloff letter
doing its job, so a decode check must not require a conjugate pole everywhere.
Read only the 44.1 k bank: frequency is rate-invariant — 2 Pole Highpass holds
54.7 Hz across all four banks and drifts 0.8 cents at 192 k — but bandwidth-Hz
is rate-invariant only for resonant roots and degenerate for heavily damped
ones, so no key merges the banks (10 of 60 collapse; raw radius, 0 of 60).

**Two debts behind the cancellation figure.** The 1.46 hand-authored control is
computed over derived four-corner bodies, but Morph Designer authors two
endpoints and writes each twice — the control may be measuring that duplication.
And the semitone-snap claim was never tested against a 7-bit lattice null; XML
frequency is a 0–127 integer, so the snapping may be forced by the encoding.

**An internal E-mu design tool is inference, not measurement.** The cancellation
ratio, the crossed lanes and the abandoned correspondence all point to a tool
that showed response curves rather than sections. Nothing found is proof, and
the shipped software no longer offers any support for it: every one of the 50
filters is now accounted for — 17 computed by closed-form primitives, 33 read
from a table — and the five morph classes are hand-authoring grammars whose
manual (Emulator X reference, p.149; Adv Apps Guide pp.43–45) describes setting
frequency and Q per section at two morph positions, with no target curve and no
fit. So Morph Designer is not a cut-down descendant of a fitter; it is its own
thing. Proof of the design tool needs internal spec files or a factoring
algorithm in an E-mu OS or tooling binary. No Proteus 2000 /
Mo'Phatt hardware OS image is on this machine. Dead ends already checked:
`~/Downloads/extracted_firmware/` is Vulcan, already mined above;
`~/Downloads/e-mu_eos_technical_documents.pdf` extracts cleanly but is a
hardware service manual whose zero hits for filter/pole/coefficient are real,
not an extraction artefact — do not re-grep it.
