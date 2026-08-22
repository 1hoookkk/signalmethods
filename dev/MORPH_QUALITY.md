# The morph sounds worse than the corners

Reported by ear, 2026-08-22: fitted corners sound like their targets; the
interior between them does not. Recording the diagnosis and the evidence for it.
Not yet fixed, not yet confirmed by measurement.

## The mechanism

The interior is a per-word linear interpolation of the packed corners, decoded
after interpolation. Section rows interpolate independently: row 3 of corner A
lerps to row 3 of corner B, whatever those two rows happen to contain.

Nothing in the fitter constrains which root lands in which row. Each corner is
fitted on its own, and the optimiser puts a root wherever the loss is lowest.
Two corners fitted independently can therefore hold completely unrelated roots
in the same row.

When that happens the interior sweeps that row's root from one frequency to a
distant one, passing through every position between. Those intermediate
positions were never authored, never scored, and never heard during fitting.
The corners are correct and the path between them is arbitrary.

## Why this session's findings make it concrete

`DVTD_VOWEL_FIT.md` §3b established that row assignment is invisible at a corner
and decisive between corners: because a cascade is a product, moving a zero from
row N to row N+1 leaves the corner response bit-identical, while completely
changing what the interior does. That is exactly the degree of freedom the
fitter is currently leaving unconstrained.

`NATIVE_CONTAINER_SURVEY.md` §9a measured how far apart same-index roots sit
across the corpus: pole frequency at a fixed section index scatters 1.15 to 3.33
octaves between bodies, and up to nine and a half octaves in the worst family.
That is the size of the sweep an unconstrained row assignment can produce.

E-mu's own bodies do not look like that. In `F022 AEParaVowel` the roots line up
across rows so that adjacent corners hold related geometry, and one pair is byte
identical between corner 0 and corner 1 — deliberate, not incidental.

## The manual says the interior is the product

Rossum Morpheus manual p. 7:

> "By varying that single CV-controllable parameter, you're actually
> interpolating between 20 different frequency, bandwidth, resonance, and gain
> parameters simultaneously."

Twenty parameters, moving together. The instrument's value is the path, not the
endpoints. A fitter that scores only endpoints is optimising the wrong thing.

## What to try, in order

1. **Measure it before fixing it.** Render the interior at several morph
   positions between two fitted corners, and score each against the interpolated
   *target* curves. If interior error is much worse than corner error, the
   diagnosis is confirmed. This is cheap and nothing should be built before it
   runs.
2. **Constrain row assignment across corners.** After fitting each corner
   independently, permute each corner's rows to minimise total root movement
   between adjacent corners. Because permuting rows does not change any corner's
   response, this is free — it improves the interior at zero cost to the fits
   already accepted. This is the obvious first fix.
3. **Score the interior directly.** Add interior positions to the loss so the
   fitter optimises the path, not just the endpoints. More invasive, and it
   changes the objective, so it needs its own fixture.

Step 2 is the one worth doing first: it is reversible, provably free at the
corners, and directly targets the mechanism.

## Caveat

The row-7 law from `NATIVE_CONTAINER_SURVEY.md` constrains any permutation —
row 7 must hold no zero, so a permutation may not move a zero-bearing row into
position 7.
