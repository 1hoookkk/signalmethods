# Open Questions & Contradictions

Every unproven assumption, missing file, and contradiction in the current
X3 import pipeline. Organised by severity: **blocking** (cannot proceed
without resolution), **material** (affects correctness), **clarification**
(affects completeness).

---

## Blocking

### Q1: The extraction script is missing

`tools/extract_x3_menu_filters.py` is referenced by both
`ref/x3_menu/README.md` ("Generate this directory with:") and the manifest
JSON, but **does not exist in this repository**.

- The runtime_blocks were produced by this script
- The script's logic (ROM table indexing, writer replication, corner
  layout) is the single source of truth for how the runtime blocks were
  generated
- Without it, we cannot:
  - Regenerate runtime_blocks from a different DLL version
  - Verify the extraction was correct
  - Add new sample rates
  - Extract the generated-class corner banks

**Likely location**: `df2-workstation` repo (external, at
`C:\Users\hooki\df2-workstation`). The `x3_fundamentals_to_bodies.py`
script imports from `df2-workstation/pyruntime/packed_interp.py`.

**Action**: Locate or rewrite the extraction script. The rewrite is bounded:
read known DLL addresses, slice per-rate segments, replicate to output
stages, write as little-endian u16.

---

### Q2: No hardware null exists for runtime blocks

The `x3_fundamentals_to_bodies.py` script verifies **codec equivalence**
(df2 decode ≈ TRENCH decode) but does NOT verify against the real X3.

- The "ground truth" tests in `minifloat.rs` are `#[ignore]`d
- The capture rig's X3 recordings exist but went through ARMA fitting
  (measurement path), not byte-level comparison
- NEXT_TASK.md line 83: "the RE codex is contaminated (Tyson) — treat as
  hypotheses"

**What would prove it**: A pink-noise capture of the X3 playing a known
fixed-class filter at a known corner, nulled against TRENCH rendering the
same corner from the runtime block bytes. The TB or Not TB null
(NEXT_TASK.md line 55) proves this is possible for P2K bytes — the same
protocol works for X3 runtime blocks.

---

### Q3: Generated classes have no runtime blocks

Dual EQ Morph, Dual EQ + LP Morph, Dual EQ Morph/Expression, and
Peak/Shelf Morph have NO runtime blocks in `ref/x3_menu/runtime_blocks/`.

- Their support tables exist (morph_shared_base, dual_eq_lp_profiles,
  etc.) but the writer functions haven't been reimplemented
- The helper function `FUN_1802c59b0` is documented in Ghidra extracts
  but not ported to Python or Rust
- These classes have **dead Q by construction** — the writer duplicates
  endpoints across the Q axis
- The Morph Designer grammar (types 1–3) IS fully documented, but the
  generated-class writers are separate code paths

**Action**: Reimplement the four generated-class writers from Ghidra
disassembly. This requires reading `FUN_1802c59b0` (shared helper),
`FUN_1802c5d60`, `FUN_1802c5e40`, `FUN_1802c5f10`, and `FUN_1802c6020`.

---

## Material

### Q4: "Fixed-point" label in README is wrong — what else is?

`ref/x3_menu/README.md` states: "The runtime blocks are raw fixed-point
coefficient words."

Multiple independent proofs show they are minifloat-encoded (see
`X3_BINARY_CONTRACT.md` §2.1). The README was written before the Ghidra
decode analysis confirmed the codec.

**Implication**: The README's "raw fixed-point" claim may have influenced
the old batman study's Q14/Q15 decode attempts, which failed for vocals
(NEXT_TASK.md line 82: "vocals fail all readings"). The Q14/Q15
hypothesis was chasing a phantom.

**Resolution**: Update the README. The runtime blocks are minifloat u16,
same codec as body240. The "raw" descriptor is correct in spirit
(verbatim DLL bytes, no transformation) but "fixed-point" is wrong.

---

### Q5: The 48000, 96000, and 192000 blocks are unused

`x3_fundamentals_to_bodies.py` only processes the 44100 Hz runtime blocks.
The other three sample-rate families exist on disk but have never been
packed into body240 files.

