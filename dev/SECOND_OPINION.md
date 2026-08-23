# Second opinion — the P2K fitter and the TRENCH instrument, from a DSP seat

2026-08-23. Written after reading `current.md`, `dev/METHODS.md`, the Morpheus and
Mo'Phatt manuals, and the literature those documents lean on (Bell 1961; Fant's
STL-QPSR series 1960–64; Kerkhoff & Boves 1993; Klatt 1980; Rossum US 5,170,369;
Martens 1985/2001/2002). Everything here is an opinion with its reasoning shown. Where a
claim can be tested cheaply, the test is named. Nothing below was run.

## 1. What the project has right, and should not re-litigate

- **The object is a serial cascade; dB adds.** Every shortcut that follows from this
  (section contribution = its own curve, the objective is a sum, one zero can dig any
  notch) is correct. Kerkhoff & Boves built the same thing for the same reason.
- **The zeros were free.** The affinity null (1.26 st from a free fit against the bank's
  1.75 st) is the right experiment and the right reading. Fant's bound pairs are a hand
  method from 1960, not a rule the bank follows. Do not add a placement prior.
- **Unity DC gain as a closed form** is right and is literally Kerkhoff's `a+b+c=1`.
  Level normalisation "almost right and wrong" is a correct diagnosis.
- **Snap-then-polish on the exponent-indexed lattice.** Quantised coefficient search
  with exhaustive per-axis sweeps plus the radius-at-constant-frequency move is the
  textbook way to finish a fit on a minifloat grid. The entangled-axes observation
  (`p = 4·d_mag + d_rsq − 2`) is the whole reason that move exists.
- **Corners are independent postures; the interior is the machine's lerp.** One
  addition worth writing down because it is the *DSP reason* Rossum lerps coefficients
  rather than roots: the stability region of a second-order section in `(p, q)` is a
  triangle, hence convex, so any convex combination of two stable corners is stable. The
  same is not true of root-space interpolation with independent radii and a fixed
  ceiling. That is the ARMAdillo argument and it means "interior not authored" is not
  merely a ruling but a property of the encoding.
- **Bell's three choices** — whole-response comparator, mean-removed weighted error,
  variation score for placement — are the 1961 design and the right one.

## 2. Where I disagree, or where the current reading is weaker than it looks

### 2.1 The continuous-solver failure is parameterisation, not the Jacobian

`current.md` proposes that forward differences at `h = 1e-6` are poorly conditioned near
`r = 0.99` and that an analytic Jacobian would settle an 11–23 dB defect. Arithmetic
says otherwise. `d(dB)/dr` near `r = 0.999` is roughly `(20/ln10)·1/(1−r) ≈ 8,700 dB
per unit radius`; the response is evaluated to ~1e-12 relative, so a difference quotient
at `h = 1e-6` carries ~1e-4 dB of noise on a derivative of order 1e4. That is fine. The
analytic derivative is still worth having because it is free:

```
|1 + p·z⁻¹ + q·z⁻²|²  at z = e^{jω}
   = 1 + p² + q² + 2p(1+q)·cos ω + 2q·cos 2ω
∂/∂p = 2p + 2(1+q)·cos ω        ∂/∂q = 2q + 2p·cos ω + 2·cos 2ω
```

— but it will not fix an 11–23 dB miss. Two things will, and both are standard:

1. **Optimise in `(log f, log bw)` per root, not in `(p, q)` or word space.** Bandwidth
   in octaves is the natural coordinate (the project already uses it for the strips and
   the caricature knob). Radius can then never cross 1, the Jacobian is well scaled, and
   the pole ceiling becomes a bound on one coordinate rather than a clamp that flattens
   steps.
2. **Bounds by transform, never by clamp inside a trust-region step.** A clamp after the
   step makes the model/actual reduction ratio lie, the trust region collapses, and LM
   stalls wherever the clamp first bit. That produces exactly the signature reported:
   coherent, repeatable, large residuals from topological seeds.

Test: refit the ten Klatt targets with the continuous stage in `(log f, log bw)` and
sigmoid-bounded radius. If the 11–23 dB goes to ~0.1 dB the Jacobian was never the
problem.

### 2.2 The ARX route: name the fix correctly

The equation-error bias in the Levy solve is the 1959 problem; the classical frequency-
domain remedy is **Sanathanan–Koerner iteration** (reweight by `1/|A_{k−1}|²` each
pass), of which Steiglitz–McBride is the time-domain cousin. Three to five SK passes
normally remove the radius collapse (0.907 → 0.425) reported. It remains a *seed*
generator — SK converges to a fixed point that is not the least-squares optimum — so it
belongs before snap-and-polish, not instead of it.

