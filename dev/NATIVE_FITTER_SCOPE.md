# Native fitter path — scope

2026-08-22. Scope only; nothing built. Answers "item 1": the offline /
in-app authoring path for the 8-corner × 7-section, 39,062.5 Hz container,
so `.4` (and cube) bodies can be fitted, not just loaded. Evidence is
`NATIVE_CONTAINER_SURVEY.md` (laws), `SIX_VS_SEVEN.md` §4 (six carries the
bank, combs excepted) and `p2k.hpp` (what the existing fitter is bound to).

## What exists

`trench::core::p2k` is the only fitter. It is bound, by constant, to the
P2K container and nothing else:

| bound to | where | native needs |
|---|---|---|
| `kSr = 44'100` | `p2k.hpp:17` | 39,062.5 |
| `kStageCount = 6` | `:18` | 7 |
| 272-rung lattice, `word_of`, `nearest_lattice`, `kLatticeLow` | `:27-48` | none — 11-bit fields |
| `kPoleRMax = 0.999786473`, `kPoleCeilingRsqWord` | `:23,25` | poles reach 1.0 (survey §3) |
| `kS6ZeroRsqWord` (S6 zero rule) | `:21` | row-7 **no zero** law |
| `Grid` at `kHiHz = 0.499·kSr`, 512 pts | `:57-81` | grid to 0.499·39,062.5 |
| `StageWords` = 4 words, scale separate | `:32` | same shape; scale word meaning open (§5) |
| `pack_body` → 240 bytes, 4 corners | `:117` | 560 bytes, 8 corners |

The response comparator (`cascade_response_db`, `response.cpp`) and the
container (`PackedBody`, `from_native_bytes`) are already lineage-correct.
`words_from_geometry` / `geometry_from_words` take a datum argument. So the
gap is the *fitter*, not the model or the file.

## Decision: sibling namespace, not parameters

Make `trench::core::native` a sibling of `p2k`, not a parameterisation of
it. Reasons, each a measured law rather than taste:

1. **Different quantiser.** P2K snaps magnitudes to a byte-indexed lattice
   and searches along it (`sweep_axis` walks lattice indices). Native words
   are 11-bit fields, `(field11 << 4) | 0xF`, range `[0x000F, 0x7FFF]`; the
   search space is a different grid with 2048 rungs per parameter. A fitter
   that walks "the lattice" has no native meaning.
2. **Different ceilings.** Native poles touch the circle and the firmware
   keeps them stable (`r' = r + (1−r)·r·q`). P2K refuses above 0.999786.
   Comb bodies live at r = 0.99999 — a P2K ceiling would refuse 9 of the 58
   `.4` bodies outright (§4).
3. **Different topology law.** P2K has the S6 zero-rsq rule; native has
   row 7 pole-only. Opposite constraints on the last row.
4. **Different grid and datum.** Nyquist 19,531 Hz; the ERB grid's upper
   edge and the root-placement ceiling (`0.4665·sr` in P2K) move with it.

Parameterising `p2k` over all four would leave one namespace whose every
constant is conditional. Two namespaces sharing `response.cpp`,
`packed_body.cpp` and the ERB weighting is the honest structure. Shared
pieces get lifted into `trench::core` (the grid builder and the
weighted-error scoring — the latter now exists as `measure::weighted_error`).

## The native fitter, piece by piece

Mirrors the P2K fitter's proven shape (seed → one-section-per-step
coordinate descent on packed words → scale pass → pack), with the laws
swapped.

**`native::Grid`** — ERB-weighted log grid, 20 Hz … 0.499 × 39,062.5, 512
points, with the same `z1/z2` tables and `factor_db` as P2K's. Lift the
builder to `core` and instantiate both from it.

**`native::words_from_root(hz, r, datum)`** — continuous root → the
nearest representable 11-bit word pair, searched on the *native* field
grid: candidate words are `(f << 4) | 0xF` for `f` in `[0, 2047]`; pick by
decoded value. No lattice.

**`native::is_legal(p, q, is_pole, row)`** — pole radius ≤ 1.0 (not
0.9998); zero radius ≤ 1.0; **row 7 zero must be degenerate** (`DFFF FFFF`
in the decoded corpus). Real-axis pairs allowed in both lanes (survey §4:
0.8% / 1.4% of the corpus, must not be flattened).

**`native::Corner`** — 7 × `StageWords`; `stage_db_into` / `total_into`
unchanged in form, evaluated at the native datum.

**Seeds** — the three P2K seed families carry over with one change:
`peel_seed` must never place a zero in row 7, and the continuous seed's
last stage is pole-only. Add a fourth seed, `comb_seed`: residual minima →
one notch per row, zero on the circle, pole just inside; §8 says this is
the flanger family's recipe and §4 says it is the only family that needs
all seven rows.

**Descent** — `sweep_axis` walks the 11-bit field index (±1, ±4, ±16, ±64
steps), not lattice rungs; `sweep_radius` likewise on the rsq field.
One section per accepted step, reported through the existing `StepFn`.

**Scale pass** — open question §5 of the survey: the runtime has no scale
word, only a per-stage DC-normalise bit. Until that is reconciled, the
native scale pass writes the word the *corpus* writes (`0xDFED` in 42–44%
of rows, range −21 … +12 dB) and is marked non-device-authoritative.

**`native::pack_body`** — 8 × 7 × 5 words → 560 bytes via
`PackedBody::native_bytes()`; already exists.

## Acceptance, before it is called done

Per `native/CLAUDE.md` Verification:

- **Corpus round trip at the native datum**: every one of the 289 bodies,
  every corner, decoded → re-encoded through `native::words_from_root` →
  bit-identical words. This is the test that proves the 11-bit law is the
  body law (survey §10 open item 1). If it fails, the `.body` files are a
  converted form and the export path must not claim device compatibility.
- **Row-7 law**: the fitter cannot emit a row-7 zero; a test feeds a comb
  target and asserts row 7 decodes degenerate-zero on every corner.
- **Ceiling**: a target needing r = 0.99999 (Flange3.4 corner 4) fits to
  < 0.3 dB packed; the P2K fitter refuses it. Both asserted.
- **Six-vs-seven parity**: the 33 `.4` bodies that fit 0.00 in six
  (`six_section_results.json`) fit 0.00 in seven from their own seed.
- **Real-axis pairs** round-trip (conjugate, real, degenerate — all three).
- **Corner correspondence**: fitting corners 1–7 seeded from corner 0 keeps
  row roles (no row changes kind across a corner edge unless the target
  demands it); measured on `Ear Bender`-style parked rows.

## Order of work

1. Lift grid builder + weighted error into `core` (small; `measure` already
   holds the scoring).
2. `native` namespace: constants, word law, legality, `Corner`, `Grid`.
   Corpus round-trip test first — it can fail and that is a finding.
3. Seeds + descent + scale pass; the six tests above.
4. App: document owns a `PackedBody` + datum; plot/tokens iterate the body's
   counts; FIT dispatches to `p2k` or `native` by container. Save writes the
   container the body came in.

Step 2's round-trip test is the gate. Everything after it is mechanics.

## Not in scope

- Device-bit-exact audition (firmware cos/sin polynomials, output
  quantiser) — survey §7, separate.
- The DC-normalise bit ↔ scale word question — must be settled for export,
  not for fitting.
- Morph movement law — `SIX_VS_SEVEN.md` §11, the P2K OS decompile.
