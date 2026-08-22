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
| best | 0.638 (`s1-14-ach-x`) |
| median | ~1.55 |
| worst | 3.483 (`s1-05-bude-tense-u`) |

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

## 6. Open

- Only a single static response is fitted per mouth. Nothing yet assembles four
  mouths into the four corners of one body, which is the actual product shape
  and is exactly how the factory vowel filters are built.
- The F1 undershoot and whether a perceptually flatter weight fixes it.
- The Transform 2 corner pairing from section 4.
- No listening yet. Per the standing rule, a build is not acceptance.
