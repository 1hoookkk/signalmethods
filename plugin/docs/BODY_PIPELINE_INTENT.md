# BODY PIPELINE INTENT — verified 2026-08-06

## Canonical conditioning decision — 2026-08-11

### Authoring-process correction — sequential quartets

`design/FOUR_CORNER_AUTHORING.md` now owns the authoring process. A body starts
from a hash-locked four-corner response target sheet (or an explicitly
collapsed two-target Q surface), then commits S1 through S6 sequentially. Each
commit is one atomic four-corner stage quartet fitted against the residual of
the packed previous serial cascade. Section responses and gains multiply; the
stages are not independent EQ bands.

Endpoint formant tables, generated Q attitudes, zero recipes, and the
two-source Corner Station are now constraints or sketch tools only. They do
not constitute four target transfer functions and cannot pack or promote a
canonical body. Any lower section of this document that describes direct
table-to-four-corner generation is historical and is superseded by this
paragraph.

The canonical registered-lane route may use an exact isolated P2K biquad when
that section is deliberately selected for a conditioning job. Its architecture
file identifies the lane; the exact P2K bank, bank SHA-256, rate-evidence
manifest, 44.1 kHz datum, and one-based lane number are recorded in the IR.
Every other lane remains our measured geometry. TalkingHedz S1 is the first
grounded case: the air pole plus traveling low zero gives a basic measured
vowel the required broad frame.

This is a narrow exception to the older "never roots" language below. It does
not make S1 or S6 universal conditioning seats, and it does not authorize whole
P2K rows or bodies as compile sources. Conditioning is established by isolated
and whole-cascade response evidence per family.

For Emulator X, `P2K_RATE_BANK_MANIFEST.json` is the datum authority: the
binary rate selector maps bank indices 0/1/2/3 to 44.1/48/96/192 kHz, and the
P2K loader selects `rate_index + 4 * skin_index`. The stored TalkingHedz donor
is bank 0, so it is decoded at 44.1 kHz without frequency or radius conversion.
The older 39,062.5 Hz dossier view is not numeric authority for this X3 bank.

One file that captures what this pipeline is for. Read this before touching
`tools/batch_compiler.py`, `recipes/tables/academia/`, or the Workstation
source picker. Anything that contradicts an earlier handoff doc supersedes it
— several earlier docs were AI-generated and wrong.

## The source of truth: trace back to INTENT, always

The chain is fixed and this is the layer order:

1. **INTENT** (`recipes/INTENT.md`, `dossiers/characters/INTENT.md`) records
   how E-MU **described** the reference filters — the one-liner, the IS/MORPH/
   Q/RECIPE per filter. That is the intent.
2. **Dossiers** (`dossiers/characters/P2k_*.json`) are the ground truth for
   **seeing the actual SOS biquads** — the decoded pole/zero frequencies,
   radii, and words per corner. They show what the real filters numerically
   ARE. We read them to verify the grammar, check the conditioning, and
   confirm the Q behaviour the description implies — never to copy rows.
3. From the description, verified against the actual biquads, we derive BOTH
   the **stage grammar** (lane roles per archetype) AND the **Q behaviour**
   (arm/defuse/revoice per filter).
4. We apply that derived grammar + Q behaviour to **OUR** data (measured
   endpoints). The reference bytes are never a compile source (clean-room).

If a description and a decoded byte disagree, the description is the intent —
the dossier row is evidence about what E-MU actually built, not an override.

## The job — X to Y, the sky is the limit

A user picks **any two endpoints** (X → Y — low morph and high morph) and the
pipeline compiles a certified `.body240` against the P2K lane grammar. The
whole creative space is combining: any object from `recipes/tables/academia/`
(or any extracted WAV) is a valid X or Y. One canonical folder per body. The
Workstation lets the user select the two endpoints and edit the Q corners
before anything packs.

## The flow (fixed)

```
two endpoint tables (M0, M100)          # freq + bandwidth per mode
  1 INGEST   load the two endpoint tables
  2 TRACK    pre-fit Hungarian on raw peaks (the one permitted use)
  3 GRAMMAR  P2K lanes: S1/S6 conditioning (static), S2-S5 the four
             fundamentals; voice zero at +6 st with serial-cascade clearance
  4 Q CORNERS  NEVER measured. Q100 comes from the documented E-MU Q
             behavior (tools/q_attitudes.py). Default = Q-collapsed.
  5 BLUEPRINT  .json + .txt — the HOLD POINT (user edits in Workstation)
  6 COMPILE  FFI encode @ real rate -> pack -> unity-DC -> certify 33x33
```

WAVs are allowed as input ONLY to be reduced to endpoint tables first via
`scipy.signal.find_peaks` (extract fundamentals, freq + −3 dB bandwidth).
**Never ARMA-fit a WAV; never joint-refine; never fit at all.**

## Verified ground truth (from decoded P2K dossiers, NOT handoffs)

