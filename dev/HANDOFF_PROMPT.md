# Transitional prompt — hand this to the next session

Paste the block below. Everything it asserts is verified in this repo; the
corrections section exists because an earlier draft of this handoff contained
three errors that would have sent you to the wrong file.

---

## The prompt

> Read `dev/SESSION_2026-08-22.md` first — it indexes everything from the last
> session and names the open items in priority order.
>
> Context: TRENCH builds a filter plug-in that interoperates with E-mu's Z-plane
> filter format, on hardware and software Tyson owns. The docs cite device
> addresses because those are the evidence for the format rules.
>
> Last session established the container laws for the 7-section lineage and read
> the device's own audio path directly. Three results are load-bearing:
> **seven poles and six zeros** (section 7 can never hold a zero, confirmed in
> both the corpus and the control flow), **no P2K lattice in this lineage**
> (11-bit fields, not the 272-rung grid), and the **datum is 39,062.5 Hz**
> decoded from the clock tree.
>
> Your first task is one of these, Tyson's call:
>
> 1. **Real-axis root pairs.** `StageRoots` in `trench-core/src/stage_law.rs`
>    can only express conjugate pairs. 46 of the 132 P2K corners carry real-axis
>    pairs across 53 stages, and the current type cannot represent them, so the
>    continuous solver cannot reach those corners. Survey §4 measures the same
>    thing in the native corpus: 0.8% of poles and 1.4% of zeros are real-axis.
>    Read the "Real-axis work" section below before starting — the obvious
>    refactor has a trap in it.
>
> 2. **Morph parity target.** `dev/MORPH.md` §1. The plug-in and EmulatorX3
>    diverge by 12–25 dB in the interior on the same preset while corners stay
>    bit-identical, and the interpolation *domain* explains exactly that. Decide
>    whether we match the hardware (already correct) or EmulatorX3 (unmeasured).
>    Do not change the interpolation until this is decided.
>
> 3. **Row-permutation experiment.** `dev/MORPH.md` §2. Free at the corners,
>    should improve our own fitted bodies' interiors. Measure before building.
>
> Standing rules that bit last session: prove claims at the instrument, not on
> paper; a negative result is worth recording; and do not let a hypothesis
> become a project rule.

---

## Corrections to an earlier draft of this handoff

An earlier version of this prompt carried a `ROSSUM_DSP_MECHANICS.md` and a
Rust refactor. The constants in it were all correct — `3FC90FDA` = π/2,
`3EC90FD8` = π/8, `FFFF8000` = −32768, `32C90FDB` = π/2²⁷, `32800800` =
2⁻²⁶(1+2⁻¹²), `447A0000` = 1000.0, and the piecewise trig branch really is at
π/8. Three other things were wrong.

**1. The file path.** It named `trench-native/src/arma_endpoint.rs`. There is no
`trench-native/src/`. The file is `trench-native/trench-core/src/arma_endpoint.rs`.

More importantly, that Rust tree is the **retired egui lineage** — the workspace
at `trench-native/Cargo.toml` lists `trench-core, author, author-server,
trench-app`, and the Qt app's CMake does not reference any of it. The canonical
fitter core is the C++ `native/trench-core` (`trench::core::p2k`), which the app
links as `trench_native_core`. Work aimed at the shipping fitter belongs there.
The Rust `arma_endpoint.rs` also exists in trench-x3-clean and trench-authoring;
decide which one you actually mean before editing.

**2. The DC-normalise flag is not "stages 0 through 5".** From the unpacker tail:

```c
*puVar1 = *puVar1 | 0x80000000;              // stage 0, unconditional
if ((*param_1 & 1) != 0) {                   // conditional on a payload bit
    for (cVar8 = 0; cVar8 < 6; cVar8++) {
        puVar1 = puVar1 + 0x10;              // stride = one stage
        *puVar1 = *puVar1 | 0x80000000;      // stages 1..6
    }
}
```

Seven stages, not six. Stage 0 always; stages 1–6 only when a bit in the payload
is set. Describing it as unconditional across stages 0–5 is wrong twice.

**3. The resonance law is not a leaky integrator, and the formula is inverted.**
The draft gave `y' = (1-r) + r(1-r)*s`. The code is:

```c
fVar35 = (fVar24 - fVar29) / fVar24;    // s = (|w1| - thr) / |w1|
if (fVar24 < fVar29) fVar35 = 0.0;      // s = 0 below threshold
fVar31 = 1.0 - fVar30 * 32800800h;      // r, decoded radius
fVar30 = (1.0 - fVar31) * fVar31;       // (1-r)*r
fVar35 = fVar30 * fVar35 + fVar31;      // r' = r + (1-r)*r*s
```

So **`r' = r + (1−r)·r·s`**. At `s = 0` this gives `r' = r` — no modulation
below threshold. The draft's version gives `r' = 1−r` there, which is a
different filter entirely.

The level input is `ABS(pfVar13[9])`, the stage's stored `w1` state read
instantaneously — there is no integrator and no envelope follower. And the
threshold is the **level parameter** (`piVar16[7]` / `[8]`, the same value
scaled by 1000.0 for the state clamp), not the pole magnitude.

---

## Real-axis work — read before refactoring

The intent is right and the gap is real. Two traps in the obvious refactor.

**Trap 1: `StageRoots` carries a pole pair *and* a zero pair *and* scale.**
Current shape, `trench-core/src/stage_law.rs:36`:

```rust
pub struct StageRoots {
    pub pole_hz: f64,
    pub pole_r: f64,
    pub zero_hz: f64,
    pub zero_r: f64,
    pub scale: f64,
}
```

Replacing this wholesale with a two-variant enum holding a single root pair
drops the zero and the scale. What is needed is a root-pair *kind* used **twice**
inside the struct, not a replacement for it:

```rust
pub enum RootPair {
    Conjugate { hz: f64, r: f64 },
    Real { r1: f64, r2: f64 },
    Degenerate,
}

pub struct StageRoots {
    pub pole: RootPair,
    pub zero: RootPair,
    pub scale: f64,
}
```

`Degenerate` is not optional. Survey §4 measures 3.4% of poles and **33.3%** of
zeros as degenerate — a third of all sections are pure resonators, and section 7
is *always* one. A two-variant enum cannot express the container's most common
zero state.

The C++ core already models this correctly and can be copied from:
`native/trench-core/include/trench/core/packed_body.hpp` has
`using RootPair = std::variant<ConjugatePair, RealPair, DegeneratePair>;` with
`SectionGeometry { RootPair pole; RootPair zero; double scale; }`.

**Trap 2: the stability check must not be `r < 1.0`.** Survey §3 measured native
pole radius reaching **exactly 1.000000000**. A `< 1.0` test rejects legal native
geometry — and that is not hypothetical: `dev/PLUGIN_STALENESS_AUDIT.md` item 2
records that `ffi.rs:253-278` already refuses those bodies at load with `-5`,
via exactly this comparison. Re-introducing it in `is_stable` would entrench the
bug.

The ceiling is lineage-dependent: 0.999786473 for P2K, 1.0 inclusive for native.
It is not one constant, which is audit item 5.

**Do this before the refactor:** add a failing test that constructs a real-axis
pair from a factory corner known to contain one and asserts round-trip through
`words_from_roots_at` / `geometry_from_words`. `native/CLAUDE.md` requires a
failing test before changing a packed law, and the round-trip is the acceptance
evidence that matters here.
