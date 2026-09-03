# Authoring tool rewrite: Qt app -> Python + marimo

Status 2026-09-04: prepared, not started. The plugin does not change. The Qt app
(`native/app`, 3,851 lines) is replaced by a reactive marimo notebook, `workstation.py`,
that drives the same engine.

## The one rule
The engine truth stays in `native/core`. The notebook is a UI layer. It must never
reimplement the word codec (16-bit minifloat lattice), corner interpolation in word space,
the rewarp from a body's datum rate, the level rule, transposition, or the runner's BITE
law. A scipy biquad drawn from a formula is not a TRENCH body; it drifts from what ships.

## How the notebook reaches the engine
Add a small C ABI to `native/core` and build it as a shared library, `trench_core_c.dll`.
The notebook loads it with ctypes; no compiler on the notebook side.

Functions the ABI needs (all in terms of bytes and plain arrays):
- words: `decode_word(u16) -> double`, `encode_word(double) -> u16`
- bodies: `body_from_bytes(bytes, n) -> handle` (240 legacy or 560 native),
  `body_words(handle, corner, section, out[5])`, `body_set_words(...)`,
  `body_legacy_bytes(handle, out[240])`, `body_native_bytes(handle, out[560])`
- play position: `body_interpolate_words(handle, morph, q, z, out[7][5])`,
  `body_cascade(handle, morph, q, z, datum_hz, host_hz, out[7][5])` (rewarped)
- geometry: `section_import(words[5], datum_hz, out geometry)`,
  `section_export(geometry, datum_hz, out words[5])`, `design(section, host_hz)`
- response: `cascade_response_db(cascade, freqs, n, host_hz, out)`
- audition: `runner_process(cascade, bite, samples, n)` on a mono buffer
- transposition: `transpose_cascade(cascade, ratio, host_hz)`

Parity suite before anything else: for all 33 P2K bodies and 289 Morpheus cubes, bytes
round-trip identically and every corner cascade matches the C++ path bit for bit.

## Hearing it
Primary: the notebook writes the working body to the audition slot as `.body240` (with its
datum rate) and TRENCH Dev hot-reloads it (the `TRENCH_PLAYER_DIAGNOSTICS` path, to be
enabled in the dev build). You hear it in the real plugin, in the DAW, with BITE and
MOVEMENT live. Secondary: `sounddevice` playback of a clip through `runner_process` for
quick checks without the DAW.

## What carries over from the Qt app
- Rows: per section Frequency / Q (bandwidth) / Peak Gain, pole and zero lanes, the zero
  locked to the pole by default, section 6 as the unit-circle notch, cuts.
- Harmonic helpers: pitch as root x n in keyboard intervals; typed frequency lands on the
  nearest word.
- Corners: 4 for P2K, 8 for Morpheus; corner picker; COPY ACROSS.
- Gestures as functions: POSTURE (write the morph partner transposed), SHARPEN (write the
  Q partner with raised radii), TRANSPOSE (affine on a corner).
- Templates: X3 types, Morpheus categories, LADDER (rungs with zeros).
- Plot: fixed grid, hard 0 dB line, never auto-scaled; path meter of the interior peak.
- Body IO: JSON recipe in, `.body240` out with datum; the roster catalogue.

## Order of work
1. C ABI + CMake shared target + ctypes wrapper + parity suite.
2. `workstation.py`: load a body, corner picker, MORPH / Q sliders, the plot.
3. Rows and helpers; gestures.
4. Templates.
5. Export and the audition slot into TRENCH Dev.
6. Retire the Qt app once the parity suite and the 29 native tests have marimo equivalents.

## Known costs
- marimo re-runs every downstream cell per slider move; the C ABI keeps that under a
  millisecond per body, matplotlib is the slow part. Start with matplotlib, move the plot
  to a lighter renderer only if dragging feels late.
- The notebook is one file by design; keep DSP calls in a tiny `trench_py` module beside it.
