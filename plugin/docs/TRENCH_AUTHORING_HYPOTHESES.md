# TRENCH Authoring Hypotheses

Interpretive ideas, proposed methods, and creative workflow — separated
from measured fact. Companion to `TRENCH_CORPUS_FINDINGS.md`. Every entry
names its evidence, confidence level, and what remains unproven.

---

## THE DEFINITIVE AUTHORING LAW

### Frozen Poles, Animated Zeros

**Evidence:** 33 recipe families (R001–R033) extracted via serial lane ablation
across 132 P2K pose observations (`recipe_index_v1.json`, 3168 rows). Per-lane
relative movement data shows poles stay still while zeros and scale carry the
motion.

**Contract** (from `recipe_index_v1.json`):

```json
"recipeDoesNotSupplyPoleScaffolds": true,
"recipeAppliesToFrozenPolesThroughZeroOnlyAuthoring": true
```

**Interpretation:** The harmonic framework (pole frequencies and radii) is a
chord — it is authored once per pose and frozen. Morph and Q motion comes
entirely from zero trajectories and broadband scale changes. This is the
opposite of an ARMA fitter that moves everything simultaneously; it is
consistent with how ROM bodies reuse identical pole sets across characters.

**Confidence:** HIGH. Supported by 33 recipe families, 3168 transition rows,
zero unmatched rows. Every measured ROM family follows this contract.

**Pipeline:**

```
1. Pick recipe family (R001–R033) → statistical profile for all 6 lanes
2. Fit frozen poles to a measurement source → harmonic framework
3. Author zero trajectories → signed log2 octave ratios, per the recipe's
   zeroBehaviorByPose norms (median ± observed min/max)
4. Author scale deltas → per-edge gain, per the recipe's scaleDbDelta norms
5. Lane ablation audit → removedRmsDb must fall within recipe's observed range
6. Taste linter → enforce register, voicing, motion, zero grammar axioms
7. Ears → the only shipping gate
```

**Not proven:** That E-mu authors consciously used this "frozen poles" workflow.
The contract is inferred from the statistical structure of the decoded corpus,
not from documentation of their authoring process.

---

## ARCHITECTURAL HYPOTHESES

### Bass Anchor

**Evidence:** 22 of 130 measured voices sit below 400 Hz (ROM_MUSIC_THEORY.md,
`scratchpad/rom_music_theory.py`). The 79 Hz sub pole appears verbatim across
BolandBass, LucifersQ, BassTracer, and BassBox-303. Millennium S1 anchors at
59 Hz across all morph positions.

**Interpretation:** At least one lane is typically assigned to protect
sub-fundamental energy. This lane often uses a real pole or a pole-zero
handshake near DC with deep scale cut, creating a floor that holds steady
while interior voices morph.

**Confidence:** HIGH. Measured count and exact Hz values from decoded corner
words.

**Workstation test:** Remove the lowest-frequency lane from a known-good body
and measure the cumulative response at 40–100 Hz. The loss should be ≥20 dB
in the sub band.

**Not proven:** That E-mu designers consciously reserved "Slot 1" for this role.

---

### Mouth Voice Cluster

**Evidence:** 57 of 130 measured voices sit between 400 Hz and 3.5 kHz.
Adjacent spacing concentrates at 1, 3, 4, and 5 semitones. TalkingHedz places
formants at 891, 1570, 2348 Hz — not harmonic, not equal-tempered.

**Interpretation:** The interior 3–4 lanes carry the filter's identity. They
are spaced tightly in the mouth register using relative intervals, not absolute
Hz targets. The spacing reads as a chord voicing — seconds, thirds, fourths —
not a swept EQ.

**Confidence:** HIGH for the spacing measurement. MEDIUM that "mouth voice" as
a category was a conscious design target rather than an emergent property of
vowel formant spacing.

**Workstation test:** Author a body with all voices in one band (all below
400 Hz or all above 3.5 kHz). The result should sound categorically wrong
compared to any ROM body.

**Not proven:** The term "mouth voice" is our label. E-mu's manuals describe
vowel formant filters but do not use this register taxonomy.

---

### Air Voice / Spare-the-Air

**Evidence:** 84 of 130 measured voices sit above 3.5 kHz — the most populated
register. TalkingHedz's air pole at 4607 Hz is spared the Q push (kept at
~30–48 dB at Q100). The 17.6 kHz air pole recurs verbatim across four
characters.

