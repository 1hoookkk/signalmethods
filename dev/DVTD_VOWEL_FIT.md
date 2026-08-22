# Vowel fitting with a prescribed cascade topology

Session 2026-08-22. Two independent results: a working vowel-fit path, and a
correction to what the `.4` filter suffix means.

## 1. The topology

Directed shape: **one lowpass section followed by six parametric bells.**

This maps onto the packed word law exactly, with no approximation:

| Section | Zero pair | Pole pair | Words 0..3 |
|---|---|---|---|
| 1 | fixed at Nyquist, double | free `(f, r)` | `FFFF 0000` + pole words |
| 2..7 | free `(f, r_zero)` | free `(f, r_pole)`, same `f` | all four free |

The lowpass numerator is `(1 + z^-1)^2`, i.e. `p_z = 2, q_z = 1`. Running that
back through the container law `p = 4*d_mag + d_rsq - 2`, `q = 1 - d_rsq` gives
`d_mag = 1.0`, `d_rsq = 0.0`, which encode to the words `FFFF` and `0000`
exactly. Both are legal container words. Note they are *not* members of the
272-rung authoring lattice, so the P2K fitter's search can never reach them —
this shape has to be authored, not searched for.

A bell is a pole and a zero at the same centre frequency with different radii.
Boost when `r_zero < r_pole`, cut when `r_zero > r_pole`; the peak gain near the
centre is approximately `(1 - r_zero) / (1 - r_pole)`.

## 2. Result

All 44 DVTD mouths (2 subjects x 22 phonemes) fit. Error is ERB-weighted,
mean-removed, over 100 Hz - 8 kHz, scored by the C++ core through the real
packed words:

| | rms dB |
|---|---|
| best | 0.585 (`s2-06-laehmung-tense-ae`) |
| median | 1.255 |
| worst | 2.332 (`s1-05-bude-tense-u`) |

Those are the figures after the cut-seeding change in section 3c; the original
boost-only seeding gave 0.638 / 1.579 / 3.483.

**Quantisation is free.** On every one of the 44 mouths the packed error equals
the continuous error to three decimals. The container's word resolution is not
the limiting factor for this topology — the section budget is. Integer polish on
the packed words afterwards buys another 0.00-0.32 dB.

Where the error lives: the first formant peak is consistently a little short.
The core's ERB weight rises with frequency (`w ∝ f / erb(f)`, ~2.8 at 100 Hz vs
~8.9 at 5 kHz), so the loss under-weights F1 relative to how the ear judges a
vowel. That is a property of the established loss, not of the topology, and it
is the obvious next thing to test.

Above 8 kHz the fit is unconstrained and dives, because the lowpass zeros sit at
Nyquist. Harmless as authored, but it must not be read as a fitted feature.

## 3. What the UltraProteus manual says about this exact shape

`E-mu UltraProteus Operation Manual`, 1994, 296 pages. Filter descriptions start
on printed page 178 (PDF page index 187). The DIPTHONGS family header reads:

> Implemented with parametric equalizer subsections, the resonances do not have
> the traditional overall lowpass effect that a true vocal resonance would have.
> Instead, they are placed at the same frequency as the resonances would be
> found in a true vowel, but the response at high frequencies is essentially
> flat...

and `F021 AEParLPVow`:

> This paravowel has a low pass filter to provide additional roll off, when
> desired. Otherwise, it behaves as the other paravowels.

So the directed shape — parametric bells, plus a lowpass for the roll-off the
bells do not provide — is E-mu's own documented construction for that filter.
The manual also publishes the paravowel resonance frequencies: A at 800, 1150,
2800, 3500, 4950 Hz; E at 400, 1600, 2700, 3300, 4900; O at 450, 800, 2830,
3500, 4950; U at 325, 700, 2530, 3500, 4950. Five bells, leaving one bell and
the lowpass spare in a seven-section cascade.

## 3a. The zeros are being wasted

Across all 44 fits, 238 bell sections have both roots conjugate. Of those:

- **218 (92%) act as a boost** — the zero sits further from the unit circle than
  its pole, so it only shapes the skirt of a resonance.
- **20 (8%) act as a cut.**

