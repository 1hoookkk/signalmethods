# AUTHORING SPEC

Sources on disk: `ref/patents/US5170369.pdf`, `ref/patents/rossum_armadillo_coefficient_encoding.pdf`,
`ref/patents/US5943427_Massie_et_al.pdf`, `ref/inputs/making_digital_filters_sound_analog.pdf`,
`ref/martens/Martens_1985_ICMC_PALETTE_*.pdf`, `ref/martens/Freed_Martens_1986_*.pdf`,
`ref/martens/Sandell_Martens_1992_*.pdf`, `ref/morpheus_manual_zplane_descriptions.txt`,
`ref/emu/`, EOS technical documents, Emulator IV manual.

Corpus: 50 `ref/presets/P2k_*.bin`. `talking_hedz.bin` is a byte-identical
duplicate of `P2k_013` — exclude it or it inflates counts by 24 rows.

## What a body is

Two constellations and one pairing.

An endpoint is **12 poles and 12 zeros**. It has no sections. Reordering the six
slots identically at both ends changes the response by **1.78e-14 dB** at M0,
M50 and M100 (20 random reorderings).

A slot stores **which root at M0 becomes which root at M100**. Reorder the slots
differently at each end: endpoints unchanged (**1.42e-14 dB**), middle of the
morph moves **95.47 dB**.

The pairing is in neither endpoint. It is the authored part.

## Word units

| slot | stores | unit |
|---|---|---|
| 0, 2 | `e^(-k1)` = \|1-p\|²/4 | 1 octave = 1.386 |
| 1, 3 | `e^(-k2)` = 1 - R² | 8.68 dB of peak height = 1 |
| 4 | SCALE | broadband level |

    rho ~ 8.68*k2 + 6.02 dB
    Omega ~ -k1/1.386 + log2(Fs/20pi)

k1 rises as pitch falls. `e^(-k1)` is the squared distance from pole to DC, so
radius drops out of the pitch axis. Interval in semitones is exactly invariant
to the assumed clock; absolute Hz is not.

US5170369, claims 4-6 verbatim (col. 12):

> "4. A digital filter as in claim 1 including first storage means and wherein
> said second coefficient is stored in a logarithmically compressed format in
> said first storage means.
> 5. A digital filter as in claim 4 including second storage means and including
> a third coefficient stored in said second storage means in a logarithmically
> compressed format representative of a difference between said first and second
> coefficients.
> 6. A digital filter as in claim 5 including means for interpolating between
> said first and second coefficients."

Claim 6 is the whole of the interpolation claim. It states **no axis order**, and
the machine it claims has one interpolating variable `x`. Body, col. 6:

> "The coefficients are interpolated according to the formula C(x)=Ca+x(Cb-Ca),
> where x varies from zero to unity."

## Verified 2026-08-12

- Repo decode vs Rossum `b1`/`b2`: max \|diff\| **0.000e+00** over 2448 pairs.
- Pitch ruler vs exact pole angle: median 0.02 oct; 250 Hz-4 kHz 0.001-0.013;
  above 11 kHz 0.302 (small-angle approximation fails).
- dB ruler vs `20log10(1/(1-R))`: median 0.04 dB, p95 0.72 dB.
- Encoder floor k = 18.02 = `-ln(2^-26)`.
- 172 of 1200 rows real-rooted. `geometry_from_words` handles them,
  `roots_from_words` returns `None`, so `StageRoots` drops that geometry.

## Cascade behaviour

- Pole lands where placed: median offset to the cascade peak it makes **0.08 st**,
  p90 2.1 st.
- **32 of 191 poles produce no peak in the total at all.**
- Control authority: move a pole +1 st, the audible peak moves **+0.53 st**
  (S6 only +0.27). Within 0.5-1.5 st in 71% of moves.

Any editor writing a pole frequency directly inherits that 2:1 gearing. Solve
backwards from the wanted peak.

## Corpus: pole-zero interval within a slot

1028 conjugate pairs of 1200 rows.