- TRENCH's engine runs at the host sample rate (CLAUDE.md)
- A body authored at 44100 Hz will have wrong pole/zero frequencies
  when replayed at a different rate (unless Hz-anchored via datum_rate)
- The X3 ships four separate coefficient banks per filter — one per
  sample-rate family — suggesting the coefficients are NOT rate-independent

**Question**: Does the X3 do sample-rate migration (recomputing
coefficients for the host rate) or does it select the nearest bank?
The Ghidra extracts show the sample-rate family index is read from
`param_1 + 0x0c`, used to index into rate-strided tables. This suggests
bank selection, not migration.

**Implication**: A correct importer needs all four rate families.
At minimum, the body must be Hz-anchored at the authoring rate so
TRENCH's datum system can remap it.

---

### Q6: Codec equivalence is df2-reference, not hardware-proven

The codec null in `x3_fundamentals_to_bodies.py` compares:
- TRENCH's `probe()` (which uses `PackedCorners::interpolate_biquad`)
- df2's `kernel_to_biquad(words_to_coeffs(...))`

This proves df2 and TRENCH use the same minifloat decode. It does NOT
prove that either matches the X3 hardware, because:
- The df2 `packed_interp` module was written by reading `FUN_1802c3600`
  from the DLL — it's a reimplementation, not a truth source
- No byte-level null against actual X3 audio output has been performed
  for the runtime blocks

**Mitigation**: The TB or Not TB P2K null (proven: NEXT_TASK.md line 55)
shows the decode chain works for P2K words. Since the codec is the same,
the X3 runtime blocks should decode correctly too. But "should" is not
"proven."

---

### Q7: Writer replication law is observed, not sourced

The observation that LP/HP/BP writers duplicate a single prototype stage
for multi-pole filters comes from byte comparison of runtime blocks, not
from reading the writer functions in Ghidra.

- For LP/HP/BP: confirmed by byte comparison (identical rows)
- For Phaser 1/2, Bat Phaser: confirmed by byte comparison
- For Flanger Lite, Vocals: confirmed (genuinely distinct stages)

**Question**: Does the 4-pole writer really copy the same row, or does it
apply a small detuning that happens to produce the same bytes at these
specific corner positions? The Ghidra disassembly of e.g. `FUN_1802c4c40`
(4-pole LP) would settle this.

**Risk**: Low. The bytes are identical. Even if the writer has detuning
logic, it produces zero detuning at the corner positions where we sample.
The runtime blocks are the authoritative output regardless.

---

### Q8: Per-coefficient ramping

TRENCH applies per-sample linear coefficient ramping into the cascade
(CLAUDE.md line 13: "per-sample linear coefficient ramping"). The X3
Ghidra extracts document interpolation and decode but NOT the per-sample
update mechanism (if any).

- If the X3 ramps coefficients, the ramp law matters for nulling
- If the X3 updates instantaneously (no ramp), TRENCH's ramp would
  cause a measurable difference in dynamic sweeps

**Evidence gap**: No Ghidra extract documents the X3's coefficient
update mechanism. The TB or Not TB null (corner positions only) doesn't
test this — corners are static.

---

## Clarification

### Q9: What is the Q-axis behaviour of the ROM table classes?

For fixed-class filters, the writer receives both MORPH and Q as inputs.
But we only have the four corner snapshots.

- Does Q control resonance (pole radius) in the table lookup?
- Does Q select between different prototype rows in the ROM table?
- Or is Q an additive offset applied after the table lookup?

The manifest's corner order (M0Q0, M100Q0, M0Q100, M100Q100) confirms
that Q0 and Q100 produce different words (otherwise corners would be
identical). Looking at the LP table: corner 0 (M0Q0) and corner 2
(M0Q100) have different words — Q does change the coefficients.

**Answer from the Ghidra extract**: The writer functions read both
controls and index into the ROM table accordingly. The ROM table has
4 rate-strided segments, each containing 4 corners. The mapping from
(MORPH, Q) to table position is class-specific.

---

### Q10: Does the X3 morphing use the same corner interpolation as P2K?

`FUN_1802c3d40` bilinearly interpolates packed u16 words. The Ghidra
extract confirms truncation between legs. TRENCH's `lerp_u16` also
truncates (`diff as i32 as i16`).

