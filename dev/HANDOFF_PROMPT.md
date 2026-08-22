# Handoff

Two things in this file. **Paste only Part 1.** Part 2 is notes for Tyson and
would confuse a fresh session — it discusses a draft they never saw.

---

# PART 1 — paste this

Read `dev/SESSION_2026-08-22.md` first; it indexes the last session and names
every open item.

Context: TRENCH builds a filter plug-in that interoperates with E-mu's Z-plane
filter format, on hardware and software Tyson owns. The docs cite device
addresses because those are the evidence for the format rules.

Last session settled the container laws for the 7-section lineage: **seven poles
and six zeros** (section 7 can never hold a zero — 100% of the corpus, and the
device's audio loop runs its zero recursion exactly six times before a
zero-less seventh block), **no P2K lattice in this lineage** (11-bit fields, not
the 272-rung grid), and the **datum is 39,062.5 Hz**, decoded from the clock
tree.

## Your task: give root pairs a real-axis representation

`StageRoots` cannot express real-axis root pairs. 46 of the 132 P2K corners
carry them across 53 stages, so the continuous solver cannot reach those
corners at all. The native corpus has them too — survey §4 measures 0.8% of
poles and 1.4% of zeros as real-axis.

**Work in the C++ core, `native/trench-core`.** That is what the Qt app links as
`trench_native_core` and it is the canonical fitter. Do not edit
`trench-native/trench-core/src/arma_endpoint.rs` — that Rust tree is the retired
egui lineage and the Qt app's CMake does not reference it. The same filename
also exists in trench-x3-clean and trench-authoring; neither is this task.

The C++ core **already has the right shape** and the fitter simply does not use
it. `native/trench-core/include/trench/core/packed_body.hpp`:

```cpp
using RootPair = std::variant<ConjugatePair, RealPair, DegeneratePair>;
struct SectionGeometry { RootPair pole; RootPair zero; double scale; };
```

`geometry_from_words` already returns real-axis pairs correctly — it takes the
`disc = p*p - 4*q >= 0` branch and returns `RealPair`. The gap is downstream:
the fitter's continuous stage works in `(hz, radius)` coordinates that cannot
represent a real pair, so those corners are unreachable from the solver even
though the container and the decoder both handle them.

### Do this in order

1. **Write the failing test first.** `native/CLAUDE.md` requires it before any
   packed-law change. Find a factory corner known to contain a real-axis pair,
   assert it round-trips through `words_from_geometry` / `geometry_from_words`
   bit-exactly, and assert the fitter can reach it. Expect the last part to
   fail — that failure is the whole point.

2. **Extend the solver's coordinate mapping,** not the geometry types. The types
   are already correct. What needs changing is how the continuous fitter
   parameterises a section so a real pair is in its reachable set.

3. **Do not add a `radius < 1.0` stability check.** Native pole radius reaches
   **exactly** 1.000000000 (survey §3). A strict `< 1.0` test rejects legal
   geometry, and that bug already exists downstream —
   `dev/PLUGIN_STALENESS_AUDIT.md` item 2 records `ffi.rs:253-278` refusing
   those bodies at load with `-5` via exactly that comparison. The ceiling is
   lineage-dependent: 0.999786473 for P2K, 1.0 inclusive for native.

4. **Keep `DegeneratePair` first-class.** 3.4% of poles and **33.3%** of zeros
   in the corpus are degenerate; a third of all sections are pure resonators and
   section 7 always is. Any representation that cannot say "no zero here" is
   wrong for this container.

5. **Run the acceptance tests.** The parity fixture and the 132-corner bank
   refit are load-bearing. If either moves, the change is wrong.

Two rules that bit last session: prove it at the instrument rather than on
paper, and record negative results — a plausible-sounding improvement
(matching-pursuit seeding) turned out three times worse than what it replaced,
and that is written down so it is not proposed again.

---
---

# PART 2 — notes for Tyson, do not paste

An earlier draft of this handoff came from another model. Its **constants were
all correct**: `3FC90FDA` = π/2, `3EC90FD8` = π/8, `FFFF8000` = −32768,
`32C90FDB` = π/2²⁷, `32800800` = 2⁻²⁶(1+2⁻¹²), `447A0000` = 1000.0, and the
piecewise trig branch is at π/8. Three other things were wrong.

**1. Wrong file.** It named `trench-native/src/arma_endpoint.rs`, which does not
exist. The real path is `trench-native/trench-core/src/arma_endpoint.rs` — but
that tree is the retired egui workspace (`trench-core, author, author-server,
trench-app`) and the Qt app does not link it. Following the instruction would
have edited dead code.

**2. The DC-normalise flag is not "stages 0 through 5, unconditional".**

```c
*puVar1 = *puVar1 | 0x80000000;              // stage 0, always
if ((*param_1 & 1) != 0) {                   // conditional on a payload bit
    for (cVar8 = 0; cVar8 < 6; cVar8++) {
        puVar1 = puVar1 + 0x10;              // stride = one stage
        *puVar1 = *puVar1 | 0x80000000;      // stages 1..6
    }
}
```

Seven stages, and six of them conditional.

**3. The resonance law was inverted.** The draft gave
`y' = (1-r) + r(1-r)*s`. The code is:

```c
fVar35 = (fVar24 - fVar29) / fVar24;    // s = (|w1| - thr) / |w1|
if (fVar24 < fVar29) fVar35 = 0.0;
fVar31 = 1.0 - fVar30 * 32800800h;      // r
fVar30 = (1.0 - fVar31) * fVar31;       // (1-r)*r
fVar35 = fVar30 * fVar35 + fVar31;      // r' = r + (1-r)*r*s
```

So `r' = r + (1−r)·r·s`. At `s = 0` that is `r' = r`, no modulation below
threshold; the draft's version gives `r' = 1−r`, a different filter. There is
also no leaky integrator — the level input is `ABS(pfVar13[9])`, the stage's
stored `w1`, read instantaneously — and the threshold is the **level
parameter**, not the pole magnitude.

**4. The proposed enum dropped fields.** It replaced a struct carrying a pole
pair, a zero pair and scale with a two-variant enum holding one pair, losing the
zero and the scale, and had no `Degenerate` variant for the container's most
common zero state.

**If you want the other two open items instead**, swap the task section for one
of these and the rest of Part 1 still applies:

- **Morph parity** (`dev/MORPH.md` §1) — the plug-in and EmulatorX3 diverge
  12–25 dB in the interior on the same preset while corners stay bit-identical,
  and the interpolation domain explains exactly that. This one needs your
  decision first — match the hardware, or match EmulatorX3 — so it is not a
  directive task until you choose.
- **Row permutation** (`dev/MORPH.md` §2) — free at the corners, should improve
  our own fitted bodies. Measure before building.
