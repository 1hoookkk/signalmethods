# CLAUDE.md

## What the machine is

Seven second-order sections in series. The count is E-MU's, from the Morpheus
manual p.99 block diagram, and the body text checks the arithmetic: "we now have
20 different parameters to control" = 2 + 6x3.

**A section has no type.** That same diagram labels its sections "1 Low Pass
Section" and "6 Parametric Equalizer Sections", but the sentence introducing it
calls that "one of the possible ways that the Morpheus filter can be
configured" — an example, not the machine. Measured over 792 factory sections
(33 character bodies x 4 frames x 6): **zero** are a lowpass or a plain shelf.
61.1% make a peak and a notch, 23.0% a peak, 13.4% a notch, 2.5% nothing. A
section is a pole and a zero. What it does depends only on where they go.

A filter stores whole responses at the corners of a cube. E-MU calls a corner a
**frame**; three axes is a **cube** (8 frames), two axes a **square** (4). The
axes are **Morph**, **Freq. Tracking**, **Transform 2**. Frame 1 is the
all-axes-zero frame and Morph varies fastest; our corner index `m | q<<1 | z<<2`
is that numbering, zero-based.

Between frames the runtime linearly interpolates the ENCODED words. Nothing is
refitted in between. That is the whole morph.

**The design is in the sections.** A section is a row that exists at all eight
frames — pole, zero, scale, eight times. That is the whole authored object.
There is no pairing to author separately: section 3 is section 3 at every frame,
and that *is* the correspondence.

## Binary contract

    560 bytes native = 8 frames x 7 sections x 5 LE u16 packed words
    240 bytes legacy = 4 frames x 6 sections x 5 LE u16
                       still the interchange length on disk and across the C ABI

Word row = minifloat [zero-mag, zero-r^2, pole-mag, pole-r^2, SCALE].
SCALE is that section's broadband level (b0) — one per section, not one per
frame.

Compat path: a 240-byte body loads as six sections plus an identity seventh
(`DFFF FFFF DFFF FFFF DFFF`, which decodes to biquad [1,0,0,0,0] with no
rounding), and its four frames duplicated onto the far plane. Every shipped body
is byte-identical through it; `trench gate` is the proof.

Traps: code editing `words[ci]` for `ci in 0..4` in place must mirror onto `ci+4`
(`set_legacy_word`) or `to_rom_bytes` panics. `interpolate_biquad` returns 7 rows
— a path that demands 6 must slice `[..LEGACY_STAGES]`. The encoder is not
monotone in radius: there is a hole just below `1 - r^2 = 2^-15` where a radius
collapses onto the unit circle, so test the actual value with
`radius_survives_encoding`, never a threshold.

## The loop

    trench doctor    what is decided, and whether the repo still obeys it
    trench facts     every number re-derived from the corpus; drift is a failure
    trench sheet     author — drag poles and zeros on the lane board
    trench inspect   BODY [RATE] [OUT.png] — the cascade plate. Reads 240 or 560
    trench make      A B SLUG [--permutation 0,2,1,3,4,5] [--install]
    trench install   build and install the VST3, then restart FL
    trench gate      prove nothing that already shipped moved

    cargo run -p trench-core --release --bin cube -- TABLE.json
                     author a cube from a section table, write 560 bytes, and
                     prove it: order, round-trip, legacy refusal, section 7,
                     Transform 2

`trench.py` is the canonical entry point. Nothing else is a route.

## Authorities

- `bench/facts.py` — every constant, with either a derivation that re-runs or a
  verbatim quote and citation. A number on the authoring path that is not here
  is a bug.
- `ref/morpheus_manual_vocabulary.md` — E-MU's own words, verbatim with line
  numbers. This is what the editor calls things.
- `AUTHORING_SPEC.md` — what a body is, the word units, the corpus
  measurements, and the list of what has been withdrawn.

## What is true about sections

- Responses multiply; dB add. Judge the whole probed cascade, never a section
  alone. 24% of placed conjugate poles produce no peak in the total at all
  (`POLE_SURVIVAL_PCT`).
- A pole lands where you put it: median 0.02 semitones from the peak it makes.
  Place; do not solve. Rossum, ICMC 1992: "the precise relative placement of the
  poles seems to be not very critical."
- Broadband shape comes from the ENDS opposing each other: **S1 rises** with
  frequency (LF -23.8 dB, HF -2.5) and **S6 falls** (LF +11.3, HF -11.5), while
  both also carry the body's biggest peak/notch features. They are not shelves —
  they are pole/zero pairs pulled far enough apart that the skirt becomes the
  tilt. A section's tilt is set by which side of its pole the zero sits on:
  **zero below the pole makes the section rise, zero above makes it fall.**
- Section ORDER is not a law. Permuting the slots identically at both endpoints
  moves the response 1.78e-14 dB. Permuting them differently per endpoint leaves
  both endpoints bit-identical and moves the middle of the morph by up to
  95.47 dB. The correspondence is the authored quantity.
- Level is per section. 79.5% of character frames happen to share one SCALE word
  across their six sections; 20.5% do not, and both Rossum sources give every
  section its own separated gain a0.
- 51.5% of factory rows put the zero more than 18 semitones from its own
  section's pole. A zero sitting on its pole cannot carve.
- No section has a role. No source names one air, throat, chest, mouth or
  anchor. Any such name is ours, and it is not evidence.
- Deep notches are the mechanism, not a defect: factory bodies dip 69.8 dB below
  their own median.

## Where things live

    trench-core/src/
      cascade.rs     NUM_STAGES, the series cascade, the saturation
      minifloat.rs   encode/decode, PackedCorners, interpolate_words
      engine.rs      per-sample morph, AGC, SLAM
      stage_law.rs   words_from_geometry, the authoring limits
      armadillo.rs   the display coordinate law, R' and theta'
      bin/cube.rs    author a cube from a section table
      bin/topology_gate.rs   the shipped-body freeze

    tools/wordsheet/          the editor (trench sheet)
    tools/inspect_body.py     the cascade plate (trench inspect)
    tools/make_filter.py, body_from_endpoints.py, ir_endpoints.py, tf_ingest.py
                              the authoring path (trench make)
    pyruntime/                trench_core.dll bindings
    bench/doctor.py, bench/facts.py   the health check and the fact table

    plugin/presets/PresetRoster.inc   the shipping roster
    ref/presets/                      the 50 P2K reference bins
    filters/bodies/, plugin/assets/bodies/   frozen; the gate hashes these

## House rules

- Never fill a data gap with a number.
- Quote, never paraphrase from memory. If a source does not answer a question,
  the answer is "NOT IN THE SOURCES" and it stays a measurement.
- The measurement beats the plausible mechanism.
- No comments or docstrings in `tools/` or `pyruntime/`. If a claim carries a
  number it goes where it can FAIL.
- `design/` and `docs/` are narration, not law. Quote E-MU's descriptors; ignore
  the prose around them.
- **The plot is the gate.** Draw the reference beside ours. There is no
  listening step in the loop.

## Build

- PowerShell for cargo; Git Bash picks the wrong linker. cargo is not on PATH:
  `$env:PATH = "$env:USERPROFILE\.cargo\bin;$env:PATH"`
- VST3: `powershell -ExecutionPolicy Bypass -File tools\build_install_vst3.ps1`
- Installs to `C:\Program Files\Common Files\VST3\TRENCH.vst3`; restart FL Studio
  to pick up a new build (DLL cache).
