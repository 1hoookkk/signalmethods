# TRENCH NATIVE — CANONICAL INSTRUCTIONS

This file governs the shipping native app under `native/`.

## TECH STACK

Required:

- C++20.
- Qt 6.11.2 Widgets: `QMainWindow`, `QAction`, `QUndoStack`, Model/View,
  `QGraphicsView`, `QPainter`.
- Pure Qt-free C++ for body, DSP, fitting, analysis, and numerics.
- Eigen for linear algebra.
- Ceres for optimization.
- PocketFFT for FFT analysis.
- JUCE 9 for audio and device/plugin hosting only.
- Python 3.13 for research and corpus work only.
- pybind11 for a narrow research bridge to the C++ core.
- CMake Presets, Ninja, and vcpkg manifest mode.
- GoogleTest, CTest, and Qt Test.

Forbidden in the shipping app:

- Browser UI, Electron, Tauri, QML, or WebView.
- Rust UI.
- Python UI or runtime DSP.
- JUCE GUI.
- Generic plotting libraries.
- GPU-first rendering.

The existing Rust and browser apps are behavioral references until native parity
is proved. Do not add new shipping behavior there.

## METHOD

### Core

- Preserve semantics. Reimplement mechanics.
- Keep domain, DSP, fitting, analysis, and file formats independent of Qt.
- The native body is 8 corners × 7 ordered 2P2Z sections × 5 packed `u16`
  words.
- A section is pole geometry, zero geometry, and scale. It may contain
  conjugate, real-axis, or degenerate root pairs.
- Section index is correspondence. Never sort, renumber, or pair by frequency,
  strength, or discovery order.
- Keep legacy 4-corner × 6-section bodies as a separate container path.
- Interpolate packed words in M, then Q, then Z. Decode after interpolation.
- Compare the complete serial cascade. Section gains multiply.
- Preserve exact bytes wherever the current implementation preserves them.

### Analysis and fitting

- ARX and Praat produce evidence and candidates. They do not author a
  14th-order cascade.
- Poles, zeros, and section scale are independently legitimate realization
  variables. Each may independently be free or held.
- Mathematical availability does not require the UI to expose every variable
  equally.
- Pole-first authoring, all-pole or nearly all-pole initialization, explicit
  lane assignment, and Kerkhoff-style zero follow are optional workflows and
  initialization methods. They are not mathematical restrictions, runtime
  laws, permanent solver limitations, or section-type rules.
- Do not encode the current preferred authoring workflow into the body model.
- PCA may provide a corpus prototype and deviation coordinates. It does not
  assign sections or override packed correspondence.
- FIT changes the same authored state used by direct manipulation. No second
  hidden model.
- The solver changes one section per accepted step.
- Every live step reports the section index and the updated working state.
- Pinned sections are excluded from solver writes immediately.
- FIT growth uses residual need, marginal contribution, holds, and explicit
  operator intent. “Empty and unheld” is insufficient.

### FIT interaction

- FIT moves the curve tokens live.
- Show which section the current solver step touches.
- Click a token to pin or unpin it. Do not provide a duplicate PINNED/FREE
  button in a panel.
- A pinned token remains visibly fixed while other tokens move.
- Draw cancellation on the curve where it occurs. Do not hide it in a warning,
  inspector, or text report.
- `STOP & KEEP` stops the solver and commits the current working state as one
  undoable edit.
- Discard restores the exact pre-fit state. It is not the only way to stop.
- Stale worker output may never overwrite a pin, edit, corner change, stop, or
  discard.

### Contribution

- Measure section contribution as
  `cascade_db(all) - cascade_db(all minus section)` on the displayed frequency
  grid.
- Include packed section scale.
- Use contribution for token height, visibility, and FIT growth evidence.
- Do not use pole-zero interval as a salience score.
- A visibility threshold is a declared UI choice, not a corpus invariant.

### UI

- Plot first. Secondary tools dock around the plot.
- Use small legible controls and explicit scientific labels.
- Color identifies data, section, selection, live fit, pin, and cancellation.
- Keep controls inside measured geometry and use screen area efficiently.
- Use `QPainter` for plots and tokens.
- Do not copy historical OS chrome, reference layouts, widget shapes, retro
  decoration, or unspecified interactions.
- Use minimal text. Do not repeat values or state.
- Do not show AI language, confidence prose, explanations, suggestions, or
  generated shorthand.
- Do not invent section roles, names, rankings, or taxonomies.

### Verification