| slot | p10 | median | p90 | \|st\|>18 |
|---|---|---|---|---|
| S1 | -75.8 | -5.2 | +90.7 | 68.2% |
| S2 | -32.4 | +7.9 | +57.4 | 55.2% |
| S3 | -19.8 | +4.5 | +65.8 | 39.7% |
| S4 | -37.4 | +1.0 | +56.2 | 35.8% |
| S5 | -24.9 | +8.9 | +67.0 | 46.4% |
| S6 | -27.9 | +24.6 | +91.3 | 66.1% |
| all | -60.4 | +4.4 | +72.4 | 51.5% |

Bands: under 3 st 20.6% · 3-18 st 27.9% · over 18 st **51.5%**.

Both roots sit in a slot by bookkeeping, so this interval is not a designed
quantity. S1/S6 lean most and S3/S4 least, but it is a tendency, not a law.

## Corpus: reuse

1200 rows, **1143 distinct**. Only 30 rows appear in more than one body, and
almost all are in the generic block `P2k_033`-`P2k_049` (a 4-pole HPF and a BPF
genuinely share sections). Among the 33 character filters there is essentially
no reuse. Only 1 row repeats across corners within a body.

**No shared stage library. Every body placed on its own.**

## Corpus: pairing

28 bodies with all 12 poles conjugate.

    mean k-space travel per root      authored   minimum
      poles                             3.00       2.18    1.38x
      zeros                             2.22       2.07    1.08x

Zeros are effectively nearest-neighbour matched. Poles are deliberately 38%
longer than necessary.

Interior of the morph (M25/M50/M75 averaged):

| assignment | roughness | max dB | peaks |
|---|---|---|---|
| factory | 0.124 | -10.3 | 5.1 |
| min-travel | 0.122 | -12.9 | 5.0 |
| random | 0.136 | -5.3 | 4.8 |

Random is rougher and **11 dB hotter** — correspondence matters. But factory is
not better than min-travel on any computable metric and is 2.6 dB hotter. The
extra pole travel costs headroom and buys nothing measurable, so it was chosen
by ear.

Rank preservation (i-th lowest pole at M0 -> i-th lowest at M100): **9 of 28**.
Minimum travel: 3 of 28. The nine are the vowel/vocal bodies (`ooh_to_eee`,
`eeh_to_aah`, `ubu_orator`, `dream_weava`, `acid_ravage`, `tooth_comb`,
`klang_kling`) — formants cannot cross, so that is a constraint on the source
material, not a design convention.

## Families

**Millennium / Meaty Gizmo** — corners 0 and 1 byte-identical, 160/240 bytes
shared. A copy with only the Q axis edited.

**Boland Bass / Lucifer's Q** — no identical corner, but every M0 pole is
identical in frequency and radius. At M100, slots 4 and 5 have their radii
**exactly swapped** (0.8293/0.8571 vs 0.8571/0.8293) and are retuned.
Same starting constellation, reassigned correspondence. This is the 95 dB
operation shipped as two named factory filters.

## Study targets

`recipes/STUDY_TARGETS.txt` — 33 bodies, `P2k_000` .. `P2k_032`, contiguous.
The whole character block. `P2k_033`+ is the generic LPF/HPF/BPF/EQ/PHA/FLG/VOW
block and is excluded. 25 were already referenced in `recipes/INTENT.md`.

`P2k_046 BlissBatz 6 PHA` is the "Bat Phaser species" of the Opium hero recipe.

## Morpheus is a different machine

| | P2K | Morpheus |
|---|---|---|
| sections | 6 | **7** |
| poles | 12 | **14** |
| corners | 4 (2 axes) | **8 (3 axes)** |
| filter names shared with P2K | — | **0 of 50** |

Both settled from E-mu's own manual, not from Symbolic Sound. The Morpheus
signal-flow figure, p.99, labels the chain:

> "In → **1 Low Pass Section** (Fc, Q) → **6 Parametric Equalizer Sections**
> (Fc, Bw, Gain ×6) → Out"

and the body text checks the arithmetic: "Right away you can see that we now
have 20 different parameters to control." 2 + 6×3 = 20. Seven sections, the
first a lowpass, is E-mu's own drawing.

Corner count, p.185:

> "A suffix of "4" or ".4" indicates filter is square, not cube and does not
> contain a Transform 2 axis."

