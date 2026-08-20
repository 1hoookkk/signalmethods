# TRENCH native boundary and first Qt shell

The `trench-core` library is the first native shipping slice. It remains
Qt-free: the packed body, root geometry, interpolation, response, and analysis
quantities are shared by the Qt workstation, command-line verification, audio
boundary, and narrow Python research binding without acquiring UI semantics.

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
- A minimal resizable `QMainWindow` whose central `QPainter` surface displays
  the canonical 512-point P2K grid evaluated by `trench-core` from a real
  packed body.
- A JUCE audio-only boundary and a pybind11 research-only boundary. Neither is
  connected to the UI or runtime DSP yet.

## Pinned toolchain and dependencies

The release preset is intentionally exact and reproducible. On 2026-08-20 it
pins the current stable upstream releases used by this slice:

| Component | Version | Source route |
|---|---:|---|
| C++ | C++20 | MSVC through the Visual Studio developer shell |
| CMake | 4.4.2 | official archive, SHA-256 checked by the bootstrap |
| Ninja | 1.13.2 | tool registry from the pinned vcpkg checkout |
| Qt Widgets and Qt Test | 6.11.2 | vcpkg overlay over the pinned Qt port |
| JUCE audio modules | 9.0.1 | official source archive, hash checked by CMake |
| Eigen | 5.0.1 | pinned vcpkg baseline |
| Ceres | 2.2.0#6 | pinned vcpkg baseline |
| PocketFFT | 2024-11-30 | pinned vcpkg baseline |
| pybind11 | 3.1.0 | pinned vcpkg baseline |
| GoogleTest | 1.18.0 | pinned vcpkg baseline |
| Python research host | 3.13 | selected explicitly from the system launcher |

The vcpkg baseline is commit
`45f9f39362a4c52e2b1fbe57b7e649db7f3d96d4`. The bootstrap script verifies
that exact checkout before use. Qt 6.11.2 and JUCE 9.0.1 are fixed at their
source archives and checksums because the same baseline does not yet supply
those required releases. Dependency work is capped at two concurrent jobs to
avoid exhausting a workstation during a source build.

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

## Corpus boundaries

The aggregate counts above verify mechanical decode/rebuild coverage only.
They are not an authoring census. P2K, Morpheus, Morph Designer XML, and the
authored recipe sets are separate lineages and must not be pooled to infer
stage activity, contribution, placement rules, or fit priors. The current
P2K response grid is 512 logarithmic samples from 20 Hz to `0.499 * datum`.

Configure, build, and test the release shell:

```powershell
native/configure.ps1
native/build.ps1
native/test.ps1
native/run.ps1
```

The executable is
`out/build/windows-msvc-release/native/app/trench_native.exe`. The default
view loads `ref/presets/P2k_013_talking_hedz.bin`; use `-Body <path>` and
`-SampleRate <hz>` to select another body and authoring datum. The research
module is not imported or shipped by the application. The legacy aggregate
contribution tool remains a mechanical diagnostic from the accepted core
slice; its pooled output is not valid authoring evidence.

## Fitter boundary

ARX is analysis evidence, not a command to fit a 14th-order cascade. Poles,
zeros, and scale are independent realization variables and each may be held or
free. P2K has no zero-placement rule, per-slot role, band, or frequency order.
A future fitter must compare the complete factored cascade, use a measured seed
before discrete polishing, solve gain only after geometry, and round-trip the
written bytes before absolute-dB acceptance. The current shell implements none
of that fitter or write path.

Corpus PCA belongs outside packed-domain authority: it may produce a prototype
and deviation coordinates for exploration, but it does not infer section
correspondence or replace the ordered root geometry.