**Interpretation:** High-frequency poles provide sheen and boundary enclosure.
On Q pushes, the highest pole is routinely spared the radius expansion to
prevent the filter from turning glassy or harsh.

**Confidence:** HIGH for the population count and for the TalkingHedz Q-sparing
measurement. MEDIUM that "spare the air" was a conscious rule rather than an
emergent property of vowel acoustics (air formants are naturally lower-Q).

**Workstation test:** Take a known-good body, push Q to 100, then add the same
Δr to the air pole that the mouth poles receive. The top end should become
harsh. Now back off the air pole Δr to zero. The difference is measurable
and audible.

**Not proven:** The term. E-mu's patent describes independent pole/zero radius
control but does not prescribe which poles to spare.

---

### Terminal Frame

**Evidence:** Millennium M0 parks five sections at 10–16 kHz as a "dark ceiling
frame" while S1 anchors the sub. ZoomPeaks S1 rockets +70 st and S6 falls −56 st
— outer sections relay to keep boundaries intact while the interior zooms.

**Interpretation:** The outermost lanes (typically S1 and S6) enclose the
active morphing space. They may relay duties mid-morph (the anchor relay) but
their structural role is boundary maintenance, not interior motion.

**Confidence:** MEDIUM. Supported by Millennium and ZoomPeaks measurements but
not consistently observed across all 33 families. Many bodies have no clear
"terminal frame" — the outer lanes participate fully in the morph.

**Workstation test:** Identify the two outer lanes in a body. Swap their
M100 destinations with interior lanes. If the frame hypothesis holds, the
result should lose its spectral boundary coherence.

**Not proven:** That all ROM bodies follow a frame/interior split. The
evidence is strongest for Millennium-class LPF bodies and weakest for
vowel filters.

---

## MOTION HYPOTHESES

### Anchor Relay

**Evidence:** TalkingHedz S2 drops to 201 Hz as S6 rises 199→1789 Hz (+38 st).
The two lanes trade bass-floor responsibility mid-morph so the frame never
loses its low end. EarlyRizer S2 leaps +69 st while S3–S6 fall — another
handoff.

**Interpretation:** When the lowest voice must move upward for the morph
journey, another voice descends to take its place. This is a composed relay,
not an emergent property of the interpolation.

**Confidence:** HIGH. Exact Hz values from decoded TalkingHedz and EarlyRizer
corner words. The cumulative response at M50 confirms the floor is maintained.

**Workstation test:** Re-pair TalkingHedz S2 and S6 destinations so they move
in parallel instead of exchanging. Measure the cumulative response at M50 in
the 100–300 Hz band. The floor should dip.

**Not proven:** That E-mu authors thought of this as a "relay." The measured
behavior is fact; the term is ours.

---

### Destination Re-pairing

**Evidence:** KlubKlassik and AcidRavage share an identical M0 pose but wire
the same M100 pole frequencies to different sections. Result: KlubKlassik has
mid-morph crossings; AcidRavage has zero crossings (smooth parallel travel).

**Interpretation:** A single pose vocabulary can produce multiple characters
by re-pairing which starting pole connects to which ending pole. Same
destinations, different journeys. This is a production method, not an
accident of similar design.

**Confidence:** HIGH. Verbatim M0 word match between KlubKlassik and
AcidRavage. M100 poles are the same set of frequencies assigned to different
sections. The mid-morph responses differ measurably.

**Workstation test:** Load KlubKlassik's M0 and M100 poses. Swap the M100
lane assignments for two sections. Measure the M50 response. Compare to the
original. The journey shape changes while the endpoints are identical.

**Not proven:** That E-mu used a "re-pair" tool or workflow. The byte evidence
proves the result; it does not prove the method.

---

### Contrary Motion = Drama, Parallel = Sweep, Oblique = Talk

**Evidence:** Motion taxonomy from `scratchpad/rom_music_theory.py`: 18/33
contrary, 7/33 parallel, 5/33 oblique. Millennium (12 crossing pairs) is the
most aggressive contrary body. MegaSweepz (parallel, zero crossings) is the
smoothest sweep. TalkingHedz (oblique, 3 voices held) is the most vocal.

**Interpretation:** The motion species determines the acoustic character.
Contrary motion creates turbulent, aggressive mid-morph moments at the
crossing points. Parallel motion creates smooth, predictable sweeps. Oblique
motion holds a vocal "face" while one articulator moves.

