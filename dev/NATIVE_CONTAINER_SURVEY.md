# Native container survey — the 7-section, 8-corner lineage

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

Session 2026-08-22. This is the survey that `native/CLAUDE.md` and the
workstation brief named as the gate on fitting this lineage. It is now measured,
from two independent directions that agree: the 289 decoded bodies, and the
per-sample arithmetic of the device firmware.

Datum throughout is 39,062.5 Hz. Nothing here is pooled with the 44,100 Hz P2K
bodies.

Scripts: `dev/native_container_survey.py`, `dev/decode_native_presets.py`,
`dev/family_recipe.py`, `dev/section_census.py`.

## Headline

**The machine is seven poles and six zeros, not seven of each,** and **the
native container does not use the P2K lattice.** Both results are load-bearing
and both contradict assumptions the project has been carrying.

## 1. Section 7 is pole-only. This is a law, not a habit.

Across the 289 bodies, every section-7 row that is not wholly identity has zero
words of exactly `DFFF FFFF` — the degenerate zero. 100.0%, one distinct value,
no exceptions.

Independently, in the device firmware, the audio callback at `0x080375FC` runs
its zero-pair body for stages 0..5 only (`cmp r7, #5 / bls`) and then falls
through to a seventh core that computes the resonator output and passes it
straight on — no `w1`/`w2` recursion, no zero pair. The runtime coefficient
struct even allocates stage-7 zero slots at `+0x1C4..+0x1E3`, and the engine
**never reads them**.

So: the authors never wrote a stage-7 zero, and the machine would have ignored
one if they had. Two lines of evidence, different media, same law.

**Consequence.** The cross-boundary bell documented in `DVTD_VOWEL_FIT.md` — a
zero in row N pairing with the pole in row N+1 — is not a stylistic choice. It
is forced. With only six zero slots for seven poles, and the last slot empty,
offsetting the pairing is the only way to put a zero against the seventh pole.
Any topology we author must place its zeros in rows 1..6 and leave row 7 a bare
pole.

Our current DVTD fit violates this: it puts a bell zero in row 7. It must be
rebuilt as lowpass + 6 bells with the pairing offset by one row.

## 2. There is no lattice in this lineage

The P2K container writes magnitudes on a 272-rung exponent-indexed lattice,
`(byte << 8) | low[byte >> 4]`, ~one byte per axis. That is an established P2K
finding and the app snaps drags to it.

Measured over 13,002 non-identity native rows:

| word slot | on the P2K lattice | distinct values written |
|---|---|---|
| 0 zero magnitude | **0.18%** | 3229 |
| 1 zero r² | 2.23% | 401 |
| 2 pole magnitude | **0.28%** | 3666 |
| 3 pole r² | 2.68% | 398 |
| 4 scale | 0.00% | 170 |

The native lineage writes essentially arbitrary u16. The handful of on-lattice
hits are coincidence at that rate.

The firmware explains the number 3229 rather than 65536: each stored corner is
an **11-bit field**, expanded at load to `(field11 << 4) | 0xF`, giving a range
of `[0x000F, 0x7FFF]`. So the native authoring resolution is 11 bits per
parameter per corner — finer than P2K's byte-per-axis lattice, and on a
different grid entirely.

**Do not snap native geometry to the P2K lattice.** It would be the wrong
quantiser for this machine.

## 3. Radius ceilings differ from P2K

| | native measured | P2K established |
|---|---|---|
| pole radius max | **1.000000000** | 0.999786473 |
| pole radius p99.9 | 0.999993999 | — |
| pole radius median | 0.974243 | — |
| zero radius max | 0.999995999 | 1.0 permitted |
| zero radius median | 0.991639 | — |

Native poles reach the unit circle. The firmware keeps them there safely: the
level-dependent radius law at `0x080378D2` computes
`r' = r + (1-r)·r·q` with `q ∈ [0,1)`, and since `r ≤ 1` this gives
`r' ≤ r(2-r) ≤ 1` — poles can touch the circle but never leave it. The
resonator state is separately hard-clamped every sample.

## 4. A third of sections carry no zero at all

Root kinds over the 13,002 live rows:

| | conjugate | degenerate | real-axis |
|---|---|---|---|
| pole | 95.8% | 3.4% | 0.8% |
| zero | 65.3% | **33.3%** | 1.4% |

