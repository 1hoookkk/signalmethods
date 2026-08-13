# Brief

What the sources establish, what is a workstation rule, and what is neither.

## Documented

**Palette** — Martens, ICMC '85 Proceedings, pp. 355–360.

Hear sound → drag line length → record perceptual amount → fit perceptual
mapping → invert mapping.

Simplex fit, more than 90% of variance in five points, iterated adaptively to
re-space stimuli at equal perceptual distance. One synthesis parameter at a
time, on a DX7. Timbre matching places a new voice on an existing scale against
a fixed standard.

**Martens 1987** — ICMC Proceedings, p. 274.

Banded spectra → PCA → fitted score trajectories → inverse PCA → resynthesised
spectra.

Twenty-four critical bands. Synthetic scores yield novel spectra.

**Martens 2002a** — ICAD.

Adjust comparison between references → perceptual bisection → lookup table.

Five settings, the median is the anchor and origin. The documented interface is
Higher/Lower adjustment, not dragging.

**Martens 2001** — ICAD.

Perceptual ratings are modelled and the prediction equation is inverted to
obtain control values for a desired percept. Multiple regression.

**Sandell & Martens** — "Prototyping and Interpolation of Multiple Musical
Timbres Using Principal Component-Based Synthesis", p. 34.

A group is common structure plus the deviations that distinguish its members.
The prototype is synthesised from the common patterns; interpolation applies the
distinguishing patterns to the prototype.

Scores are meaningless without their mean and rotation matrix.

**Burred, Röbel & Rodet** — LSAS 2006, §2.3, Figures 1–2.

Resample envelopes onto a fixed frequency grid before PCA. Partial indexing
misaligns f0-invariant features; envelope interpolation aligns them.

**US5170369 and the ARMAdillo encoding note.**

Filter architecture, interpolation, log-polar encoding. Pole display:

    R' = 20 log10(1 / (1 - R))
    theta' = pi (10 + log2(theta / pi)) / 10

with theta below pi/1024 excluded.

**Not applicable.** The EMU8000 programmer's guide describes one lowpass per
channel with a cutoff and a Q index; it is not a specification for body
authoring. US5943427, US5952599 and the NASA technical memorandum cover
spatialisation, generative music and HRTF measurement.

**Already in this repository.** `recipes/*.json` carry four named corners, an
explicit `slot` per section, and sections expressed as frequency, bandwidth and
gain. `arma_endpoint::fit_arma_pinned` refines six pole/zero sections jointly
against a measured magnitude spectrum with pins held, leaves sections below
0.05 dB improvement as identity, and validates roots through `stage_law`.
`praat_endpoint` carries formant lanes as frequency and bandwidth.

## Workstation rules

These are decisions, not findings. No source is cited for them.

- Assign SOS lane ownership before fitting.
- Fit the complete serial cascade.
- Preserve correspondence.
- Responses multiply and dB responses add.

The last rule has a consequence: error is measured on the sum of the section
curves. Pinning is the only thing stopping the optimiser from satisfying that
sum by reseating a different section.

## Modern translation

Optional, and clearly marked as translation rather than as documented practice.

- Use Palette's draggable perceptual line to calibrate a control.
- Use PCA-generated spectra as possible fitting targets.
- Do not claim that Palette's line is a PCA path or a filter-response editor.

Bisection uses Higher/Lower adjustment, per Martens 2002a. Store the mean and
rotation matrix alongside any scores: a basis that moves under saved scores
corrupts them.

## Chain

    envelopes -> fixed log grid -> PCA (store mean + rotation)
      -> score trajectory -> inverse PCA -> target spectrum (Hz, dB)
      -> pinned ARMA fit -> slots -> words -> 240 / 560 bytes

Components never become lanes. They produce a target the fitter meets.

## Removed

Invented here, unsupported by any source, and not to be reintroduced:
response-curve dragging with an influence width; a two-segment morph warp; a
`station.json` project format; the laws vocabulary (`crown_db`, `dip_db`,
`tilt_db`, `meet_st`, `gap_oct`); the corner and section naming grammar;
golden-angle corner hues; a plain radius/angle pole view in place of the
documented mapping.

## Open

Converting a WAV to an envelope needs a decoder. Authoring in PC space does not.