And on every single mouth, the largest signed error is the fit sitting *above*
the measurement: `s1-05-bude-tense-u` by 13.3 dB at 4807 Hz,
`s1-16-bett-lax-ae` by 8.9 dB at 3968 Hz, and so on down. Those are
antiresonances in the measured tract that the fit never cut.

The first reading was that the zero freedom is simply being wasted. The family
census in section 7 says something more interesting: **E-mu's own DIPTHONGS
filters use zeros in exactly the same proportion — 92% boost, 8% cut.** Our fit
independently landed on the factory's vowel practice.

The manual explains why, and says it is deliberate:

> ...the resonances do not have the traditional overall lowpass effect that a
> true vocal resonance would have... the response at high frequencies is
> essentially flat to allow high frequencies of the samples to get through.

A paravowel is intentionally *not* a vocal tract. It has no tract rolloff and no
deep antiresonances, so that the sample's own top end survives the filter.

Our target is the opposite: a real measured tract, nulls and all. So the fit is
behaving like a paravowel while chasing a real vowel, which is precisely why the
worst error on every mouth is an uncut antiresonance. The fix is a seeding
change — start some bells as cuts on the residual's minima — not a change of
topology or resolution. It matches the Kerkhoff/Boves rule already cited in
`native/CLAUDE.md`: poles track formants, zeros shape the global spectrum.

## 3c. Reseeding with cuts on the residual minima

The fix in 3a was implemented and run over all 44 mouths. Rather than seeding
every bell as a boost, the seeder now picks both peaks and valleys from the
target and sweeps the allocation — 0, 1, 2 or 3 of the six bells seeded as cuts,
with the pole set inside the zero — keeping whichever allocation wins.

| | boost-only | cut-seeded | change |
|---|---|---|---|
| rms best | 0.638 | 0.585 | -8% |
| rms median | 1.579 | 1.255 | **-21%** |
| rms mean | 1.658 | 1.307 | -21% |
| rms worst | 3.483 | 2.332 | **-33%** |
| largest single-point error, worst mouth | 13.30 | 7.85 | **-41%** |
| largest single-point error, median mouth | 5.27 | 4.10 | -22% |

38 of 44 improved, 6 got slightly worse. The 6 are an artefact of the sweep
choosing its allocation on the continuous cost while the table reports the
post-polish figure; they are within 0.06 dB and not worth a second sweep.

Allocation actually chosen, out of six bells:

| cut bells | mouths |
|---|---|
| 0 | 8 |
| 1 | 6 |
| 2 | **22** |
| 3 | 8 |

**Two antiresonances is the mode for a real vocal tract.** And eight mouths
still do best with no cuts at all, which is consistent with E-mu's all-boost
paravowel construction being the right answer for some vowels rather than a
shortcut.

## 3b. E-mu pairs a zero with the *next* row's pole

Dumping `F022 AEParaVowel` corner 0 at the Morpheus datum:

| row | pole | zero |
|---|---|---|
| 1 | 1009 Hz r .98871 | degenerate |
| 2 | degenerate | 16148 Hz r .99561 |
| 3 | 16148 Hz r .99561 | 3952 Hz r .68563 |
| 4 | 4290 Hz r .97730 | 2972 Hz r .86624 |
| 5 | 3057 Hz r .99066 | 2465 Hz r .99362 |
| 6 | 2449 Hz r .99872 | 688 Hz r .99634 |
| 7 | 688 Hz r .99884 | degenerate |

Row 2's zero words are `FD8E 91F4`; row 3's pole words are `FD8E 91F4`. Byte
identical — they cancel exactly in the cascade product. Row 6's zero is 688 Hz
at r .99634 and row 7's pole is 688 Hz at r .99884: same frequency, different
radius, which is a +10 dB bell **formed across the row boundary**. Row 5's zero
(2465, .99362) against row 6's pole (2449, .99872) is another, at +14 dB.

So a paravowel bell is not a pole and zero sharing a row, the way our fit builds
them. It is the zero of row N against the pole of row N+1. Because the cascade
is a product, both arrangements give the same response at a corner — but the
words interpolate per row, so the row assignment changes the entire morph
interior. This is an authoring decision with no effect at the corners and a
large effect between them, which is worth knowing before any four-corner vowel
body is assembled.