**Confidence:** HIGH for the classification. The correlation between species
and character is consistent across all 33 dossiers. MEDIUM that the species
was a conscious authoring choice rather than an emergent property of the
endpoint selection.

**Workstation test:** Take a contrary-motion body. Re-wire all lanes to move
in parallel while keeping the same M0 and M100 endpoints. The mid-morph
response should lose its aggressive character. The opposite test (making a
parallel body contrary) should introduce turbulence.

**Not proven:** The exact causal link between crossing count and perceived
aggression. Correlation is measured; causation is inferred.

---

## Q-AXIS HYPOTHESES

### The Three Q Clades

**Evidence:** Only 38% of stages push pole radii under Q. The remaining 62%
re-voice to new notes, retune, stay flat, or back off. Specific clades:

| Clade | Example | Measured behaviour |
|---|---|---|
| Radius push | TalkingHedz | +0.01 to +0.05 uniform, air pole spared |
| Re-voicing / tuner | DJAlkaline | Q100 poles at shifted Hz, radii unchanged |
| Lurker snap | BassTracer S6 | r=0.64 at Q0 → r=1.0 at Q100 (+0.36 Δr) |

**Interpretation:** Q is not a resonance knob. It is a multi-tool that the
composer assigns per-lane. A lane may intensify, shift frequency, invert its
shape, or snap from invisible to dominant.

**Confidence:** HIGH for the 38/62 split and the per-filter measurements.
MEDIUM that the three clades are a complete taxonomy rather than a convenient
grouping of observed behaviors.

**Workstation test:** Author a body where every lane receives the same Δr
under Q. Run the taste linter — it should flag this as a violation of Axiom 5.
Now re-author with one lane receiving the full Δr and the others backed off.
The character should change from "generic resonance" to something with a
specific Q attitude.

**Not proven:** That the three clades exhaust the design space. Additional
clades may exist in the 3 unclassified bodies.

---

### The Lurker

**Evidence:** BassTracer S6 sits at r=0.64 (real pole, non-resonant) at Q0
and snaps to r≈1.0 (+0.36 Δr) at Q100. BassOMatic S4 starts at r=0.65 and
snaps to r≈1.0 (+0.35 Δr) at Q100. LucifersQ S6 starts as real pole r=0.75
and becomes a complex mid resonance at Q100 (+0.25 Δr).

**Interpretation:** A deliberate design pattern: one lane is parked at a
safe, low-radius state at Q0, contributing almost nothing to the audible
response. At Q100 it "snaps" into a screaming resonance. The drama is in
the birth of the resonance, not its intensity.

**Confidence:** HIGH. Three bodies exhibit the exact same pattern with
similar Δr values. The lane ablation data confirms the lane is low-impact
at Q0 and dominant at Q100.

**Workstation test:** Author a body with five honest EQ lanes and one
"lurker" lane at r=0.65. Sweep Q from 0 to 100. Measure the lurker lane's
contribution (via ablation) at Q0 vs Q100. It should go from <3 dB RMS
impact to >20 dB.

**Not proven:** That E-mu authors conceived of this as a "lurker" pattern.
The byte evidence proves the behavior; the name and the design intent are
our interpretation.

---

## COMPOSITION HYPOTHESES

### Cumulative Construction (Signal-So-Far)

**Evidence:** E-mu's internal visual compiler displayed the cumulative Bode
plot updating in real-time as poles were dragged (Kyma/ARMAdillo documentation,
Symbolic Sound archive). Measured ROM sections often look strange in isolation
but compose correctly in cascade.

**Interpretation:** No section is designed or judged by its solo response.
The "signal so far" — cumulative cascade response of stages 1 through N —
is the only meaningful view during authoring. This matches BUILD mode's
signal-so-far panels.

**Confidence:** HIGH for the visual compiler existence (documented in Kyma
materials). HIGH for the "solo looks strange, cumulative works" observation
(verifiable on any ROM body by probing individual stages). MEDIUM that E-mu
authors exclusively used the cumulative view rather than toggling between
solo and cumulative.

**Workstation test:** Open any ROM body in BUILD mode. Toggle between solo
and signal-so-far for each stage. Note how stages 3–5 often have bizarre
solo responses that make perfect sense in the cumulative context.

**Not proven:** The exact E-mu authoring UI. The Kyma documentation describes
a visual compiler; the Emulator X Morph Designer is a later, simplified
version.

---

### The Library Rail