Real-axis pairs exist in both lanes and must not be flattened. The large
degenerate-zero share is the same phenomenon as section 7 generalised: many
sections are pure resonators.

## 5. Scale word

170 distinct values; `0xDFED` accounts for 42–44% of every section index.
Decoded `4·d` runs 0.0889 to 4.0000, i.e. **−21.03 dB to +12.04 dB**.

Note the firmware does **not** consume a per-stage scale word. In the runtime
struct the equivalent is a **single bit** — bit 15 of the corner-7 halfword of
each block's first zero-angle word — masked off with `bic` and used as a
DC-normalise flag: set means the stage gain becomes `1 - 2·r·cosθ + r²`, clear
means unity. The 5-word-per-stage file format does not survive into the runtime.
Whether our 5th word maps onto that flag is **open** and matters for export.

## 6. Interpolation is confirmed packed-domain

The established law — interpolate the packed words, then decode — is confirmed
in the instruction stream. The trilinear tree at `0x080378DC` runs `SMUAD` over
Q15 axis weights against int16 corner values with a `>>15` renormalise per axis,
and the minifloat decode is applied to the *interpolated* word afterwards. Axis
weights satisfy `w_lo + w_hi = -32768` exactly. Corner index is
`bit0 | bit1<<1 | bit2<<2` over the three axes.

This is the one prior assumption the firmware confirms rather than corrects.

## 7. Runtime section form

Each section is a **coupled-form (Gordon–Smith) resonator with a cot(θ) output
tap, followed by a Direct-Form-I zero pair** — not DF1, not DF2, not transposed:

```
u' = clamp(g*x + u*(r cosθp) - v*(r sinθp), ±L)
v' = clamp(      u*(r sinθp) + v*(r cosθp), ±L)
w  = u' + cot(θp)*v'
y  = w - 2*rz*cos(θz)*w1 + rz²*w2
```

The `cot(θp)` tap exactly cancels the zero the coupled form would otherwise
introduce, so the section is algebraically the same biquad we already model —
verified numerically to 8e-15. Signal path is **float32 throughout**, no
requantisation between stages, no headroom shift. Our response comparator is
therefore correct as-is.

Two places the device is *not* our model, both audible and both currently
unmodelled:
- **cos/sin are ~0.2%-accurate firmware polynomials**, not libm. Bit-accurate
  audition needs them reproduced.
- **Output stage**: a 1.5-LSB round-toward-zero quantiser, then a parabolic
  C¹ soft clip saturating at ±2³¹.

## 8. The datum is 39,062.5 Hz, decoded from the clock tree

An earlier pass on the firmware read a `12288000` literal and inferred 48 kHz.
That was wrong, and the correction is decisive: **the clock tree cannot produce
48 kHz at all.**

The 12.288 MHz constant is the HAL's external-clock value, returned by the SAI
clock getter only when `RCC_DCKCFGR[23:20] == 0x300000`. The firmware programs
those bits to zero, so that branch is never taken. It is a dead constant.

The live chain, read instruction by instruction:

```
HSE                        8,000,000 Hz
PLLI2S    M=4  N=200  R=5  ->  VCO 400 MHz, R output 80.000 MHz
SAI1 Block A, master TX, I2S 32-bit x 2 slots = 64-bit frame
AudioFrequency == 0 (MCKDIV mode), literal divider written to CR1[23:20]
Fs = SAI_CK / (MCKDIV * 512)
```

| MCKDIV | MCLK | Fs |
|---|---|---|
| **4** | 10.000000 MHz | **39,062.5000 Hz — exact** |
| 3 | 13.333333 MHz | 52,083.33 Hz — used by nothing |

`80e6 / 39062.5 = 2048 = 512 x 4`, an integer. `80e6 / 48000 = 1666.67`, which
would need MCKDIV = 3.255 and is not representable. Neither VCO — 360 MHz nor
400 MHz — has any integer divisor yielding 12.288, 24.576 or 73.728 MHz, so a
48 kHz design is arithmetically unreachable from this clock tree. Every number
in the chain is round: 8 -> 400 -> 80 -> 10.000 MHz MCLK = 256 x 39,062.5.

**This independently confirms the hard invariant.** The Morpheus datum was
already established from the decoded corpus; it is now also read out of the
hardware clock configuration. The eurorack unit clocks the same rate the 1993
machine did.

