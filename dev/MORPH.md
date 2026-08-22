# The morph: two separate problems with the same symptom

Both surfaced 2026-08-22, both sound like "the in-between doesn't sound right",
and they have nothing to do with each other. Keep them apart.

- **§1** — the plugin does not match EmulatorX3 on the *same factory preset*.
  A parity problem. Corners match, interior doesn't.
- **§2** — our *own fitted* bodies morph badly between corners we authored.
  An authoring problem. Different cause, different fix.

---

# §1 Parity: plugin vs EmulatorX3, same preset

Reported by ear: identical presets, different sound between corners.

## Ruled out — the interpolation arithmetic

Our `interpolate_word` (`native/trench-core/src/packed_body.cpp`) and the
plugin's `lerp_u16` (`trench-x3-clean/trench-core/src/minifloat.rs:56`) are the
same law:

```
diff  = (int32)b - (int32)a, as float
delta = (int16)(int32)(diff * frac)
out   = (uint16)(a + delta)
```

Compared against a model of the device arithmetic decoded from firmware — Q15
axis weights, `SMUAD`, `>>15` renormalise, negate — over 60,000 random
`(a, b, frac)` draws including the identity and S6 sentinel words
(`dev/lerp_parity.py`):

| model | mismatch | worst word error |
|---|---|---|
| device, words read as **unsigned** | 30.4% | **1 LSB** |
| device, words read as signed int16 | 54.0% | 32768 |

The device reads packed words as **unsigned**, and our arithmetic agrees to
within one LSB. The `int16` truncation looked like a wrap hazard and is not one:
the result is masked to 16 bits either way, so truncation and mask are the same
operation modulo 2¹⁶. Not the cause.

## The cause is almost certainly the interpolation *domain*

Both choices give bit-identical responses **at** the corners; only the interior
can differ. Measured over all 33 factory bodies at four interior points each
(132 points), "interpolate packed words then decode" against "decode then
interpolate biquad coefficients", mean removed (`dev/morph_domain.py`):

| | dB |
|---|---|
| median rms | **12.64** |
| p90 rms | 24.98 |
| worst single point | 126.8 |

At plane centre (m=0.5, q=0.5): ace_of_bass 22.6, megasweepz 23.5, early_rizer
26.0, millennium 26.5, fuzzi_face 28.8 dB rms.

**Twelve to twenty-five decibels in the interior, from a choice invisible at the
corners.** That is precisely the reported symptom.

## Which is correct depends on which machine we are matching

Our plugin interpolates in the packed domain. That is the project invariant and
it was independently confirmed today in the eurorack firmware: the trilinear tree
at `0x080378DC` runs `SMUAD` over Q15 weights against int16 corner values with a
`>>15` renormalise per axis, and the minifloat decode is applied to the
*interpolated* word afterwards. For the hardware, packed is settled.

EmulatorX3 is a different implementation — E-mu's software emulation, not the
ARM firmware. Nothing establishes it made the same choice.

Two distinct targets, and they are not the same thing:

- **match the hardware** — keep packed-domain interpolation; the plugin is
  already correct;
- **match EmulatorX3** — measure what EmulatorX3 actually does first.

Decide before changing code. Switching the domain to chase EmulatorX3 would
break hardware parity that is currently proven.

## The decisive next step

Numerical, not a listening test:

1. Pick one factory preset whose corners we hold byte-identical.
2. Drive EmulatorX3 to a known interior point (e.g. M=50, Q=50) and capture its
   magnitude response.
3. Score that curve against `packed_then_decode` and `decode_then_lerp` at the
   same point.

Whichever it matches is what EmulatorX3 does, and the question closes.
`trench-x3-clean/pyruntime/` already contains DLL bindings, and
`ref/presets/README.md` documents the live-object dump through
`CPhantomRTFilter`, so a capture path may already exist.

## Secondary candidates, not excluded

All downstream of the domain question:

- **Morph parameter mapping.** How a knob position becomes an interpolation
  fraction. A different curve or range puts the same knob at a different
  interior point — sounds different, every corner still matches.
- **Axis order.** Our law is M, then Q, then Z. Trilinear is not
  order-independent when weights differ per axis.
