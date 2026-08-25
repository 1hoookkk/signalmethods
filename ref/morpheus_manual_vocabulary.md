# E-MU's own words for the machine

Extracted 2026-08-13 from `ref/morpheus_manual_zplane_descriptions.txt`, all
1661 lines read. Everything below is verbatim with its line number. Nothing is
paraphrased and nothing is coined. This is what the editor should call things.

## The machine

| E-MU's word | what it names | line |
|---|---|---|
| **frame** | one corner — a whole stored response | 226, 241, 714, 870 |
| **cube** | a filter with three axes, 8 frames | 208, 870, 1594 |
| **square** | a filter with two axes, 4 frames | 208 |
| **Morph** | first axis | throughout |
| **Freq. Tracking** | second axis | throughout |
| **Transform 2** | third axis; absent on a square | 209 |
| **Morph Offset** | the axis's static offset control | 295, 880 |

> "A suffix of "4" or ".4" indicates filter is square, not cube and does not
> contain a Transform 2 axis." — :208

> "Frames 1 and 3 contain multiple, unevenly spaced notches and peaks, while
> frames 2 and 4 are essentially flat. Modulating upward along the Morph axis,
> pushes the filter towards flat." — :714 (F066 Notcher 2.4)

> "Frame 1 starts with notches at 50, 100, 200, 400, 800, and 1600Hz." — :241
> (F003 Flange 2 .4)

> "Cube construction - Use the Aux. Envelope to provide attack transient which
> then fades off to rear frames." — :1594

Frame 1 is the all-axes-zero corner, Morph varies fastest, and the third axis is
the "rear". Our `m | q<<1 | z<<2` is that numbering, zero-based.

## The chain

Block diagram, p.99 (:188-202). Figure text extracts out of reading order; these
are the labels as printed:

> "In" · "1 Low Pass Section" · "Fc  Q" · "6 Parametric Equalizer Sections" ·
> "Fc  Bw  Gain" ×6 · "Out"

> "Right away you can see that we now have 20 different parameters to control.
> Ah, there's the catch. How can all these parameters be effectively controlled?
> 20 envelope generators? We don't think so." — :185

2 + 6×3 = 20, which fixes the count at **seven sections in series**.

**Read the sentence that introduces the figure before using its labels** (:169):

> "The Morpheus filter is actually much more complex than the four parametric
> sections described above. As an example of its power, the diagram below shows
> **one of the possible ways that the Morpheus filter can be configured.**"

One of the possible ways. The seven is real; the *types* are an example.

The figure's job is to produce the number 20, and the number 20's job is to sell
the Morph wheel — that is the sentence immediately after it (:185). The four
pages before it teach a 1993 keyboard player "lowpass" and then "parametric,
which has Frequency, Bandwidth and Boost/Cut", so those are the only two words
the reader owns when the diagram arrives, and `Fc, Q` + 6 × `Fc, Bw, Gain`
happens to total exactly 20. It is a teaching figure with a punchline, not a
schematic. (Inference from how the passage is built — E-MU's intent is not
stated. The census below is measured.)

A section is not a lowpass or an equalizer — it is a pole and a zero, and what
it does depends only on where they go. Measured over 792 factory sections (33
character bodies × 4 frames × 6): **zero are a lowpass or a plain shelf.** 61.1%
make a peak and a notch, 23.0% a peak, 13.4% a notch, 2.5% nothing. Broadband
shape comes from the two end sections opposing each other — S1 tilts −22.5 dB,
S6 +29.8 dB — while both also carry the body's biggest peak/notch features.
See `bench/facts.py` LOWPASS_SECTIONS, SECTION_TILT_S1_DB, SECTION_TILT_S6_DB.

**Fc**, **Bw**, **Gain** and **Q** are E-MU's parameter words and stay. No
section is ever given a role-name anywhere in the manual.

> "Often times, several parametric sections are cascaded (placed one after
> another) in order to create complex filter response curves." — :157

## The families

Header list, :206 — "The Z-Plane filters are categorized into groups of:
Flangers, Vowel Filters, Traditional Filters, Parametric Filters, Instrument
Models, etc."

Section headings as actually printed: **FLANGERS** :210 · **DIPTHONGS** :384 ·
**STANDARD** :536 · **EQUALIZATION FILTERS** :748 · **COMPLEX FILTERS** :793

> "Members of this filter family contain a series of notches with various depths,
> widths and frequencies." — :211 (Flangers)

> "Implemented with parametric equalizer subsections, the resonances do not have
> the traditional overall lowpass effect that a true vocal resonance would have.
> Instead, they are placed at the same frequency as the resonances would be found
> in a true vowel, but the response at high frequencies is essentially flat to
> allow high frequencies of the samples to get through the filter." — :385
> (Dipthongs)

> "These filters are variations on traditional 2 and 4-pole filter models" — :537
> (Standard)

> "Many of these filters have never existed for musical applications before!"
> — :794 (Complex)

## The descriptors

Every count is over the 184 filter entries (:205-1661), excluding the axis names
and stop words. This is the whole working vocabulary — it is small on purpose.

    peaks 76 · notches 75 · sweeps 68 · brightness 67 · cutoff 52 · flange 38
    resonance 35 · flat 33 · depth 33 · lowpass 32 · volume 32 · tunes 32
    resonant 31 · bump 21 · notch 18 · pole 19 · roll-off · brickwall · slope
    comb · boost · highpass · allpass · Q

Shapes are described as whole responses, never as a stack of stages:

> "A series of deep notches spaced at octave intervals" — :231
> "Alternating resonant peaks and notches, spaced at approximately" — :1225
> "A series of unevenly spaced resonant peaks converge to a single" — :1314
> "Moves the 'chord' of resonant peaks from first to second" — :1067
> "Changes from a 4-pole filter (-24dB/oct.) to a 2-pole filter (-12dB/oct.)"
> — :819

## What is NOT in the manual

No section is called air, throat, chest, crown, shoulder, body or anchor. No
stage has a role. Every per-filter description is of the total response. Any
name for a section is ours, and `design/STAGE_LAW.md` should say so.