- Add a failing test before changing a packed law or solver invariant.
- Test all 65,536 packed words.
- Test every live corpus body at its own datum.
- Test native and legacy byte round-trips separately.
- Test conjugate, real-axis, and degenerate geometry.
- Test packed interpolation before decode.
- Test complete-cascade response against the current runtime oracle.
- Test one-section-per-step, live pin changes, pinned immobility, stop-and-keep,
  discard, undo, stale results, and cancellation display data.
- A build is not acceptance. Require plots, packed-runtime checks, and listening
  for authored sound.

## CITATIONS

Encoding and packed interpolation:

- Dave Rossum, E-mu Systems, US 5,170,369, *Dynamic Digital IIR Audio Filter*,
  1992. `C:\Users\hooki\Downloads\df2_notebooklm_zplane_lean_20260609\01_primary_sources\US5170369_Rossum_E_Mu_Dynamic_Digital_IIR_Audio_Filter.pdf`
- Dave Rossum, US 10,514,883, *Morphing Digital Audio Filter*, 2019.
  `C:\Users\hooki\Downloads\df2_notebooklm_zplane_lean_20260609\01_primary_sources\US10514883_Rossum_Morphing_Digital_Audio_Filter.pdf`
- Live mechanical oracle: `trench-core/src/minifloat.rs`,
  `trench-core/src/stage_law.rs`, `trench-core/src/response.rs`.

Serial pole-zero analysis-by-synthesis:

- Bell et al., *Reduction of Speech Spectra by Analysis-by-Synthesis*, 1961.
  Operator places and adjusts complex-plane poles and zeros against the complete
  response. `C:\Users\hooki\Downloads\ReductionofSpeechSpectraAnalysis-by-Synthesis6112c.pdf`
- J. Kerkhoff and L. Boves, *Designing Control Rules for a Serial Pole-Zero
  Vocal Tract Model*, Eurospeech 1993, pp. 1705–1708. Pole/formant tracks come
  first; zeros shape the global spectrum; `follow` prevents accidental crossing
  cancellation. `C:\Users\hooki\Downloads\qcinst_extracted\kerkhoff93_eurospeech.pdf`
- D. Klatt, *Software for a Cascade/Parallel Formant Synthesizer*, JASA 1980.
  `C:\Users\hooki\Downloads\df2_notebooklm_zplane_lean_20260609\01_primary_sources\Klatt_1980_Cascade_Parallel_Formant_Synthesizer.pdf`
- `C:\Users\hooki\Downloads\qcinst_extracted\1960_1_1_014-016.pdf`
- `C:\Users\hooki\Downloads\qcinst_extracted\1961_2_1_001-002.pdf`
- `C:\Users\hooki\Downloads\qcinst_extracted\1961_2_4_018-018.pdf`
- `C:\Users\hooki\Downloads\qcinst_extracted\1962_3_2_020-021.pdf`
- `C:\Users\hooki\Downloads\qcinst_extracted\1964_5_2_006-008.pdf`
- `C:\Users\hooki\Downloads\qcinst_extracted\1964_5_3_001-007.pdf`

Human-in-the-loop fitting:

- Zhu et al., ICLSP 1996. ARX estimates are followed by graphical editing.
  `C:\Users\hooki\Downloads\qcinst_extracted\zhu96_icslp.pdf`
- US 6,195,632. Iterative formant estimation requires initialization checks and
  permits manual reassignment when tracks fail.
  `C:\Users\hooki\Downloads\US6195632.pdf`

Prototype and interpolation:

- Sandell and Martens, *Prototyping and Interpolation of Multiple Musical
  Timbres Using Principal Component-Based Synthesis*.
  `C:\Users\hooki\Downloads\PCA.pdf`
- Martens, *Principal Components Analysis of Spectral-Cue Variation in
  Directional Transfer Functions*, 1987.
  `C:\Users\hooki\Downloads\1987 — PCA + spectral-cue resynthesis.pdf`
- Burred et al., 2006. `C:\Users\hooki\Downloads\1033Burred2006.pdf`

Machine behavior:

- E-mu Mo'Phatt Operation Manual, z-plane filter pages.
  `C:\Users\hooki\Downloads\df2_notebooklm_zplane_lean_20260609\01_primary_sources\E-MU_MoPhatt_Operation_Manual.pdf`
- Extracted local text: `ref/mophatt_zplane_pages_extracted.txt`.

Corpus evidence:

- `dev/EVIDENCE.md`.
- `native/README.md`.
- `native/trench-core/tests/core_tests.cpp`.
