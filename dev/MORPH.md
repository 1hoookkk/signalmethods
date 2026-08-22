# The morph: two separate problems with the same symptom

> **Context.** TRENCH builds a filter plug-in that interoperates with E-mu's
> Z-plane filter format. Tyson owns the hardware and the software this evidence
> comes from. Establishing an undocumented binary format from a device you own,
> so your own product can read and write it correctly, is ordinary compatibility
> engineering — the same work as supporting any undocumented file format. Device
> addresses and instruction sequences appear below because they are the evidence
> for the format's rules; without them these would be unsourced assertions, which
> `native/CLAUDE.md` ranks as the more dangerous failure. Nothing here
> circumvents protection, and no vendor code is copied into the product — only
> the numerical format rules needed for compatibility.

Both surfaced 2026-08-22, both sound like "the in-between doesn't sound right",
and they have nothing to do with each other. Keep them apart.

- **§1** — the plugin does not match EmulatorX3 on the *same factory preset*.
  A parity problem. Corners match, interior doesn't.
- **§2** — our *own fitted* bodies morph badly between corners we authored.
  An authoring problem. Different cause, different fix.

---

# §0 Measured 2026-08-22: corner, interior and a slow travel all match X3

Two EmulatorX3 renders of Talking Hedz playing the same dry sawtooth
(`Downloads/trench_capture/dry_saw_49hz_-12dBFS.wav`, root key, insert at
100%), scored against the real `trench_core.dll` through `pyruntime`
(`dev/x3_capture_null.py`, `dev/x3_capture_detail.py`). Response = capture
harmonics minus dry harmonics, ERB-weighted, mean-removed.

**Sweep** (`hedzenv.wav`: Filter Env + → Filter Frequency +100, attack set to
4 s, panel Frequency 0, Q 50). The morph position X3 actually played, frame
by frame, against the engine at Q 0.5:

    morph(t) = 0.2498·t + 0.0002   (t from note-on; residual rms 0.002)
    morph 1 reached at 4.003 s

A straight line, no pole, no step: the envelope attack is linear in morph
and the *engine's interior reproduces every frame at 0.15–0.8 dB rms*
(the 0.5–0.8 dB frames are sweep smear inside the analysis window). So the
interpolation domain and law are the X3's — the "decode-then-lerp" theory
in §1 below is dead, as the DLL read in `plugin/X3_MOVEMENT_SPEC.md` §2 had
already said. Interior parity at the response level is **done**.

**Static** (`hedznoenv.wav`: cord off, panel Frequency 0, Q 50):

    engine morph 0.00, q 0.50 — 0.16 dB rms   (morph 0.05 is already 3.7 dB,
    so the match is sharp, not a flat landscape)

(An earlier draft of this section read the panel as 50 and inferred a knob
mapping; Tyson confirmed the panel was at 0. No knob finding.)

So at a corner, across the interior, and along a 4 s linear travel, the
engine and X3 agree to < 1 dB. What these two captures do **not** test is
how X3 *smooths* a control that moves abruptly — its one-pole and block
ramp only show on steps and fast modulation, and a 4 s envelope is far too
slow to excite them. That, plus level staging outside the filter, is where
"doesn't sound the same" must now live.

**Step** (`hedzstep.wav`: Filter Env → Filter Frequency +100, Attack 1 =
2.0 s at level 0, Attack 2 = 0.000 s at level 100; `dev/x3_step_null.py`,
`dev/x3_step_fit.py`). Time-domain null, sample-aligned, filter-only engine
render (AGC/DC/saturation/nonlinearity/preamp/grit/key all off), level
matched on the settled tail:

| region | engine X3 path | engine per-sample |
|---|---|---|
| before the step, 1.0–1.9 s | **−80.4 dB** | −80.4 |
| 2.00–2.02 (first 20 ms) | −9 … −11 | −7 |
| 2.02–2.06 | −33.5 | −15 |
| 2.06–2.15 | −65.3 | −50 |
| settled, 6–11 s | **−72.1 dB** | −72.1 |

Trajectory fits on the per-sample path: a one-pole with τ ≈ 1–2 ms from
the step nulls the tail best (−68.6 dB at 2.06–2.15 for τ = 1 ms); that is
the X3's tick pole (R = 0.4516 per 32-sample tick ⇒ τ ≈ 1.2 ms). Host-buffer
-sized ramps (256–2048 samples) are *worse* by 10–20 dB at every region, so
X3 does **not** ramp across the DAW buffer; the 32-sample tick model is the
right one. The engine's X3 path reproduces the step to −33 dB after 20 ms
and −65 dB after 60 ms.