Square = 4 frames = 2 axes; cube = 8 frames = 3 axes. E-mu calls a corner a
**frame** and an axis Morph / Freq. Tracking / Transform 2. The Symbolic Sound
"2^N sets of coefficients" line is no longer load-bearing and is not quoted here.

`NUM_STAGES` is a single const in `trench-core/src/cascade.rs`; 6 -> 7 is one
line. Corners 4 -> 8 is `PackedCorners` plus bilinear -> trilinear. The cost is
compatibility (240 -> 560 bytes, roster, plugin), not DSP.

## Timeline

    1991-10  ARMAdillo (WASPAA). Encoding + a graphical pole-analysis tool.
    1992-12  US5170369 issued. Interpolation architecture.
    1992     Rossum, Making Digital Filters Sound "Analog". ICMC pp. 30-33.
    1992     Sandell & Martens, Prototyping and Interpolation. ICMC pp. 34-37.
    1992-93  Morpheus cube production.
    1993     Morpheus ships.

The two 1992 papers are **consecutive** in the same proceedings.

Rossum's 1991 plotting tool, verbatim, checked against the page:

> "We have found a useful graphical analysis tool to be the plot of the poles
> translating the radii from R to R' and the angles θ to θ' (excluding all θ
> below π/1024 ≈ 20 Hz) such that:  R' = 20log₁₀ 1/(1-R)  and
> θ' = π(10+log₂ θ/π) / 10
> Such a plot maps the poles onto a nominally log/log semicircle, and even
> coefficient density indicates an even perceptual mapping."

**Corrected 2026-08-13.** This file previously wrote the angle as
`pi(10 + log2 Omega)/10`. The log₂ argument is **θ/π**, not Ω. The formula is
sample-rate free: ten octaves of normalised frequency from π/1024 to π map
linearly onto 0..π, and θ = π/1024 is exactly where θ' reaches 0. Our
`display_freq_min_hz = sample_rate/2048` is that exclusion and agrees.

Ω is a separate quantity, defined p.1: "the "musical octave number" Ω varying
from zero to ten which is logarithmic in resonant frequency based on the pole
angle θ and sample rate Fs", Ω ≈ log₂(θFs/40π). It is what makes k₁ linear in
octaves; it is not the plot axis. The two coincide only at Fs = 40 kHz.

What the plot is FOR: judging a coefficient **encoding**. It is introduced as
"The final proof of the encoding scheme lies in the effectiveness of the
implementation used with "real" filters", and figures 1 and 2 are the same
quantisation unencoded and encoded. It is not an instruction to space poles
evenly.

## Hardware

Parts list: **G-chip = "Sound Engine" (2.0)**, **H-chip = "Digital Filter" (1.5)**.
Four H-chips per Ultra, `CHPN0`/`CHPN1` addressing, `CSCDI`/`CSCDO` cascade
chain, only the last reaching the FPGA. Host bus `A0..A13` + `D0..D15` + `CSN`
`RD` `DTACKN` `RSTN` (16384 registers). Sample bus `BA1..BA14`, `BD0..BD31`
split across chip pairs. Eight strobes `STB0N..STB7N` per chip.

E-IV sheet 183 annotates **"H-CHIP ADDR SCRAMBLING"** with stuffing options on
`R188/R190/R192/R194` — host address lines are permuted per board build.

No H-chip programmer's guide exists publicly. `"Contrary Bandpass"`, `CSCDI`,
`CHPN0` return zero hits anywhere.

## Firmware/cube audio format (Rossum Morpheus eurorack)

Solved this session. Same for all three files (boot, firmware, cubes).

    8-bit unsigned WAV, 48 kHz
      1.000 s leader of 6 kHz (12000 short half-cycles = exactly 48000 samples)
      frame sync: run lengths 11 5 3 13   (out-of-band, identical in all three)
      negative half-cycles 3 or 7 samples; positive 5 or 9; never mixed
      negative-edge intervals 8/12/16 samples = 2T/3T/4T, T = 4
      MFM: an interval of k cells emits (k-1) zeros then a 1
      take every second channel bit; phase 1 is the clock track (all 0x55),
        phase 0 is data
      -> bytes