This confirms the earlier "cross-boundary pole/zero bells" observation and puts
byte-level evidence under it.

## 4. Correction: `.4` is about corners, not sections

Printed page 178, first paragraph:

> A suffix of '4' or '.4' indicates filter is square, not cube and does not
> contain a Transform 2 axis.

`.4` means **square rather than cube — four corners instead of eight, no
Transform 2 axis.** It says nothing about section count.

Confirmed against the decoded bodies. 153 of the manual's named filters match a
decoded body; live sections counted per body (a section is live if it differs
from the identity row `DFFF FFFF DFFF FFFF DFFF` in any corner):

| | filters | 7 live sections | fewer |
|---|---|---|---|
| `.4` square | 58 | 47 | 11 |
| cube | 95 | 83 | 12 |

**`.4` filters carry seven sections, the same as cubes.** The minority with
fewer live sections is a per-filter authoring choice and occurs at nearly the
same rate in both groups, so it does not track the suffix.

Byte-difference is the weaker test, so it was repeated by measuring each
section's own response range at the Morpheus datum through the packed law, at a
0.5 dB threshold (`scratchpad/section_census.py`). The two tests agree: 47 of 58
`.4` and 83 of 95 cubes have all seven sections contributing, and only 3 bodies
of 153 contain a row that differs from identity while doing nothing.

The seventh section is not a pad. Its own response range, taken as the maximum
over the eight corners, has a median of 60.2 dB on `.4` filters and 66.0 dB on
cubes; only 6 of 58 `.4` filters have a seventh below 0.5 dB.

Genuine six-section filters do exist, and one is directly relevant:
**`F021 AEParLPVow` has an exactly-identity seventh row** (`DFFF FFFF DFFF FFFF
DFFF`). Its six live rows are one broad shaping section that cuts (pole 439 Hz
r .729 against zero 616 Hz r .942, net −13.4 dB), four boosting bells at
+25 to +30 dB, and one bare pole. That is the manual's "paravowel with a low
pass filter" and it is close to the shape we fitted.

Do not confuse this with the six-section container. Six sections x four corners
x five words = 240 bytes is the later P2K family. The UltraProteus/Morpheus
lineage is seven sections x eight corners = 560 bytes, and its `.4` filters live
in that same 560-byte container.

Not established: which corner index pairing carries the Transform 2 axis. The
naive test (corner `c` equals corner `c+4`) fails on all 58 `.4` bodies, so the
corner ordering is something else. Open.

## 5. Files

Written this session:

| Path | What |
|---|---|
| `native/research/bindings.cpp` | Research bridge, extended. Now exposes `cascade_db`, `section_db`, `geometry`, `lattice_words_from_root`, `nearest_gain_word`, `erb_grid_hz`, `erb_grid_weight`, plus the datum constants. Every score in this document came through it. |
| `dev/fit_dvtd_topology.py` | The fit. `--mouth <name>` for one, `--all --out <json>` for the set. Requires Python 3.13 (`py -3.13`) — the extension module is built `cp313`, and the default `python` on this box is 3.10. |

Read, not modified:

| Path | What |
|---|---|
| `C:\Users\hooki\trench-authoring\recipes\vocal\dvtd\subject-{1,2}\*\*-vvtf-measured.txt` | The 44 measured vocal-tract transfer functions. Three columns, `freq_Hz magnitude phase_rad`, 20807 rows, 0 to 19999.9 Hz, linear magnitude. |
| `C:\Users\hooki\trench-authoring\recipes\tables\dvtd_formants.json` | Derived peak table for the same 44, from the older Rust path. Not used by this fit; the fit picks its own peaks. |
| `C:\Users\hooki\trench-authoring\ref\morpheus\bodies\*.body` | 290 decoded bodies, 560 bytes each, used for the section count in section 4. |
| `C:\Users\hooki\trench-authoring\dev\shots\p2k\dvtd_s1_*.bin`, `dvtd_s2_*.bin` | Ten 240-byte outputs from the earlier Rust attempt at this. Prior art, not yet compared against these results. |

