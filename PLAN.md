# Workstation plan

Specification: `workstation/CLAUDE.md`. Measured facts: `dev/EVIDENCE.md`. This file is
the build order.

## The authoring model, in one line

**You author poles. FIT authors zeros.**

Every measurement this session reads as noise if zeros are authored geometry, and as signal
if they are solver output: the zero/pole ratio has sd 9.99 across cells; 0.0% of lanes hold
a constant zero-pole interval across corners; 143 of 144 vowel cells carry a zero anyway;
the high-frequency asymptote sits at −6.46 dB with sd 6.6 rather than at any normalisation
point; sections cancel 3.24× and all 198 stage-quads are unique. Those are the fingerprints
of a comparator minimising error over the sum, not of a designer placing anti-resonances.

Manual zero sculpting stays available — that is what the parked handles and ROOTS are for —
but it is the override, not the method. The laws already express this: `freedom[0]` is
frequency and `freedom[2]` is the zero, and `zeroHeld()` already reads it. Nothing yet sets
poles pinned / zeros free, which is the default FIT should offer.

## Where the tree is

- **Step 1 done.** All modules TypeScript; `tsc`/Biome clean; Vite builds the worklet as
  its own chunk; `author-server` serves `workstation/dist`. Parity green — 521 cases, words
  and rows exact, worst 2.05e-12 dB — and on that evidence `workstation/js` was deleted.
- **Step 5 mostly done, out of order**, because the migration alone changed nothing the
  operator could feel. Corner selection binds, jumps the audition point, writes no undo
  entry. Targets belong to corners. FIT is one atomic command with a corner+generation
  stale guard; ACCEPT is gone. One `playing` flag, latched transport. Reversible section
  OFF. Binary lock. Terse status. `pole_ceiling_r` as the single ceiling. One repaint path.
  **None of it is covered by a test.**
- **Simplification pass done.** Stage roles deleted outright — `fit.roles.ts`, `doc.roles`,
  `Slot.roles`, the `F1..Fn` guide lanes and the role-invalidation branch inside FIT. A
  section is a factor in a constrained realization, not an acoustic object, so nothing
  labels one. The floating "working" target and its bind hook are gone: a target is
  `Slot.target` and nothing else, which also removed LAND. Write-only state deleted
  (`doc.rms`, `doc.packing`, `doc.rmsStale`, `doc.cornerWords`, all `peaks`, `Slot.held`,
  `Slot.cloned`). Duplicate verbs deleted: the FIT panel (a second comparator over the same
  doc), KEEP, the `n`/`w` keys, corner-hold, cell-dblclick OFF. Dead code deleted: the
  worklet grit path, four render exports, `seatTemplate`, seven `api` wrappers, ~120 lines
  of unmatched CSS. Two real defects fixed: the cascade panels rendered at 1× on HiDPI, and
  the response drag scale was off by 1/0.76 because its inverse used the full canvas height
  instead of the plot's. `curveInto` and `sumCurve` were two different summations of the
  same curve and are now one.
- Deleted: `wire.js`, `cube.js`, `ride.js`, `project.7z`, `ws.rs` and the `/ws` route,
  `inspectors/`, root `tests/`, `plots/`, `plotdata/`, `target-ws/`, `roots.ts`.

## The one change everything else waits on

**One authored representation, carrying real-axis pairs.**

`target.rs` matched on both roots and fell to `StageRoots::IDENTITY` if *either* was a real
pair — so `vocal_ah_ay_ee` S1, a conjugate pole with a real zero, arrived with its pole
deleted too. That is patched to keep whatever can be expressed, but the fork itself
remains: `doc.lanes` and `slot.words` are still two editable representations.

`trench_core::stage_law` already has the whole model — `StageGeometry`, `RootPair`, and
both `geometry_from_words_at` and `words_from_geometry_at`. Nothing needs inventing.

## Order

**1 — TypeScript migration.** Done.

**2a — One canonical geometry. NEXT.** Serde on the two types; `target.rs` / `response` /
`fit` carry geometry instead of flattening; the workstation lane type becomes that geometry
and defers to `words_from_geometry_at`.
*Proof:* for every body in all four corpora, words derived from loaded geometry equal the
file's own words bit-exactly across 8×7 cells. A null test, not a smoke test.

