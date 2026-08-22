# Shipping plugin vs. this session's findings

Read-only audit of `C:\Users\hooki\trench-x3-clean` (branch `feat/frontend`,
HEAD `4e56239dd`) against the container laws established 2026-08-22. Nothing was
edited in that repository.

## Headline

**Nothing is broken today, and every defect below fires the moment a cube load
path is added.**

The JUCE C++ layer has no 560-byte path at all — `PluginProcessor.cpp` gates
every body operation on `len != 240` (lines 485, 523, 550, 587, 599, 601, 622),
and `PluginEditor.cpp:124` says so outright: "there is no cube load path yet, so
it is not built yet." The Rust core *does* accept, author and play 560-byte
bodies (`minifloat.rs:96` `BODY_BYTES = 560`, `cascade.rs:1` `NUM_STAGES = 7`,
`bin/cube.rs`, `arma_endpoint.rs`). So all the staleness sits in trench-core, on
paths the shipping plugin does not currently call.

## Ranked, worst first

**1. Crash.** `trench_body_compile_at` (`ffi.rs:283-317`) accepts a 560-byte
body via `is_body_len`, then writes the result through `packed.to_rom_bytes()`,
which is documented to panic on a native body (`minifloat.rs:241-246`).

**2. Legal native bodies are rejected at load.** The gate at `ffi.rs:253-278`
runs `if r >= rmax` with `r_max = 1.0`, called from every
`trench_engine_load_body_bytes_at` at `:142`. Survey §3 measured native poles
reaching exactly 1.000000000. Those bodies return −5.

**3. Wrong datum on every native body.** One datum constant exists:
`compiler.rs:5  pub const DEFAULT_AUTHORING_SR: f64 = 44_100.0`. `ffi.rs:102`
passes it for any length `is_body_len` accepts, so a 560-byte body compiles at
44,100 instead of 39,062.5 — every root shifted 1.2288×, about +3.57 semitones,
while looking entirely plausible. `39062.5` appears once in the whole tree, in a
comment at `bin/make_body.rs:39`.

**4. Section-7 zeros written by three authoring paths.**
`arma_endpoint.rs:307-353` and `:450-500` loop `0..NUM_STAGES` with a free
`zero_hz`/`zero_r` each; `bin/cube.rs:80-135` *requires* seven sections and
gives every one a zero. Survey §1 says section 7 can never hold one.

**5. One radius ceiling for both lineages.** `stage_law.rs:97`
`max_contiguous_pole_radius()` = 0.9999847410945204, derived from encoder
representability and applied to all authored roots regardless of container. The
P2K figure 0.999786473 appears nowhere.

**6. Third axis unprobeable.** `ffi.rs:841` `trench_packed_probe_at` hardcodes
`z = 0.0`, so it can never probe the far plane.

## What is already right

**No lattice anywhere.** Searched `lattice`, `rung`, `272`, `admissib`,
`quantize`, and the `(byte<<8)|low[byte>>4]` shape across `trench-core/` and
`plugin/`: no table, no admissibility check, no 11-bit expansion. Nothing snaps
native geometry to the P2K grid — consistent with survey §2, by omission rather
than by design, but correct.

**Packed-domain interpolation, confirmed.** `minifloat.rs:282-305`
`interpolate_words` lerps `u16` words and decodes downstream, pinned by the test
`interpolate_words_is_the_exact_source_of_decoded_interpolation`. Corner index is
documented at `:269` as `m | q<<1 | z<<2`, matching the firmware's
`bit0 | bit1<<1 | bit2<<2`, and `bin/cube.rs:72-75` names the eight frames in
that order.

The device uses Q15 weights with `SMUAD` and a `>>15` renormalise, while
`lerp_u16` (`minifloat.rs:56`) uses an f32 weight and an i16-truncated delta.
Same domain, same law. Measured difference: within 1 LSB — see
`dev/MORPH.md` §1. Not a defect.

**The 240→560 compat path is correct.** `IDENTITY_STAGE` is exactly
`[0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF]`; `from_legacy_corner_data`,
`from_legacy_words` and `from_rom_bytes` never touch `si == 6`; and
`is_legacy_representable` checks section 6 is still identity before allowing a
240-byte export.

## Section form and output stage

`cascade.rs:139-155` is transposed Direct Form II in f64, not the device's
coupled-form with a `cot(θ)` tap. Those are algebraically identical to 8e-15, so
this is a difference, not a defect.

The device's output stage is **not** modelled: no 1.5-LSB round-toward-zero
quantiser, no parabolic soft clip at ±2³¹. The plugin has a different
nonlinearity — a tanh at `engine.rs:276-283` with knee +12 dBFS and asymptote
+18.1 dBFS — and `TrenchDspBridge.h:264` sets `saturate = false` by default. Its
cos/sin are libm, not the firmware's ~0.2%-accurate polynomials.

## Stale 6-vs-7 assumptions

- `compiler.rs:16` `STAGES = 6` — the legacy typed-card compiler, and the only
  compile path the plugin actually drives.
- `cartridge.rs:184` rejects JSON cartridges unless `packedWords` has exactly 6
  rows.
- `ffi.rs:386-402` declares `out_roots30` / `out_words30` while `arma_endpoint`
  writes 7 rows. `TrenchDspBridge.h:68-81` already records this as a known
  authoring-path defect.
- `runtime_preset.rs:179` — test named `..._pads_to_seven_stages` asserts
  against index 5.
- Stale comments at `cascade.rs:295` and `engine.rs:572` say "six stages" over a
  loop of seven.
- `tests/body_bytes_canonical.rs:31` sizes a 240-byte buffer with
  `Vec::with_capacity(BODY_BYTES)` where `BODY_BYTES` is now 560. Capacity only,
  harmless.

A previously-fixed instance is documented in place at
`TrenchDspBridge.h:105-124`: buffers left at 6×5=30 after `NUM_STAGES` went to 7
overflowed a stack array by 5 doubles and fail-fasted the host with
`0xC0000409`.

## Working tree

Not clean: two tracked modifications, neither touching any of this — a JUCE pin
bump 8.0.13 → 8.0.15, and documentation in `ref/ghidra_extracts/runtime_hacks.md`
adding RTTI class names and a `rad = (v >> 1) + 0x6400` derivation. That document
independently notes "the fifth word is written as the constant `0xdfff`",
consistent with the identity sentinel.

**No uncommitted change touches any datum constant, radius ceiling, section-7
handling, interpolation path, or DSP section form.**