- `dossiers/characters/P2k_*.json` are the ground truth (decoded ROM rows).
- **S6 zero is r = 1.0000 in 100% of corners across ALL archetypes.** It is
  the terminal frame's unit zero. Universal.
- S1 = HF frame (~7 kHz median), its zero sits BELOW its pole.
- S6 = the throat (low, in VOW/REZ). NOT universal — in WAH, S6 is the high
  cap (11 kHz). Use per-family medians, never one fixed pair.
- Conditioning medians (measured here, talker trio, n=12):
  S1 7374 Hz r 0.9642 / zero 2217 r 0.9489
  S6  371 Hz r 0.9983 / zero 13732 r 1.0000
- Family conditioning medians: VOW S1 5699 r .994, S6 1322 r .997 · REZ S6
  724 r .963 · WAH S6 11059 r .908 · LPF S1 9634 · EQ S1 10294 r .972.

## Clean-room meaning (this is how we use the word)

Clean room = **not byte-for-byte E-mu/P2K corner rows.** Peer-reviewed
academia and open instrument data are fine sources. Conditioning anchors are
MEDIAN statistics (derived), S2–S5 carry OUR measured data — clean-room by
construction. Never ship a verbatim ROM row.

## Recipe → zero architecture (the missing piece — 2026-08-06)

The compiler must turn measured poles into a coherent P2K-style cascade, and
the recipe is what makes that deterministic. **The recipe must carry enough
zero-policy for the compiler to generate every zero itself.** A hand modifies
only what fails perceptually — never types raw zero frequencies for all 24
stage-corners, never tunes a section in isolation, never invents notches
numerically.

Zero policy per lane (the four recognised species, from the ROM corpus):

- **valley** — offset above its pole (default +6 st), with serial-cascade
  clearance enforced: e.g. pole 1800 Hz, `recipe = valley, +6 st` →
  compiler places ~2546 Hz, then checks clearance against every other lane.
- **razor** — offset beside the pole (small / slightly negative), married.
- **parked-high** — compiler chooses the family's permitted parked region
  (family-specific; ZERO_PARK habit), no carve.
- **fixed boundary** — compiler preserves the same zero coordinate across
  X → Y (a held silhouette zero).

Motion policy per lane: **married** (rides its pole), **fixed** (stays), or
**counter-moving** (moves against its pole to preserve the silhouette —
Stage Law). Default for a voice = valley + married.

So the authored intent per lane is exactly one row:

> What rings (pole), what is removed (zero species + offset/region), what
> moves (motion species), what stays, what Q changes (attitude).

`+6 st with serial-cascade clearance` in the compiler is the **default valley
recipe, not the complete authoring philosophy.** New recipes add zero-policy
to that default.

### The practical process (the final three presets)

1. Select X and Y measured endpoints
2. Track and assign poles to persistent lanes
3. Select the relevant P2K-style recipe
4. Let the recipe generate the initial zero architecture
5. Generate the Q corners from its Q attitude
6. Inspect the complete blueprint
7. Manually adjust only the zeros or Q moves that fail perceptually
8. Normalize, certify, audition, ship

No large zero-editing system is built before shipping. The final three
presets have their zero plans authored directly in their blueprints.

## SCALE law

Broadband gain is a **closed analytical normalization, evaluated post-hoc**,
forcing unity gain at DC (z=1). One 1/6-root factor per corner over the six
SCALE words. Never a search/optimization parameter. (`dc_anchor_body` does
this; `tools/batch_compiler.py` applies it unconditionally.)

## Q corners vs the Q wheel

The Q **wheel** is the runtime gesture (interpolation). The Q **corners**
(M0Q100, M100Q100) are authored coordinates. We do not measure Q — we derive
the Q100 corners from the documented E-MU Q behavior:
- `recipes/tables/p2k_filter_q_behavior.json` (per-filter rail)
- `tools/q_attitudes.py` (arm / defuse / revoice laws, cited)
- Default Q100 = Q0 (Morph Designer has no Q axis; `NEXT_SESSION_X3_CARTRIDGES.md`).

## Endpoint table format (machine readable)

One JSON file per dataset at `recipes/tables/academia/<dataset>.json`,
schema `acoustic-source-v1` (see `recipes/tables/academia/SCHEMA.md`). Each
`object` = one selectable endpoint, `formants` = ascending F1–F4 with
`frequency_hz` + `bandwidth_hz`.

**Bandwidth is a hard requirement, not an optional field.** The whole
resonance character comes from `r = exp(-π·BW/Fs)` — without measured
bandwidth a lane is just a frequency with a guessed radius. Frequency-only
endpoints (Hillenbrand, Peterson-Barney) are NOT compilable as-is; they must
be paired with real measured bandwidths from a bandwidth-bearing source
(ETL has them; Kent 2018 and Dunn 1961 carry B1–B4; the local
`Downloads\vowel_formant_review_fundamental_tables.xlsx` review). Klatt
default-BW (B1=90 B2=110 B3=170 B4=250) is the documented **fallback only** —
never presented as measured, always flagged in the table when used.