**Evidence:** 10 tuning-level reuse relations in 33 products. Millennium
and MeatyGizmo share both Q0 corners verbatim. KlubKlassik, AcidRavage,
and ToothComb share the same M0 pose. BolandBass, LucifersQ, BassTracer,
and BassBox-303 share a vocabulary of four specific pole frequencies.

**Interpretation:** E-mu maintained a small library of proven poses and
shipped new characters as recombinations — same starting pose, different
destination wiring, different Q attitude. This maximized product count
within tight ROM budgets while maintaining quality: a reused pose is a
pre-verified pose.

**Confidence:** MEDIUM. The reuse evidence is high-confidence (verbatim
words). The "library rail production paradigm" is a strong inference from
the reuse pattern but is not proven by any recovered E-mu internal document.

**Workstation test:** Build two bodies from the same M0 pose. Wire different
M100 destinations (re-pairing). Wire different Q attitudes. The two bodies
should sound like siblings — same family, different characters.

**Not proven:** That E-mu's internal workflow used a literal "library" of
poses selected from a menu. The reuse could also be explained by a single
designer reusing their own work across products.

---

## RECIPE PIPELINE CONTRACT

### Connecting Measurement Sources to the SOS Grammar

**Evidence:** 20+ recipe directories in `df2-workstation/recipes/` containing
measurement targets organised by category (vocal, circuit, modal, phononic,
HRTF, aeroacoustic, etc.). The `recipe_index_v1.json` provides statistical
profiles for 33 recipe families derived from the P2K ROM corpus.

**Interpretation:** The recipe index is the bridge between a raw measurement
and a composed body. It does not supply pole scaffolds (the `recipeDoesNotSupplyPoleScaffolds` contract) but provides the statistical norms
that keep a new body in-family.

**Pipeline:**

```
MEASUREMENT SOURCE (WAV, IR, target spectrum)
       │
       ▼
FROZEN POLE FIT ─── place 6 pole pairs to match target
       │              (trench_fit_arma_endpoint, 6-stage certified)
       │
       ▼
RECIPE FAMILY ───── select R001–R033 matching the target category
       │              (vocal → R00x, circuit → R01x, modal → R02x, etc.)
       │
       ▼
ZERO AUTHORING ──── zero trajectories per the recipe's zeroBehaviorByPose
       │              norms. Signed log2 octave ratios relative to poles.
       │              Poles stay frozen.
       │
       ▼
SCALE AUTHORING ─── per-edge gain per the recipe's scaleDbDelta norms.
       │              Median + observed min/max as guardrails.
       │
       ▼
LANE ABLATION ───── each lane's removedRmsDb must fall within the
       │              recipe's observed range. Lane alone span in-family.
       │
       ▼
TASTE LINTER ────── enforce register architecture, intervallic voicing,
       │              motion taxonomy, zero grammar, Q attitude.
       │
       ▼
EARS ────────────── the only shipping gate. No body ships unheard.
```

**Confidence:** MEDIUM. The recipe index is built from ROM data and the
pipeline logic follows from the frozen-poles contract. But the pipeline
has not been exercised end-to-end on a novel measurement source. The first
full run is the proof.

**Workstation test:** Pick a measurement source from `recipes/vocal/`. Fit
frozen poles. Select the nearest recipe family by category. Author zeros
and scales within the recipe's statistical bounds. Run lane ablation and
the taste linter. Render an audition sweep. Compare against the nearest
ROM reference. This is the Smalltalk method applied generically.

**Not proven:** That the recipe index categories (R001–R033) map cleanly to
measurement source categories (vocal, circuit, modal, etc.). The mapping is
a hypothesis to be tested.

---

## REFERENCE

### Confidence Levels

| Level | Meaning |
|---|---|
| HIGH | Supported by exact decoded values from multiple bodies, reproducible measurement |
| MEDIUM | Supported by pattern evidence but not exhaustively verified or missing direct documentation |
| HYPOTHESIS | Plausible interpretation of observed behavior, not yet tested by experiment |

### Distinction from Corpus Findings

`TRENCH_CORPUS_FINDINGS.md` contains only claims backed by decoded data
with generating script references. This document contains interpretations,
proposed methods, and creative vocabulary. The boundary is:

- **Finding:** "TalkingHedz S6 moves 199→1789 Hz (+38 st)"
- **Hypothesis:** "S2 and S6 execute an anchor relay to maintain the bass floor"