### 2.3 The locked S6 zero is not a floor under all-pole targets

Proposed in `current.md` as the first thing to settle. It can be settled on paper.
The S6 zero sits at `r = 0.9999981`; its 3 dB notch width is `(1−r)·sr/π ≈ 0.03 Hz`.
The section's own pole may sit at the same angle with `r ≤ 0.99979`, width ≈ 3 Hz.
Parked on each other the pair leaves a residual dip ≈ 3 Hz wide; the nearest bin on a
512-point log grid is ~0.7 % of frequency away (≈ 7 Hz at 1 kHz, 35 Hz at 5 kHz), so
the residual on the grid is a fraction of a dB unless a bin lands inside that 3 Hz.
Elsewhere the pair cancels to within `20·log10(r_zero/r_pole) ≈ 0.002 dB`. So the
floor is ~0 and the 1.3–3.3 dB tail on the front vowels is **search**, which agrees
with the project's own data (three vowels at 0.2–0.3 dB).

Test, one line: for each failing vowel, evaluate the fitted cascade with S6 replaced by
identity and compare the residual. If it barely moves, the floor story is dead.

### 2.4 The rows model is a peaking EQ, and the bank is mostly not peaks

The strips expose Fc/Bw/Gain with the zero placed on the pole. That is the Regalia–Mitra
/ Orfanidis peaking form. The bank's own-stage pole-zero intervals are +0.11 oct (REZ) to
+2.18 oct (DST), with a third of all stages beyond two octaves — those are shelves and
tilts. The project's own test says the four controls reproduce 95 of 562 factory EQ
rows within 3 dB. That is not a bug in the fitter; it is the expressivity of the form.

The earlier SOS design spec had a fourth control — **zero offset `fz/fp`** — and the
strips dropped it. Fant's "bound pair" is exactly a pole and zero at a small interval,
and it is the element that produces broadband tilt. With offset restored, one row spans
peak (offset 0), shelf (offset ±½–1 oct), tilt (offset > 2 oct) and notch (negative
gain), and most of the 467 unreachable rows become authorable from the strip rather than
only by paste. The keeping-offset edit law already preserves an authored offset; only
authoring one from scratch is missing.

### 2.5 Bell's correction spectrum is not optional for recorded targets

A recording is source × tract × radiation. Fant gives the source at −12 dB/oct and
radiation at +6 dB/oct, net −6. The factory vowel bodies carry a median −5.72 dB/oct —
the bank stores *radiated* spectra, not transfer functions. Two consequences:

- Fitting a Klatt transfer function will always produce a body flatter than the bank.
  That is not a fitter defect. Add the slope to the target or co-fit it.
- Fitting a recording without a co-fitted smooth curve spends rows on the source.
  Bell's six stored correction curves are the 1961 version of "a low-order smooth term
  in the search". The modern cheap version: a 2nd- or 3rd-order polynomial in `log2 f`,
  mean removed, as free parameters in the same LM. Cheaper still: a single real
  pole–zero tilt section outside the six.

Test: refit the ten Klatt targets once with −6 dB/oct added. If the residual drops and
the fitted rows stop carrying tilt, the hypothesis in `current.md` is confirmed.

### 2.6 Comparator symmetry — make it an excitation pattern, not a weight

Bell passes speech and candidate through the same 36 filters and compares *after*. The
project ERB-weights the residual, which is a different operation: it still scores
harmonic-scale detail the 1961 comparator blurred on both sides. The modern equivalent is
an excitation pattern — `|H|²` integrated through roex/gammatone ERB filters (Moore &
Glasberg), then dB — applied identically to target and candidate. Two notes:

- Once you do this, dB no longer adds across sections inside the comparator (power sums
  across a band). Keep the additive cascade for the inner search and blur only in the
  outer acceptance, or accept a slower inner loop.
- A deep, narrow notch (the trench) is nearly invisible in an excitation pattern. That is
  *correct* perceptually and it is the reason the factory's −100 dB trenches cost nothing
  to the ear and everything to an unweighted rms. The smeared comparator will stop the
  fitter fighting the trench and will also stop rewarding it. Depth then becomes an
  authoring choice, which matches "the trenches are the work".

### 2.7 The block cuts are headroom, not taste

Cuts at the tails of two blocks of three, from `{1, ½, ¼, ⅛}`, cumulative down the
chain (`S3 ≥ S6` in 128/128), never a compensating boost. That is a fixed-point DSP
rescaling a partial product by a barrel shift. The test the project has not run is the
right one: correlate the cut with the **peak of the partial cascade at the block
boundary** (S1·S2·S3 and S1…S6 at unity scale), not the overall peak. If the cut tracks
a fixed dBFS ceiling per block it is a hardware rule and our plugin needs none of it at
float precision — "apply none" stays correct and becomes principled.