Why it matters beyond bookkeeping: the angle encoding is a pure fraction of
Nyquist (`θ = ((m|0x800) << e) · π/2^27`, max 3.140826 ≈ π). The cube data
carries **no absolute Hz**. Every formant therefore scales linearly with
whatever rate the hardware runs, so decoding this corpus at 44,100 Hz would
shift every root by 1.2288x — about +3.57 semitones — and produce geometry that
is wrong everywhere while looking plausible. Use `f_Hz = θ · 39062.5 / (2π)`.

One caveat kept honest: the handle field read as `Init.Mckdiv` holds 3, while
the app also writes `0x400000` to `handle+0x1C`, which is MCKDIV = 4 pre-shifted
into CR1[23:20]. This driver stores raw register bit patterns rather than HAL
enums elsewhere too, so its struct is not the vanilla ST layout. The arithmetic
breaks the tie: only MCKDIV = 4 yields a rate anything uses.

## 9. Decoded corpus

`dev/decode_native_presets.py` writes `ref/morpheus_decoded/<family>/<body>.json`
— one file per filter, geometry decoded at 39,062.5 Hz, every corner and section
separated, with raw words, hex, pole/zero kind and coordinates, scale in dB,
per-section span, and a 256-point response per corner. Plus `index.json`.

289 filters: COMPLEX 81, STANDARD 26, DIPTHONGS 23, FLANGERS 21, EQUALIZATION 6,
and 132 not named in the UltraProteus manual (numbers above F156 — the Morpheus
corpus is larger than the UltraProteus filter list).

## 9a. Square vs cube is NOT in the body bytes

The manual says `.4` means square rather than cube — four corners, no Transform
2 axis. The obvious expectation is that a square body duplicates its corners
along the dead axis. **It does not.**

Testing all three corner-index bits against the 153 manual-labelled bodies
(`dev/axis_probe.py`); the firmware gives corner index as
`bit0 | bit1<<1 | bit2<<2`, so a flat axis would show as `corner[c] ==
corner[c ^ bit]`:

| axis bit | exact equality on square | on cube |
|---|---|---|
| bit 0 (stride 1) | **0 / 58** | 0 / 95 |
| bit 1 (stride 2) | **0 / 58** | 0 / 95 |
| bit 2 (stride 4) | **0 / 58** | 0 / 95 |

Nor is it near-equality. Taking each body's flattest axis by mean absolute word
difference, the median is 1727 for square bodies and 1624 for cube — the same
number. And which axis is flattest does not separate the labels either (square
18/28/12 across the three bits, cube 20/35/40).

**All eight corners carry distinct data in square and cube bodies alike.** The
`.4` property lives in the instrument's filter table, not in the 560 bytes.

Consequence for the decoded corpus: `geometry` can only be set from the manual
name suffix, and a body with no manual entry must be recorded as `"unknown"` —
never defaulted to `"cube"`. `dev/decode_native_presets.py` now does this and
carries a `geometry_source` field. The counts are 58 square, 95 cube, 136
unknown; the 132 unlisted bodies plus 4 complex-family bodies whose numbers fall
outside the manual's list are all unknown.

This also means the Transform 2 axis mapping remains open. Knowing which bit is
T2 would require either a body whose T2 plane happens to be flat, or evidence
from the instrument side.

## 10. What this unblocks, and what it does not

Unblocked: the container's write laws are now measured, so authoring in this
lineage is no longer refused for lack of evidence. The correct quantiser is
11-bit-per-corner fields, not the P2K lattice; the correct topology is 7 poles
and 6 zeros with row 7 bare.

Still open:
- The fitter core is six-stage. Fitting this lineage needs a seven-stage path
  that honours the row-7 zero law.
- Whether our 5th word corresponds to the runtime's single DC-normalise bit.
- Which physical axis (Morph / Q / Z) maps to which corner bit — the firmware
  reads three ADC sums into corner bits 0,1,2 but the labels need the UI side.
  Section 9a shows the bodies themselves cannot settle it.
- 32 bytes at struct `+0x1C4` are unpacked from the file and never read.

## 11. Evidence quality note

Two prior repository scripts should not be cited on this subject.
`dev/exact_dsp_stage_emulator.py` (in trench-authoring) indexes the firmware
image with a negative Python offset and therefore decodes unrelated bytes, and
its own comment marks the numerator as a guess.
`dev/verify_exact_biquad_law.py` never opens the firmware at all. Neither
reflects the instruction stream.