Reference PDF: the UltraProteus manual is at `manuals.plus`, hash
`af4931b18363da4d60ad9d50856379fe840b2d58c61e6c743c2f3d32ae9001f4`. Text extracts
cleanly with `pypdf`; the fetch tool's own summariser cannot read it.

## 7. What each documented family is actually made of

The manual groups the 153 filters into five families and states each one's
construction in a header paragraph. Measuring the decoded bodies per family
(all 8 corners, conjugate roots only; a zero is "paired" when some pole sits
within a 1.35x frequency ratio of it, regardless of row) gives the recipe:

| family | F range | poles/corner | zeros/corner | median pole r | median zero r | paired zeros acting as cut |
|---|---|---|---|---|---|---|
| FLANGERS | 000-020 | 5.10 | 4.27 | 0.9683 | 0.9926 | **69%** |
| DIPTHONGS | 021-043 | 5.03 | 3.15 | 0.9793 | 0.9439 | 8% |
| STANDARD | 044-069 | 4.74 | 1.96 | **0.7071** | 0.9939 | 24% |
| EQUALIZATION | 070-075 | 4.77 | 3.77 | 0.9478 | 0.9922 | 28% |
| COMPLEX | 076-156 | 5.20 | 3.71 | 0.9813 | 0.9907 | 30% |

Read against the manual's own words for each family:

**FLANGERS** — "a series of notches with various depths, widths and
frequencies." Confirmed and inverted relative to vowels: 69% of paired zeros
cut, and 58.5% of all zeros sit at r > .99, i.e. hard on the unit circle where
they make deep narrow nulls. The poles are the *shallower* root here
(median .968). This family is zero-led.

**DIPTHONGS** — "parametric equalizer subsections." Confirmed: the only family
where zeros are pulled well inside the circle (median r .944, only 18.5% above
.99) and the only one that is overwhelmingly boost. Pole-led, zeros shaping
shoulders.

**STANDARD** — "variations on traditional 2 and 4-pole filter models." The
median pole radius is 0.7071, which is 1/sqrt(2) — the Butterworth pole radius.
Fewest zeros of any family (1.96/corner), sitting at r .994, i.e. parked at DC
or Nyquist to make the lowpass/highpass rolloff. Textbook, and it shows.

**EQUALIZATION** — "variations of traditional parametric EQ filters." Matched
pole/zero pairs with gain of *either* sign (28% cut / 72% boost) and zeros
distributed across the whole radius range. This is the family that uses the
pole-and-zero-at-one-frequency device most symmetrically.

**COMPLEX** — "Many of these filters have never existed for musical applications
before!" Statistically the union of the others; no single recipe. Treat
individually.

So the family-to-recipe map, stated as authoring rules:

- notch/sweep families (flanger, phaser): zeros on the circle, poles inside,
  frequencies in a series; sweep by moving the zero set together.
- vowel families: poles on the formants, zeros inside as shoulders, no tract
  rolloff unless a lowpass section is explicitly added.
- traditional filters: poles at r ~ .707 in a Butterworth arrangement, zeros
  parked at DC or Nyquist.
- EQ families: one pole and one zero per band at a shared frequency, sign of the
  gain set by which root is nearer the circle.

## 6. Open

- Only a single static response is fitted per mouth. Nothing yet assembles four
  mouths into the four corners of one body, which is the actual product shape
  and is exactly how the factory vowel filters are built.
- The F1 undershoot and whether a perceptually flatter weight fixes it.
- The Transform 2 corner pairing from section 4.
- No listening yet. Per the standing rule, a build is not acceptance.

## 8. Superseded by the container survey

`dev/NATIVE_CONTAINER_SURVEY.md`, same session, establishes that **section 7 of
this machine can never hold a zero** — 100% of the 289 decoded bodies, and the
device firmware does not even read the stage-7 zero slots.

The fit in this document violates that law: it places a bell zero in row 7. The
response is still correct as a 7-section cascade, but it is not a legal body for
this container and must be rebuilt as one lowpass plus six bells with the
pole/zero pairing offset by one row, leaving row 7 a bare pole.

The cross-boundary structure described in section 3b is therefore not an E-mu
stylistic preference. It is the only legal way to put a zero against the
seventh pole.
