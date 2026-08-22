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

## Your task: dig into the `.4` presets, six sections versus seven

Read `dev/SIX_VS_SEVEN.md` first. It is a first pass that frames the question
and deliberately does not answer it.

Morpheus (1993) and UltraProteus (1994) are 7 sections, 14th order. Proteus 2000
(~1999) is 6 sections, 12th order. The **later** machine has fewer. Nothing we
hold says why.

Three measurements are already in hand. All 33 P2K factory bodies use all six
sections, no exceptions and no slack. Morpheus uses all seven in 247 of 289. And
P2K's per-section contribution runs systematically larger than Morpheus's, its
last section reaching 94.7 dB median span against Morpheus's 51-66. Fewer
sections, each working harder.

A response-similarity sweep found no evidence that P2K bodies are ported-down
Morpheus bodies: median cosine similarity 0.909, and several distinct P2K bodies
map onto the same Morpheus body, which a re-authoring relationship would not
produce. Treat P2K as independently authored unless you find better evidence.

**This has product consequences, not just archaeological ones: v1 ships `.4`
presets.**

### Questions, in priority order

1. **Is a `.4` filter expressible in six sections?** They use all seven in 47 of
   58 cases. If seven is genuinely required then no six-section path can carry
   the v1 bank, and the 560-byte path becomes mandatory rather than preferred.
   Test it directly: take a `.4` body, fit its corner responses with a
   six-section cascade, report the error. Under about 1 dB means the seventh
   section is convenience; well above means it is structural.

2. **Does P2K's sixth section do the work of Morpheus's sixth and seventh?**
   Take a close pair from `SIX_VS_SEVEN.md` section 3 - `millennium` against
   `4PoleMidQ.4` at 0.990, or `cruz_pusher` against `2p>4p 0` at 0.993 - and
   compare them section by section, each at its own datum. Look for two adjacent
   Morpheus sections corresponding to one P2K section.

3. **Does any manual state the order?** The Proteus 2000 operation manual has
   not been read. The E-Loader manual was checked and says nothing about filter
   data; the Audity OS images were checked and contain no filter bodies.

### Rules that bind here

- **Never pool the datums.** P2K is 44,100 Hz, Morpheus is 39,062.5 Hz. Decode
  each at its own rate and compare in real Hz afterwards. Decoding one at the
  other's rate shifts every root by 1.2288x, about +3.57 semitones, and looks
  entirely plausible while being wrong everywhere.
- **Section 7 can never hold a zero** (`NATIVE_CONTAINER_SURVEY.md` section 1).
  Respect that in the seven-section original when comparing.
- **A negative result is a result.** The similarity sweep above is one. Record
  yours the same way.

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