**Open question**: Is the truncation order (morph-first → q-second)
always the same? The Ghidra extract says "each intermediate leg
truncates before the next interpolation leg" but doesn't specify
which axis is interpolated first.

TRENCH interpolates morph-first, then q:
```rust
let edge0 = lerp_u16(a[wi], b[wi], morph);   // M0→M100 at Q0
let edge1 = lerp_u16(c[wi], d[wi], morph);   // M0→M100 at Q100
result[si][wi] = lerp_u16(edge0, edge1, q);  // Q0→Q100
```

**Risk**: If the X3 interpolates q-first, mid-point results differ by
the truncation error (< 1 LSB). Inaudible but prevents bit-exact nulling.

---

### Q11: The Morph Designer pad row

When fewer than 6 rows compile, Morph Designer pads with:
`[0xdfff, 0xffff, 0xdfff, 0xffff, 0xe000]`

This is identical to TRENCH's identity row (encoded from
`(1000, 0, 1000, 0, 1.0)`). Both decode to the unity biquad
`[1, 0, 0, 0, 0]`.

**Question**: Is this pad row universal across ALL X3 filter classes,
or does each class have its own bypass sentinel? The Ghidra extract
shows it's at `DAT_1806d7500` which is `morph_designer_bypass.raw` —
suggesting it's Morph Designer-specific. But the runtime blocks for
fixed-class filters don't include padding (they produce exactly N
stages). The padding is added by the body240 packer.

**Risk**: Low. Any row that decodes to unity is electrically identical
in the cascade.

---

### Q12: Should existing X3 bodies be discarded?

**X3F_* bodies** (from `x3_fundamentals_to_bodies.py`):
- **KEEP** as 44100 Hz reference
- They are verbatim minifloat words, certified, codec-null passed
- But they are INCOMPLETE — only one sample rate, no rate-family variants
- The import workflow should supersede them with properly anchored bodies
  covering all four rate families

**x3_shape_* bodies** (Morph Designer presets):
- **KEEP** — separate pipeline, not in scope for this task
- They are the current shipping Morph Designer preset set

**X3_* bodies** (capture rig, in `bodies/candidates/`):
- **KEEP as measurement artifacts** but clearly separate from runtime
  preset imports
- They went through audio capture + ARMA fitting — the exact path the
  mission forbids for runtime presets
- Several are known bad (dead-Q capture trap, NEXT_TASK.md line 32)
- Do not confuse with byte-extracted runtime preset bodies

**Old batman study artifacts** (`ref/batman/`):
- **DEPRECATED** — incorrect class grouping, incorrect encoding guess
  (Q14/Q15)
- The README explicitly says "Do not regenerate or use"
- Keep for provenance but never use as reference

---

## Summary of contradictions in the existing importer

| # | Claim | Source | Contradiction | Evidence |
|---|---|---|---|---|
| 1 | "raw fixed-point coefficient words" | `ref/x3_menu/README.md` | Words are minifloat, not fixed-point | Ghidra `FUN_1802c3600` matches TRENCH `decode()`; codec equivalence test passes |
| 2 | Batman loaders grouped as one class | `ref/batman/` | Four separate classes per RTTI/vtables | `ref/batman/README.md` correction + manifest vtables |
| 3 | Runtime blocks = "raw fixed-point" → "not P2K minifloat" | `ref/x3_menu/README.md` | Same codec, same decode path | Byte-comparison shows identical encoding; `kernel_to_biquad` produces valid responses |
| 4 | df2 codec = truth | `x3_fundamentals_to_bodies.py` | df2 codec is a reimplementation of the DLL, not hardware-proven | No X3 hardware null exists for runtime blocks |
| 5 | 44100 Hz is sufficient | `x3_fundamentals_to_bodies.py` (only reads 44100 blocks) | X3 has four rate families; coefficients differ per rate | Runtime blocks exist for 48k/96k/192k; rom_tables are rate-strided |
| 6 | capture rig = runtime preset import | `capture_x3.py` + `x3_typewalker.py` | Capture measures audio and fits — measurement path, not byte path | The mission forbids measurement-derived runtime presets |