**2b — One legible owner.** Not a file split; for ~4k lines a monolithic `main.ts` is fine.
The defect is distributed ownership. `main.ts` holds the operation lifecycle, the only
writes to authoring state, and the only `propagate()`. Views render and emit events.
`field.ts` stops calling `commit`. Nothing outside `main.ts` calls `setCorners`.

**3 — ts-rs contracts**, after 2a so they describe the final shape.

**4 — Playwright.** Was meant to precede step 5 and didn't, so it now covers behaviour that
already exists. Session to cover: open body; Space plays and stays playing; click a corner
(target/response/sections/roots follow, position moves, no undo entry); drag M/Q; drag a
root (one undo entry); analyse a WAV; FIT (one result, one undo entry, no ACCEPT, audio
uninterrupted); Escape during FIT leaves bytes unchanged.

**5 — Interaction.** Remaining: undo still snapshots session state; `Shift+F` batch fit in
Gray order; the corner-swap audio dip (suspected worklet duck — measure first); whether
section OFF persists into a written body; poles-pinned/zeros-free as FIT's default law.

**6 — Layout, centred on the comparator.** See below.

**7 — Wasm kernel.** Extract `trench-kernel` and a thin `trench-wasm`. Keep JS until the
Wasm path passes the same vectors, then switch authority and delete it. Same kernel in the
worklet, preserving message and smoothing semantics exactly. Parity converts to
native ↔ Wasm ↔ worklet.

**8 — Server transport.** Axum + Tokio + Tower replacing the hand-rolled TCP/HTTP/JSON;
fitting on a bounded Rayon pool with cancellation. Streaming NDJSON is the delicate part.

## The comparator is the surface — Bell et al. 1961, Fig. 7

Input spectrum, generated spectrum, difference curve, numerical error — together.

*Landed:* RESPONSE draws target, generated cascade, and an explicit residual curve in its
own strip at its own scale with the RMS figure.

*Landed, and it supersedes the "strip of seven plots" this section used to call for.*
Because `20log|H| = Σ 20log|H_i|`, a serial cascade behaves additively on the dB plot, so
the response **is** the editing surface and the seven miniature plots were redundant:

- RESPONSE carries target, complete cascade, the selected section's own term, and the
  residual strip. Each section has a solid pole handle and a ring zero handle sitting on
  the curve at its frequency, with a faint connector. Horizontal drag is frequency,
  vertical is how hard the root pulls, the wheel changes that without moving frequency,
  double-click on empty space allocates a free section and on a handle clears its section.
  The selected section reads at full weight; the others are faint context.
- CHAIN is a compact identity/ordering strip — `INPUT → S1 → … → [S3] → … → OUTPUT`, one
  verb row, drag to reorder. Position still identifies a section; no curves.
- **ARMADILLO replaces ROOTS.** Not a restyle: the axes are the packed 16-bit words
  themselves, straight out of the parity-tested encoder. X is the frequency word, Y the
  resonance word, drawn with hex ticks so the space is unmistakably machine space. Equal
  word steps are equal pixel steps, which is the only space in which the runtime's
  `lerpU16` is straight — so the dashed corner-travel path drawn for the selected section
  is a genuine diagnostic: if it bends, the plot is lying. Dragging inverts through the
  same `decode`, and a real-axis pair is drawn but refuses the drag rather than being
  silently flattened. The ceiling is `encode(1 - r²)` of the kernel's own ceiling.
- Targets are smoothed to 1/3 octave on assignment. A measured spectrum carries harmonics,
  analysis ripple and source structure that seven second-order sections cannot and should
  not reproduce; the fraction is named in the target chip rather than hidden.

**Direct manipulation and FIT are the same operation** — human-driven and machine-driven
search over the same variables against the same comparator.

## The analyser is three layers, and it is not built

`MEASUREMENT → ANALYSIS → TARGET → REALIZATION`, with a literal visual flow and no hidden
jump. Measurement is waveform, spectrogram, cursor, time selection. Analysis is spectrum,
average, envelope, LPC, formants, pitch, source correction. Realization is what now exists.

Automatic *analysis* is fine — compute LPC, formant candidates, cepstral envelope, local
peaks. Automatic *interpretation* is not: "this is a vowel, so S1 is F1 and the zero goes
3.7 semitones above it" is invented ontology, and that is why the role vocabulary was
deleted and why the fixed +2-semitone zero seeding in `boundAndNormalize` went with it.