### Dataset admission rule (enforced)

**Only commit a dataset if it carries measured bandwidth** (any of the four
formant/mode rows has a real `bandwidth_hz`, or the source's own B table).
Frequency-only datasets are deleted, not kept as "incomplete". A table with
null bandwidth in every row has no place in the corpus — it cannot compile a
real body and it will silently waste a lane.

### Extracting bandwidth (the real route)

Bandwidth is MEASURABLE, and we own the tooling for it:
`scipy.signal.find_peaks` on the FFT, then read the −3 dB width off the same
curve — exactly what `tools/batch_compiler.py:table_from_wav` already does.
So the strongest path is: take the authoritative source **recordings**
(e.g. Iowa instrument WAVs, the 303 acid samples, our own captures) → FFT →
find_peaks → per-peak frequency AND measured −3 dB bandwidth from the same
curve. That yields a compilable endpoint with real BW, no inference. There
are 285 WAVs in the repo today. `scipy` is present; `scipy.io.wavfile` is
already the repo's loader (`pyruntime/arma_measure_lib.py:load_wav`).

Rule: prefer measured-BW-from-recording over consulted tables. A table only
enters the corpus if its source already carries bandwidth — otherwise the
gap belongs on the harvest list, not in the corpus.

**Bandwidth/f0/Q are `null` when the source doesn't carry them — never
invented or inferred.** Pairing another paper's numbers onto frequencies is
also barred (that is inference). If a dataset has no bandwidth source, it is
incomplete, not "done" — the gap goes on the harvest list. No CSV tier, no
XML, no markdown tables — JSON is the only ingest format.

## Workstation (the wish — "hyper-optimised")

The Workstation's whole job is picking endpoints and holding a blueprint. The
user should be able to, in a few clicks:

1. **Select two sources** from the academia tables — M0 (low morph) and M100
   (high morph). Each source = one `object` in an `acoustic-source-v1` JSON.
   X to Y. That's the entire input step; nothing else is measured.
2. **See the compiled lanes** immediately (the blueprint: 4 corners × 6 lanes
   of (f, r, zero) — pole book, not prose).
3. **Edit the Q corners in the GUI** (radius arm/defuse and lane revoice),
   since Q is authored, never measured. The blueprint is the hold point.
4. **Pulse**: "compile from here" → FFI encode → pack → unity-DC → certify.
   Failures are loud (`certify`/encoder refusal), never silent identity.

Keep it minimal: no graph gestures, no modes, no chrome. Two sources in, a
pole board, Q editing, one compile button. (Shipping-player UI contract in
AGENTS.md still applies: real parameter gesture or it doesn't ship.)

## Priorities (what matters most)

1. **Vowel filters / DeepBouche** — the category nobody else has; the flagship
   voice work. Most important.
2. **Authentic TB-303** — the one preset to get right. Source:
   `Downloads\musicradar-303-style-acid-samples.zip` (165 MB),
   `Downloads\tb.armadillo.json`, `Downloads\tb.body240`.
   Existing artifacts in repo: `bodies/tb303.body240`,
   `bodies/clean_tb303.body240`, `bodies/candidates/P2K_BassBox_303.body240`,
   `bodies/candidates/TRENCH_303.body240`.
   Recipe for the species is in `recipes/INTENT.md` (Tadpole — BassBox-303 +
   TB-OrNot-TB species: sub engine + harmonic ladder, KEY-tracked squelch).

## Next session (make it productive)

1. **Ship the source picker** in the Workstation off `recipes/tables/academia/`
   — single object selection per endpoint is the whole interaction.
2. **Author the authentic TB-303** from `Downloads\musicradar-303-style-acid-samples.zip`
   through this pipeline (extract → grammar → blueprint → compile), verify
   against `bodies/tb303.body240` + Tadpole recipe. Acid family conditioning
   (REZ) via `--anchors REZ`.
3. **Author a DeepBouche vowel** from two endpoints in the academia tables
   (VOW conditioning) as the flagship voice proof.
4. **Close the bandwidth gap (blocking).** A compilable endpoint needs
   measured bandwidths. Get them for the frequency-only sets (Hillenbrand,
   Peterson-Barney) from: Dunn 1961 (bandwidth/Q), Kent 2018, and the local
   `Downloads\vowel_formant_review_fundamental_tables.xlsx` (openpyxl works).
   Pairing rules in "Endpoint table format" above — never infer, never
   invent. PDFs need a human or table-extraction tool (no PDF here).
5. **Verify `tools/batch_compiler.py` end-to-end** on two real endpoints
   through the full FFI path (encode → pack → unity-DC → certify) and keep
   the evidence.
6. **Per-family Q-corner defaults** already land via `--anchors`/`--q-attitude`;
   decide whether the Workstation exposes them or keeps "flat" by default.
