# Project instructions

One document. What the object is, what the sources establish, what we decided,
what is proven here, and what was removed.

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
carries 73 measured letters mined from the Morpheus cube corpus (bell
ladders, the octave notch ladder with its lowpass pole ladder, the Be-Ye
and Uhrrrah all-pole vowels, the ParaVowel A formant/control-zero scaffold,
the Vocal Cube comb, and the corpus's three structural roots — the idle
pole `[1909,2015,0,2047]` filling 1,124 stages across 160 cubes, the off
zero, and the 18.2 kHz top-shelf pole); every anchor cites its source
cube/corner inline, and the letters survive encode/decode at 0.21 cents /
3.1e-5 worst error. All 51 factory presets round-trip bit-exact through the design form
(`design_null_check.rs`). Note: several factory objects exceed the audit
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

A WAV → envelope decoder (DVTD transfer functions bypass it). The Palette
rating and respacing round — Martens 2002a Higher/Lower bisection into a knob
lookup, and the eight-at-a-time audition flow specified in
`trench-x3-clean/NEXT_SESSION_EDITOR.md`. Program-material audition (the
editor plays pink noise today).
