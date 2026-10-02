# The authoring operation: decision

2026-09-08. What must be believed for the product decision, and nothing else. The supporting
material is in `AUTHORING_MATH.md` (derivations and sources), `AUTHORING_EXPERIMENT.md`
(measurements, with `evidence/research-results/authoring-operation/`), and
`AUTHORING_CONTRACT.md` (the reduced surface and its state contract).

In a serial cascade section gains multiply and responses add in dB; a corner is six sections of
five words, sixty bytes; a body is four corners and the chip's lerp; the ear decides.

## Thesis

HEADSPACE's fundamental operation is the chip's own interpolation between two complete,
compatible packed corner states, run through the cascade with its state retained, with the
excitation held fixed and chosen by the author. Nothing is separated at authoring time.
Separating excitation from shaping is an analysis problem that belongs to LENS and to ingest,
not to the morph, and the two problems are independent. Of the two, the morph must be solved
first, because it is the only operation whose output is what ships.

Defence, in three parts: (1) the separation x = h * e is not recoverable from x alone, and the
tool never needs to recover it, because the author supplies e; (2) inside the morph, the only
object that moves is the word vector, the coefficients and the state follow deterministically,
and the experiment shows the chip's slow word lerp with retained state behaves as well as a
state reset without its discontinuity; (3) nothing about perceived size of a step can be claimed
without listeners, and Martens gives the protocol for that claim.


## What the decision rests on

1. The morph is the chip's word lerp between two complete corners; the words, the coefficients
   and the running state are three different objects, and only the words are authored.
   (`AUTHORING_MATH.md` §2.)
2. Nothing in the tool needs to recover an excitation; the author supplies it.
   (`AUTHORING_MATH.md` §1.)
3. The chip's slow lerp with the state retained is as continuous as a reset and has no step;
   a derived state map is worse. (`AUTHORING_EXPERIMENT.md`, Part C.)
4. A radius edit by word 3 alone moves the pole frequency; the fix is to re-encode the pair.
   (`AUTHORING_MATH.md` §2, `AUTHORING_EXPERIMENT.md` Part B.)
5. No perceptual size of a step is claimable without listeners. (`AUTHORING_MATH.md` §3.)

## The highest-value change

Correct the radius and mask edits to re-encode both words of the pair, and turn the test into
an angle-preservation check. Acceptance: for every Klatt row and every P2K corner row with a
live pole, a radius edit to R' leaves the decoded frequency within one word-2 quantum,
0.2 cents at 620 Hz, of the original; the same for zeros; `trench_quad` green.


## Source, equation, code, discrepancy

| Source | Says | Code | Discrepancy |
| --- | --- | --- | --- |
| US 5,170,369 col. 6 | B2' = 1 − R², encode log-ish; B1' − B2' encodes angle | `decode_word`, `section_values_to_biquad`: d3 = 1 − R², 4d2 = a1 + 2 − d3 | none; but word 2 depends on R and θ |
| US 5,170,369 col. 10 | C(x) = Ca + x(Cb − Ca) per sample, clamp | `interpolate_word` in code units; runner lerps log(d) over 256 samples | two domains: code-linear across corners, log-linear inside the runner |
| US 5,170,369 col. 3 | DC gain fixable to unity per section | P2K DC-unity rule in `unityDc`; not in the section itself | rule lives in authoring, not the runner |
| US 5,248,845 FIG. 2, cols. 5 to 11 | average, warp, estimate, invert, residual; minimum phase assumed | no inversion anywhere; reads are poles only | the tool never separates; consistent with thesis |
| US 10,514,883 cols. 11 to 13 | angle and log(1 − R) separate tables, trilinear, decode by exp | `interpolate_biquads_float` mixes code-linear and decoded-linear | different encoding; not substituted |
| Peevers MSv2 | STFT filter; GAL envelope, Haykin pp. 215 to 219 | `Peevers.cpp` GAL for readout only | none |
| Martens 1987 §7, Freed & Martens 1986 §2 | perceptual claims need direct ratings | none | no listening data exists |
| FIELD_PUSH.md | "angle writes word 2, radius writes word 3" | `Field::setRadius` writes word 3 only | falsified by the proof above |

