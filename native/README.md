# Trench native core boundary

This directory is the first native shipping slice. It is intentionally Qt-free:
the packed body, root geometry, interpolation, response, and analysis quantities
must be usable by the future Qt workstation, command-line verification, audio
runtime, and a narrow Python research binding without acquiring UI semantics.

The existing Rust/browser implementation remains the behavioral oracle until
native parity is demonstrated. Nothing here changes or removes it.

## Implemented

- C++20/Ninja/CMake Presets/vcpkg build with GoogleTest.
- Exact E-mu 16-bit word encode/decode law.
- Native `8 corners x 7 ordered sections x 5 words` body and separate legacy
  `4 x 6 x 5` container conversion.
- Encoded-word interpolation in `M`, then `Q`, then `Z`, without sorting or
  reassignment.
- Canonical section geometry: conjugate pairs, real-axis pairs, degenerate
  pairs, and one scale.
- Complete serial-cascade response.
- Marginal section contribution on an explicit frequency grid:
  `cascade_db(all) - cascade_db(all minus section)`.

Section scale is part of the transfer function. Section gains multiply in the
cascade and therefore add in dB. The contribution API evaluates the packed
scale; it is not a normalized pole/zero-shape score. Prefix/suffix products
reimplement the leave-one-out mechanics without division, so exact zeros do not
turn the implementation into an unsafe `full / section` shortcut.

## Verified corpus

The native tests normalize the six live body sources at their own datum:

- `ref/morpheus/bodies` (289)
- `recipes/hero` (12)
- `recipes/extrusions` (4)
- `ref/presets` (33)
- `ref/x3` (17)
- `ref/md_templates` (69)

Measured results:

- 424 bodies and 23,744 normalized section cells.
- 65,536/65,536 packed words survive `encode(decode(word))` exactly.
- 298 native and 109 legacy files preserve their original bytes; the 17 X3
  runtime blocks preserve their stated short topology while being padded only
  in the corpus adapter.
- 23,744/23,744 cells survive geometry decode/rebuild exactly.
- 1,220 cells contain at least one real-axis root pair.
- 6,343 cells are fully degenerate.
- The complete Talking Hedz cascade matches the existing Rust oracle at three
  morph positions and fourteen frequencies.

## Contribution census

The corpus command uses 768 logarithmic samples from 20 Hz to `0.49 * datum`.
Its 2026-08-20 result is:

| Maximum absolute leave-one-out delta | Cells | Share |
|---|---:|---:|
| `<= 0.4 dB` | 6,453 | 27.2% |
| `0.4 .. 1 dB` | 1,146 | 4.8% |
| `1 .. 3 dB` | 701 | 3.0% |
| `3 .. 6 dB` | 498 | 2.1% |
| `> 6 dB` | 14,946 | 62.9% |

The former interval label does not reproduce its old 65/35 split on the full
corpus. Of 12,018 cells that have a conjugate pole and zero with measurable
frequencies, 9,786 are within 24 semitones (81.4%) and 2,232 are farther apart
(18.6%). The remaining 11,726 cells cannot receive that interval label. Only 80
local-labelled cells and one cross-labelled cell contribute at most 0.4 dB on
this grid. The interval census is therefore descriptive geometry, not a proxy
for salience, token position, visibility, or FIT growth.

The `0.4 dB` value is a query threshold supplied for this measurement, not a
new invariant. A future UI may use the contribution curve at its current column
or a declared summary of that curve, but must display which quantity it uses.

Run the census after configuring and building:

```powershell
native/configure.ps1
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
out/build/windows-msvc-debug/native/trench-core/trench_corpus_contribution.exe <repo-root>
```

## Fitter boundary

ARX is analysis evidence, not a command to fit a 14th-order cascade. The first
fitter slice should:

1. expose the measured spectrum and ARX/formant candidates without committing
   sections;
2. let the operator place and assign pole pairs to persistent section indices;
3. begin all-pole or nearly all-pole;
4. add zeros deliberately for global spectral shape/slope or an identified
   antiresonance;
5. offer a `follow` constraint when a zero must retain its relative position
   between neighboring pole/formant tracks across a transition;
6. compare the complete cascade to the target continuously; and
7. use marginal contribution, holds, and explicit operator intent as FIT growth
   evidence without sorting, renumbering, or silently assigning a lane.

Corpus PCA belongs outside packed-domain authority: it may produce a prototype
and deviation coordinates for exploration, but it does not infer section
correspondence or replace the ordered root geometry.