### 2.8 Two shapers plus four resonators

S1 and S6 sit beyond 24 st from their own zero in ~55 % of corners; S3/S4 in 17 %/8 %.
With the radiated-spectrum reading above, this is simply where the −6 dB/oct and the
low-frequency body tilt are parked: two opposed shelves framing a passband. It argues
for a seeding strategy the project has not tried: seed S1 and S6 as tilt rows from the
target's first-order slope and fit the four middle rows to the mean-removed remainder.
It is a seed, not a slot law — METHODS §7 stands.

### 2.9 Small things

- Klatt Table II bandwidths are *synthesis* values; Fujimura & Lindqvist measured
  narrower B1/B2 for close vowels. Klatt himself (1980) reports bandwidth is a weak
  perceptual cue next to F1/F2. Low priority unless close vowels keep missing after 2.1.
- Lerping words in `(d_mag, d_rsq)` means `r²` and `r·cos θ` interpolate linearly, so
  the interior frequency trajectory is `cos θ(t) = lerp(r cos θ)/√lerp(r²)`: not log-
  linear. Sections far apart in angle sweep fastest near the ends. That is the machine;
  mention it in the ride audit so non-uniform sweeps are not read as authoring errors.
- "Authored or solved" is unlikely to be decidable from bytes. The geometry-byte
  histogram test (round-value clustering) is the only cheap discriminator proposed and
  it has not been run. The display patent is the strongest circumstantial evidence for
  an editor; the instrument being built now is that editor.

## 3. Open questions — one list, in the order I would take them

Each item names the test that closes it.

1. **Continuous stage in `(log f, log bw)` with transformed bounds.** Refit the ten
   Klatt targets. Closes 2.1 and, with it, most of "the search does not find optima it
   is proven to have".
2. **S6 floor on paper, then one line of code.** Swap S6 for identity on the failing
   vowels; compare. Closes the floor hypothesis either way.
3. **Correction spectrum.** Co-fit a smooth low-order curve; refit Klatt targets with
   −6 dB/oct added. Closes "why our fits are flatter than the bank" and makes recorded
   targets fittable without the dry source.
4. **Zero offset as the strip's fourth control.** Measure the bank coverage of
   (Fc, Bw, Gain, offset) against the 95/562 baseline. If it clears ~60 %, the strips
   author the bank rather than a fifth of it.
5. **Excitation-pattern comparator**, applied symmetrically, for acceptance only at
   first. Re-score the VOW cold tail (multi_q_vox 2.65, ubu_orator 2.55 …). If the tail
   collapses, the "search failing on notch structure" result was the metric.
6. **Block-boundary headroom test** for the cuts. Closes the gain-cut question as
   hardware, or leaves it open with a better measurement.
7. **Two-shelf seed** (S1/S6 from the target's first-order slope, four resonators on
   the remainder). Compare cold residuals against peel on the 132 factory corners.
8. **Sanathanan–Koerner on the ARX seed.** Five passes; does the 6.7 dB stall clear?
   Keep as a seed if so.
9. **Notch depth as an authoring control, not a fit output.** With 5 in place the
   fitter is indifferent to depth below the ear's resolution; the instrument should
   expose it (the trench fader already does for the low section).
10. **Real-axis pairs.** 46 of 132 corners. Needs a word law for `disc ≥ 0` so a strip
    can author one, or an explicit ruling that they arrive only by paste. After the HP
    fold they read as EQ rows whose Gain edit is a no-op; that is a hole a user will
    find.
11. **EmulatorX round trip, steps 3–5.** Still the only check that is not our encoder
    against our decoder.
12. **Geometry-byte histogram.** Run `bytes_alone.py`'s question. Cheap; may say nothing.
13. **Acceptance metric in the corpus PCA space** (Burred/Röbel/Rodet, Sandell &
    Martens). Read the paper properly before building; it may specify the projection.
14. **Perceptual calibration of the controls** by bisection (Martens 2001/2002). Last,
    because it calibrates a space the items above are still changing.

## 4. What this means for tonight

Nothing above changes how to make corners by ear. Three practical notes that do follow
from it: the factory bodies are radiated spectra, so a corner that sounds right will
usually carry a net downward tilt; cuts cost nothing to the ear and are where the
character is, so go deeper before going taller; and a corner is a posture, not a point
on a slider — make four that are each complete.