Every analysis result should be draggable into authoring: click a measured peak, drop it on
a section, and that section's pole takes that frequency. Drop a notch on a zero only if you
actually want to assert that correspondence. Otherwise leave it and let FIT find zeros.

The loop is measure → inspect → manipulate → synthesize → listen → compare, continuously.
The machine computes; the operator judges.

## Timbre space — the corner chooser

Choosing which four states define a body and riding between them are different acts. A
corner's M/Q position is not a choice; the 2×2 grid is four slots. Where corners have
positions is timbre space. PCA over corner spectra is measured: PC1 tilt 72.7%, PC2
2–5 kHz presence 14.0%, PC3 formant balance 4.6% — 91.4% in three components.

Build order: `author::pca::fit(rows, keep) -> Basis` exists. Missing is the index — compute
each factory corner's response on a fixed log grid, fit three components over ~2,800
corners, project each, serve id + corner + coordinate. The UI is a scatter with an inverse
query on click: seat the nearest *real* factory geometry, then FIT. A lookup with a
citation, never an inverse map.

**Martens is about control laws, not synthesis.** The musician adjusts a parameter until
the sound sits where they want it perceptually; the resulting relation is then inverted so
a desired perceptual value drives the synthesis parameter. Three consequences here:

- When the physical model is too complex to predict perception, the answer is to collect
  judgments, fit a prediction model, invert it, and ship the inverse as a **lookup table** —
  which is exactly the "lookup with a citation, never an inverse map" rule above.
- Sampling is adaptive. After the first round, use the fitted function to choose the next
  round's machine coordinates so the stimuli are *perceptually* evenly spaced. A uniform
  sweep wastes measurements where the ear is dull and undersamples where it is sharp. This
  is how interactive breeding should pick its four trials, not uniformly.
- The perceptual coordinate is an **interchange space between different generators**. Match
  a Morpheus form and a P2K body to the same perceptual standard and they become
  comparable despite different encodings, datums and section counts. That is the only
  honest bridge across the four corpora, and it is the human twin of FIT: adjust B until it
  matches A, against one comparator.

**Prototype origin (Sandell & Martens).** Rather than a raw mean, which carries the
idiosyncrasies of whatever happens to be in the set, take the eigenvectors of the pooled
within-groups SSCP matrix to get a prototype holding only what the whole corpus shares —
grouping by filter type or by preset. Subtract it from every corner, then score the
deviations on three components. The origin then means something, and distance from it means
"how far from typical". Note the data here is already spectral profiles across a corpus
(corners × frequency bins), so the temporal/spectral transpose distinction does not bite.

Two constraints: Q corners are queried, not calculated (type gives Q direction only). And
the map must never draw the morph path — the interior is `lerpU16` on words. Plotting where
interpolated responses actually land is legitimate; an idealised trajectory is not. A
prototype is an origin for the *map*, not for the body: the runtime interpolates absolute
words, so authoring as deviations changes nothing it does.

## What the sources settle

**Rossum, "ARMAdillo Coefficient Encoding"** — primary source for the format. Validates
three things previously taken on faith:

- `R′ = 20 log₁₀(1/(1−R))` — the ROOTS vertical axis is his mapping.
- The 40 Hz plot floor is his musical-octave reference.
- *"It linearly interpolates the coefficients in the encoded space at the sample rate."*
  Word-space interpolation is the encoding's designed purpose, not a quirk to work around.
  Resonance height is linear in the encoded word: `p = 8.68·k₂ + 6.02`, ~1 dB error.

**Mo'Phatt manual.** 50 filters in ROM, corroborating the 50-entry table. Order documented
per filter, and order = filter elements, so order 12 = 6 biquads. Critically: **"The Q
parameter can be modulated only at note-on time."** M is a continuous performance axis; Q
is not. The four corners are two morph pairs, one at each Q — which is why the manual
describes the Z-plane filter as two frames with one Morph axis.

**Morph Designer UI.** Up to six 2nd-order sections, each Lowpass / Highpass / EQ. The user
sets static Freq and Gain/Q for a Lo Morph and a Hi Morph state; one wheel interpolates
between them. Six sections, two endpoints — matching the compiler's six 6-byte records
(type, freqA, gainA, freqB, gainB).

