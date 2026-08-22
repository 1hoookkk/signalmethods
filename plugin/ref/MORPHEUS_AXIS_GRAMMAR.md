# What the three axes are for

Extracted 2026-08-13 from `ref/morpheus_manual_zplane_descriptions.txt` by
`tools/extract_axis_grammar.py` into `ref/morpheus_axis_grammar.json` — all 184
filters, verbatim, with their manual line number. Nothing here is paraphrased.

This is the only evidence anywhere about a **designed third axis**. No 3-axis
body has ever been decoded; E-MU described 89 of them in words.

## The separation, measured over E-MU's own descriptions

Percent of each axis's live descriptions that mention the theme. A filter can
mention more than one, so rows do not sum to 100.

| theme | Morph | Freq. Tracking | Transform 2 |
|---|---:|---:|---:|
| frequency move | **58%** | 36% | 19% |
| notch / peak shape | **39%** | 13% | 11% |
| key tracking | 1% | **43%** | 5% |
| brightness / tone | 7% | 22% | **34%** |
| resonance / depth | 9% | 14% | **31%** |
| **amplitude / volume** | **2%** | **1%** | **42%** |
| flatten / defeat | 11% | 4% | 2% |
| *live descriptions* | *170* | *170* | *93* |

**Morph picks the shape. Freq. Tracking places it on the keyboard. Transform 2
sets how much.**

The amplitude row is the proof: 2% / 1% / **42%**. Morph and Freq. Tracking
essentially never touch level; Transform 2 is mostly about level, and about the
two things that read as level — brightness and resonance depth.

## Transform 2, verbatim

> "Controls the amplitude of all of the resonances, with 000 providing the
> lowest resonance amplitude, and 255 providing the highest resonance
> amplitude." — F022 AEParaVowel

> "Increases resonance and introduces some peaks." — F004 CubeFlanger

> "Controls volume, depth of effect." — F021 AEParLPVow

> "Increases depth of flange." — F020 CO>FlngT

> "Provides volume and brightness control with velocity and/or key position"
> — F012 Flng>Flng1

## Morph, verbatim

> "Controls movement between vowels." — F022 AEParaVowel

> "Tunes the filter notches." — F004 CubeFlanger

> "Moves all of the notches up in frequency to above 10kHz, with decreasing
> depth." — F003 Flange 2 .4

## Freq. Tracking, verbatim

> "Tracks filter to keyboard frequencies with Note-on key assignment. Controls
> brightness." — F001 Low Pass Flange .4

> "Moves the notches to 400, 800, 1600, 3200, 6400, 12800Hz, which allows key
> tracking to keep the "sweet spot" of the flanger centered over the harmonics
> of the note." — F003 Flange 2 .4

> "Shifts all of the resonances up in frequency." — F022 AEParaVowel

## Squares and cubes

90 filters are squares (`.4` suffix, 4 frames, no third axis) and 94 are cubes
(8 frames). **85 of the 90 squares say Transform 2 is "Not used"**, which is what
the suffix means. 89 of the 94 cubes have a real Transform 2 description.

Families as the manual prints them: FLANGERS 21 · DIPTHONGS 23 · STANDARD 26 ·
EQUALIZATION FILTERS 6 · COMPLEX FILTERS 108.

## What this is good for

1. **Authoring.** A cube's third axis should change intensity, not position. A
   Transform 2 that moves poles up an octave is not what E-MU built.
2. **Validating a decode.** The cube ROM has never been decoded and a candidate
   decode has no oracle. This file is 89 falsifiable predictions: F004's
   Transform 2 must raise resonance, F022's must scale every resonance amplitude
   together, F003's Freq. Tracking must land its notches on exactly 400, 800,
   1600, 3200, 6400, 12800 Hz. A decode that satisfies those is right.

## Limit

This is E-MU describing **sound**, not geometry. "Increases resonance" does not
say which section's radius moves. It constrains a decode and aims an authoring
choice; it does not hand over numbers.
