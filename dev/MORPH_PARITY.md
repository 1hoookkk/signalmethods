# The plugin's morph does not match EmulatorX3 on the same preset

Reported by ear 2026-08-22: identical presets, but the plugin and EmulatorX3
sound different **between** corners. This is a parity problem, not an authoring
problem — the corners are the same bytes on both sides.

## Ruled out: the interpolation arithmetic

Our `interpolate_word` (`native/trench-core/src/packed_body.cpp`) and the
plugin's `lerp_u16` (`trench-x3-clean/trench-core/src/minifloat.rs:56`) are the
same law:

```
diff  = (int32)b - (int32)a, as float
delta = (int16)(int32)(diff * frac)
out   = (uint16)(a + delta)
```

Compared against a faithful model of the device's arithmetic decoded from
firmware — Q15 axis weights, `SMUAD`, `>>15` renormalise, negate — over 60,000
random `(a, b, frac)` draws including the identity and S6 sentinel words:

| model | mismatch | worst word error |
|---|---|---|
| device, words read as **unsigned** | 30.4% | **1 LSB** |
| device, words read as signed int16 | 54.0% | 32768 |

So the device reads packed words as **unsigned**, and our arithmetic agrees with
it to within one LSB. The `int16` truncation looked like a wrap hazard and is
not one: the result is masked to 16 bits either way, so the truncation and the
mask are the same operation modulo 2^16. Not the cause.

## The cause is almost certainly the interpolation *domain*

Both corners of the question produce bit-identical responses **at** the corners.
Only the interior can differ. Measured over all 33 factory bodies at four
interior points each (132 points), comparing "interpolate the packed words then
decode" against "decode then interpolate the biquad coefficients", mean removed:

| | dB |
|---|---|
| median rms | **12.64** |
| p90 rms | 24.98 |
| worst single point | 126.8 |

Per-body at the centre of the plane (m=0.5, q=0.5): ace_of_bass 22.6, megasweepz
23.5, early_rizer 26.0, millennium 26.5, fuzzi_face 28.8 dB rms.

**Twelve to twenty-five decibels of difference in the interior, from a choice
that is invisible at the corners.** That is precisely the reported symptom.

## Which one is right depends on which machine we are matching

Our plugin interpolates in the packed domain. That is the established project
invariant, and it was independently confirmed today in the eurorack Morpheus
firmware: the trilinear tree at `0x080378DC` runs `SMUAD` over Q15 weights
against int16 corner values with a `>>15` renormalise per axis, and the minifloat
decode is applied to the *interpolated* word afterwards. For the hardware, packed
is correct and settled.

EmulatorX3 is a different implementation — E-mu's software emulation, not the
ARM firmware. Nothing establishes that it made the same choice. If it decodes
first and interpolates coefficients, the 12-25 dB above is exactly what would be
heard.

So there are two distinct targets and they are not the same:

- **match the hardware** — keep packed-domain interpolation, and the plugin is
  already correct;
- **match EmulatorX3** — measure what EmulatorX3 actually does first.

This needs deciding before any code changes. Changing the interpolation domain
to chase EmulatorX3 would break hardware parity, which is currently proven.

## The decisive next step

Get EmulatorX3's own interior output and compare it against both models. Not a
listening test — a numerical one:

1. Pick one factory preset whose corners we already hold byte-identical.
2. Drive EmulatorX3 to a known interior point (for example M=50, Q=50) and
   capture its magnitude response.
3. Score that curve against `packed_then_decode` and against
   `decode_then_lerp` at the same point.

Whichever it matches is what EmulatorX3 does, and the question is closed.
`trench-x3-clean/pyruntime/` already contains DLL bindings, and
`ref/presets/README.md` documents the live-object dump procedure through
`CPhantomRTFilter` — so a capture path may already exist rather than needing to
be built.

## Other candidates, not yet excluded

Ranked by how cheaply they could be checked, and all secondary to the domain
question above:

- **Morph parameter mapping.** How a knob position becomes an interpolation
  fraction. A different curve or range puts the same knob at a different
  interior point, which sounds different while every corner still matches.
- **Axis order.** Our law is M, then Q, then Z. Trilinear interpolation is not
  order-independent when the weights differ per axis.
- **Note-on versus continuous.** The Rossum manual, p. 6, records that the
  original hardware morphed one dimension in realtime and fixed the others at
  note-on: "interpolation in the frequency and transform dimensions were set at
  note-on and remained static for the remainder of the note." If EmulatorX3
  reproduces that and the plugin morphs all axes continuously, sweeps differ.
- **Coefficient update rate and smoothing**, per-sample versus per-block.

## Note on the earlier diagnosis

`dev/MORPH_QUALITY.md` diagnosed a different problem — that independently fitted
corners leave row assignment unconstrained, so the interior sweeps roots through
positions never authored. That remains true and worth fixing for **our own
fitted bodies**, and Kerkhoff & Boves 1993 states the mechanism independently.
But it does not explain the report here, because this is the same factory preset
on both sides. Two separate problems.