What remains is the first ~20 ms at about −10 dB, which no pole or ramp on
the morph reproduces. The likeliest cause is the capture, not the engine:
the envelope's "0.000 s" attack has a finite minimum segment time, so the
source of the step is itself a few-ms slope. A step from a MIDI CC would
settle this; it is not a product question.

**Engine chain, headless, at the real input level** (dry −12 dBFS, Morph 0,
Q 0.5, against `hedznoenv.wav`; X3's output peaks at −1.1 dBFS):

| engine chain | X3 − engine level | null, gain-matched | shape rms |
|---|---|---|---|
| filter only | **+4.14 dB** | −80.4 dB | 0.15 |
| + AGC / + saturation / + section nonlinearity | +4.14 | −80.4 | 0.15 |
| + DC blocker | +4.15 | −24.5 | 0.15 |
| full chain, shipping defaults | +4.15 | −24.5 | 0.15 |
| full chain + preamp 0.5 (Mackie input) | −5.08 | −0.4 | 6.66 |
| full chain + grit 0.5 | +3.62 | −20.8 | 1.23 |

Three facts:
1. **X3 is +4.14 dB louder than the filter math, constant.** Same +4.13 dB the
   2026‑08‑13 diagnostic found and could not place. It is EmulatorX's voice /
   preset output staging (the unread `voice+0x48` / preset volume), not the
   filter. At equal fader settings TRENCH is 4 dB quieter than X3 on the same
   preset — enough to hear as "less resonant, narrower sweep".
2. **AGC, saturation and section nonlinearity are transparent** at this level
   (−80 dB). The DC blocker is the one default that moves the null (−24.5 dB)
   — a phase/level change on a 49 Hz fundamental, shape unchanged; audible
   only as a touch less sub.
3. **PREAMP is a different instrument.** 0.5 on the Mackie input mode changes
   the spectrum by 6.7 dB rms and the level by 9 dB. If PREAMP (or GRIT) is
   up in an A/B, that is the whole difference by itself.

Not rendered here (JUCE wrapper, outside the DLL): SLAM limiter, MIX, the
preamp taper, the MOVE renderer. SLAM at default is a safety ceiling and
X3's −1.1 dBFS peaks sit below it.

**Conclusion for the product.** Corner (−80 dB), interior (< 1 dB), slow
travel (< 1 dB) and fast step (−33/−65 dB beyond 20 ms) all match X3 with
the engine's filter alone. Whatever makes TRENCH sound different from X3
is not in the filter, its interpolation or its movement law. The next null
must be TRENCH-the-VST rendered in the **same FL chain** as the X3 capture
(Tyson, 2026-08-22: "to null you must render TRENCH at the same stack") —
that measures the level staging and the defaults (AGC, GRIT, preamp,
FOLLOW, MOVE, KEY) that this harness switched off.

# §1 Parity: plugin vs EmulatorX3, same preset (superseded in part by §0)

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

The `int16` truncation looked like a wrap hazard and is not one: the result is
masked to 16 bits either way, so truncation and mask are the same operation
modulo 2^16.

**Correction, from reading the device decompilation directly.** An earlier pass
compared our arithmetic against a device model over random full-range `u16`
draws and concluded "the device reads packed words as unsigned" (signed being
54% wrong, unsigned within 1 LSB). That conclusion was an artefact of testing
outside the real value range.

The decompilation shows the interpolation operand is `(int)(short)` on both
halves of each 32-bit word — genuinely **signed** int16 SMUAD. But the values it
operates on are the unpacker's output, which is `(field11 << 4) | 0xF`, or `0`
when the field is zero. That range is **[0x000F, 0x7FFF]** — never above 0x7FFF,
so never negative as int16. Signed and unsigned coincide on every value the
device actually interpolates, and the question is moot rather than settled
either way.

The one word that could set bit 15 is the per-stage flag (below), and the audio
path masks it with `& 0x7fffffff` *before* using that word as the SMUAD operand.
So even that never reaches the multiply as a negative.

`dev/lerp_parity.py` should be re-run restricted to `[0x000F, 0x7FFF]` if this
is revisited; its current sweep tests inputs the device never sees.

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