Yields boot 15718 B (ent 4.60, 36% zeros), firmware 128678 B (4.86, 32%),
cubes 96499 B (5.31, 26%). Byte framing confirmed independently: bit-level
autocorrelation peaks land on exact multiples of 8.

Cube record structure NOT settled. Bit-level periodicity is **44 bytes**
(corr 0.180) with a 332-byte super-period; `8*7*5 = 280` scores 0.003, the
noise floor. 96499/44 = 2193.2, not a multiple of 289.

`ref/p2k_variants/combined/cubes_raw_bytes.csv` is **not filter data**: its 55
distinct values are exactly the 55 eight-bit patterns with no two adjacent zero
bits — the RLL channel alphabet, one step before the MFM decode. Discard
anything derived from it.

## Word space

3825 bodies from all 1275 factory pairs at t = .25/.5/.75:

| pair distance | refused | rougher than factory p95 | peaks p50 |
|---|---|---|---|
| nearest 20% | 0.0% | 1.3% | 4 |
| 20-40% | 0.0% | 1.4% | 4 |
| 40-60% | 0.0% | 20.7% | 4 |
| 60-80% | 0.0% | 60.7% | 7 |
| farthest 20% | 0.0% | 60.0% | 7 |

Closed under interpolation — nothing refused. Coherence is local. Use kNN /
local neighbourhoods, not global PCA.

## Method (US5943427 col. 12, verbatim)

> "Table 1 gives the first notch frequencies of comb filter 702 for some
> representative elevation/azimuth combinations… Front: 9 kHz, Right: 10 kHz,
> Rear: 9.5 kHz, Left: 1.3 kHz, Above: 10 kHZ, Below: 6 kHZ"

> "The A values may be derived empirically by moving a real Sound emitter around
> a mannequin and measuring the notch depth in the frequency responses at each
> ear. A Series of A values is then calculated for a Series of elevations and
> azimuths to reproduce the measured notch depths."

1. Name the features that carry the perception. Two numbers, not a spectrum.
2. Write anchor values at named positions.
3. Measure the missing feature on the real source.
4. Solve the parameter that reproduces it. No residual is minimised.
5. Control axis determines the rest.

3D audio, filed 1995, same authors. NOT proof of the method used for the filter
ROMs.

## Martens

Works above the synthesis parameters, never inside them. PALETTE: "it should be
possible to choose synthesis algorithms strictly in terms of their perceptual
features without even looking at how an algorithm has been programmed."
Freed & Martens judged recordings of struck cookware. His documented Morpheus
contribution is F1 150-850 Hz, F2 500-2500 Hz and the schwa-collapse rule —
axis coordinates. **Nothing in Martens bears on slot assignment.** On a cube his
three axes fit directly.

Modules: `Parser` -> descriptors -> `Pscaler` -> parameters -> `Synth`;
`Parser` -> judgments -> `Funk` -> functions -> `Pscaler`. New engines "must
adopt the standardized data structure that Pscaler uses in communicating with
the Synth module."

Gestures, every screen: left = audition at pointer (unlimited replay),
right = commit point + drop marker, middle = finish + write file.

The three steps, verbatim: "1. **Sound Selection.** The user selects the range of
timbres of interest for a given synthesis algorithm. 2. **Unidimensional
Scaling.** The user generates a psychophysical scale for a single timbral
dimension taken from that range. 3. **Timbre Matching.** The user matches the
timbres resulting from other synthesis algorithms to those of the above
algorithm on the scaled timbral dimension."

The loop, verbatim: "In each iteration, the results of the last curve fitting
are used to predict how the synthesis parameter values should be spaced in the
next so as to separate the timbres by roughly equal perceptual distances at each
pitch."

That is the ARMAdillo plot's law one level up: Rossum checks whether a coefficient
ENCODING is evenly spaced in perception; Palette re-spaces a PARAMETER until it
is. A control the editor adds that is not already an ARMAdillo coordinate has to
earn its spacing this way, by ear.