**Peak/Shelf Morph tutorial.** FilFreq drives Morph, FilRes drives Q, confirming the axis
semantics from the user side.

**IRCAM envelope interpolation.** Extracting partials misaligns f₀-invariant formants under
pitch variation; resampling amplitudes onto a uniform grid with a fixed frequency limit
reduces reconstruction error. Our targets already work this way — `DRAW_GRID`, 1024 points,
40 Hz to 16 kHz — so the response grid choice is validated rather than incidental.

**Hardware residual, confirmed.** Four TalkingHedz corner captures deconvolved against
bypassed pink noise identify their own corners 4 of 4, at 0.28–0.95 dB rms over 1–15 kHz,
with 7–20× margin over the next-best corner. The same test at 39,062.5 Hz gives 6.6–12.4 dB.
This confirms the decode, the response evaluation, the 44,100 Hz datum, and the corner index
law together.

**Open:** 7 of 17 X3 filters have fewer live sections than the documented order implies —
ContraBand, Swept ×3, PhazeShift ×2, bat_phaser. 10 of 17 match `order/2` exactly. The
manual confirms the discrepancy is specific and real rather than a decode artifact.

## Literature: what applies, what does not

**The "follow" rule is measured false here.** Across 164 P2K lanes with a conjugate pole and
zero in all four corners, the zero–pole interval varies by a median 23.64 semitones within
a lane; 0.0% hold it within 1 st. That spread matches the poles' own travel, so they move
independently. It also cannot be a runtime behaviour — the machine interpolates words and
has no mechanism to hold an interval mid-sweep. At most an authoring aid: offered, never
enforced, never applied to a lane that already owns geometry.

**Cascade transients are real for our audition path.** A zero's amplitude can jump on a
parameter change and feed the next section's feedback. The block ramp, the envelope limiter
and the duck are the mitigations; measure a fast sweep before changing any.

**Pearson's arc-length cost belongs to the analyser, not FIT.** It is formant extraction —
better-conditioned than LPC because it stops source characteristics being modelled as
poles. That is `api.skeleton`'s job. FIT fits a seven-stage cascade to a target curve.

**Nasalisation is where acoustic zeros are real.** For non-nasal vowels the zeros are skirt
management, consistent with the measurement: +8.9 dB at 16 kHz, ~92 dB of cascade gain
removed.

**Not claimed:** that stages are autonomous self-normalising cells. Bell et al. model global
interaction, and the −6.46 dB asymptote has sd 6.6 — which argues against normalisation.

## Feature backlog

- **Scrubbable history** (after the lifecycle). A body is 560 bytes, so a session is
  kilobytes; render undo as a filmstrip scrubbed with audio playing.
- **Morph X-ray** (step 6). The whole morph as a contour field — M across, frequency up,
  magnitude as colour. The only view that would show the travelling unit-circle S6 notch
  that 28 of 33 P2K presets carry, and it makes dead zones visible as bands.
- **Interior audit strip** (step 6). `interior_crown_db` along the morph axis. The path
  cannot be rerouted, so the value is seeing which endpoints to move.
- **Corpus conscience** (needs the index). Nearest factory corner and the distance to it.
- **F1/F2 target surface** (needs the index). A tongue-coordinate plane over
  `recipes/tables/phonetic_formants.json`, synthesised into a target met by FIT. A target
  generator, never a geometry generator: stages are never slaved to covary, because no
  cross-stage covariance is measured.
- **Interactive breeding** (with the chooser). Four trials, pick by ear, repeat. Mutate
  along the measured PC axes, not randomly; round one can offer the four nearest real
  factory corners. Every trial is authored geometry passing the stability ceiling. Not
  blocked on Wasm — one cascade evaluation is already well under a millisecond.

## Not doing

Analysis and fit stay modules until the dependency graph justifies crates. No Lit, no
Tauri, no threaded Wasm, no WebGPU, no new top-level directories.

## Standing constraints

Never altered: the packed-word interpolation law, the corner index law, the encoder, the
solver objective, lane ownership, section order, the response grid, the 44,100 Hz P2K and
39,062.5 Hz Morpheus datums, parity behaviour.

Every replacement keeps the old implementation until the new one passes the same vectors,
then switches authority and deletes the old.