- **Note-on versus continuous.** Rossum Morpheus manual p. 6: "interpolation in
  the frequency and transform dimensions were set at note-on and remained static
  for the remainder of the note." If EmulatorX3 reproduces that and the plugin
  morphs all axes continuously, sweeps differ.
- **Coefficient update rate and smoothing**, per-sample versus per-block.

---

# §2 Authoring: our own fitted bodies

Different problem. Here the corners are ones *we* fitted, and the interior was
never scored.

## The mechanism

The interior is a per-word linear interpolation of the packed corners, decoded
after interpolation. Rows interpolate independently: row 3 of corner A lerps to
row 3 of corner B, whatever those rows happen to contain.

Nothing in the fitter constrains which root lands in which row. Each corner is
fitted on its own and the optimiser puts a root wherever the loss is lowest, so
two corners fitted independently can hold completely unrelated roots in the same
row. The interior then sweeps that row's root from one frequency to a distant
one, through positions never authored, never scored, never heard during fitting.
The corners are correct and the path between them is arbitrary.

## Why the session's findings make it concrete

`DVTD_VOWEL_FIT.md` §3b: row assignment is invisible at a corner and decisive
between corners — because a cascade is a product, moving a zero from row N to
N+1 leaves the corner response bit-identical while completely changing the
interior. That is exactly the freedom the fitter leaves unconstrained.

`NATIVE_CONTAINER_SURVEY.md` §9a: pole frequency at a fixed section index
scatters 1.15 to 3.33 octaves across the corpus, up to nine and a half octaves in
the worst family. That is the size of sweep an unconstrained row assignment can
produce.

E-mu's own bodies do not look like that. In `F022 AEParaVowel` roots line up
across rows and one pair is byte-identical between corners 0 and 1 — deliberate.

## Confirmed by primary literature

Kerkhoff & Boves, Eurospeech 1993, hit exactly this. p. 1706:

> "Even if manipulations of zeros do not upset the 'stationary' system response
> of the overall ARMA structure, they may still cause problems internally. When
> the zero parameters of a section in the chain are changed, the output of that
> section may change its amplitude abruptly. This may well upset the AR part of
> the succeeding section (simply because its input grows instantaneously), and
> this disturbance will last until the output of that section has damped out."

And p. 1705: "the behaviour of a time-varying ARMA system is sensitive to
internal group delays that are immaterial in stationary systems."

Section order is immaterial for a stationary cascade and **not** immaterial for a
morphing one — stated independently, thirty years ago. Their fix (p. 1707) was
architectural, "all zeros are lumped in one large section, and the poles in
another one", which is unavailable to us since the container fixes one pole pair
and one zero pair per section.

They also give a directly implementable rule for the crossing problem, the
`follow` interpolation, whose stated purpose is "to prevent a zero from
inadvertently cancelling a pole at the point where the two tracks cross":

```
RelF_zero  = (Zero_target - Form1_target) / (Form2_target - Zero_target)
RelQ_zero  = (Q_zero - Q_form) / Q_zero
Zero_track = Form1_track + RelF_zero * (Form2_track - Form1_track)
Q_track    = Zero_track / (RelQ_zero * Q_form(track) + Q_form(track))
```

with Form1/Form2 the next lowest and next highest formant, and "If the zero is
below F1, Q_form is the Q-factor of F1." Their rule developers "display a clear
preference for the option follow". Full text in
`dev/CITATIONS_POLE_ZERO_PLACEMENT.md`.

## What to try, in order

1. **Measure before fixing.** Render the interior at several morph positions
   between two fitted corners and score each against the interpolated *target*
   curves. If interior error is much worse than corner error, confirmed. Cheap;
   nothing should be built before it runs.
2. **Constrain row assignment across corners.** After fitting each corner
   independently, permute each corner's rows to minimise total root movement
   between adjacent corners. Permuting rows does not change any corner's
   response, so this is free — it improves the interior at zero cost to fits
   already accepted. The obvious first fix.
3. **Score the interior directly.** Add interior positions to the loss so the
   fitter optimises the path, not just the endpoints. More invasive, changes the
   objective, needs its own fixture.

Constraint on any permutation: row 7 must hold no zero
(`NATIVE_CONTAINER_SURVEY.md` §1), so a zero-bearing row cannot move there.