("RESPACE" and "SEPARABLE" were labels this file invented. The paper's commands
are `pxv`, `pxvadj`, `pxvscal`, `pxvmch`, `pxvtrans`. Do not reuse the coinages.)

Sandell & Martens 1992: prototype = eigenvectors of pooled WITHIN-group SSCP
(what the set shares); interpolation = BETWEEN-group SSCP (what distinguishes).
Subtracting the prototype makes it the origin. Interpolation preserves the
prototype. Paths: shortest, or "piecewise linear, changing values on one
dimension at a time."

## Rossum on the runtime (ICMC 1992)

Re-read end to end 2026-08-13. Verbatim, §3 p.31:

> "Interestingly, our research has shown that the spectral behavior of filters is
> not tremendously important. While the difference between a 2nd order and 4th
> order rolloff is fairly audible, the precise relative placement of the poles
> seems to be not very critical."

> "Digital filters, however, operate at a fixed sample rate and as a result do
> not approach an asymptotic rolloff of 6dB/octave per pole at the high end of
> the audio band. Instead, due to the fact that the rolloff is akin to a cosine
> function in frequency, the rolloff of a pole levels out at high frequency.
> This, fortunately, is easily compensated using a tracking zero (see figure 2a
> and 2b)."

§4 p.32:

> "To band limit the coefficient changes,the coefficients must be updated at the
> sample rate. To update them at any lower rate will cause audio images of the
> update rate to appear in the signal path, which may be somewhat attenuated by
> filtering."

§5 pp.32-33, the nonlinearity:

> "If we add some headroom to the accumulator portion of the filter, and instead
> saturate only when delaying the signal prior to the multiplier inputs, we have
> caused the saturation to occur at a point equivalent to the input of the
> filter (see figure 3)."

> "Viewed in the latter way, it is obvious that the onset of distortion is
> equivalent to changing the coefficients of the filter. As mentioned above, the
> coefficients are themselves filtered, so the fact that the coefficients are
> varying depending on the instantaneous saturation of the signal will not result
> in hideous distortion."

Figure 3 as drawn: INPUT → Σ (marked "Extended Headroom") → OUTPUT tapped
directly off Σ; a `Saturate` block sits **between** Σ and the first `z⁻¹`; the
two delays feed b1 and b2 back into Σ. Output before the saturate, saturate only
into the delays. Per second-order section, no detector, no added state.

Shipped in EMAX II. 32-bit mantissa.

## Tools built this session

    tools/wordsheet/            drag poles/zeros on a lane board, frame lock,
                                blend toward any factory body (sorted by
                                word-space distance), real-engine audition
    tools/plate_endpoints.py    two-row endpoint plate for any body
    recipes/STUDY_TARGETS.txt   the 33
    dev/reference/study_plates/ 33 plates

## The move available now

720 pole permutations per body leave both endpoints bit-identical and change the
interior by up to 95 dB. 29 approved presets x 720 = 20880 filters reachable
with no new geometry, no fitting, no cube decode. Score the interior on the real
runtime; rank by distance from the shipped pairing; judge by ear.

## Withdrawn

- "S1/S6 are the frame, S2-S5 are formants." An endpoint has no section roles.
- The pole->zero interval within a slot as a designed quantity.
- "The refit put sections in the wrong roles." There was no fact to match.
- Opposing sections as a defect (authored 207.0 dB vs refit 211.3 dB lost to
  internal cancellation — not a discriminator).
- `C11 = A + DM + DQ + DX` as evidence-led. Fourth-corner prediction scores
  14.31 dB RMS vs 18.64 chance and a 3.20 floor; the plain parallelogram is
  worse than chance.
- Boland Bass / Lucifer's Q as "same plan, different geometry". It is the
  reverse: same geometry, different correspondence.

## Open

- 40 of 200 corners refused by the runtime. Real roots are not the cause: 88
  corners contain one.
- `StageRoots` cannot express a real-rooted pair; 172 rows of shipped geometry
  pass through as `None`.
- Cube record layout. 44-byte period found, does not divide by 289 or match
  8*7*5.
- Whether the 7th section is worth anything: testable today by setting
  `NUM_STAGES = 7` at 4 corners, before committing to a third axis.
