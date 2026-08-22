# Morpheus intent → P2K mechanism

Source: E-MU Morpheus Operation Manual (the actual PDF, extracted this
session to scratch), cross-checked row-by-row against
`dossiers/characters/`. Nothing here is inherited prose.

> **Audit 2026-08-13.** That scratch extraction is gone and the PDF is not in
> `ref/`. Only `ref/morpheus_manual_zplane_descriptions.txt` survives, and it
> covers manual pp.84-88 and pp.174+ only. Of the quotes below, these are
> confirmed verbatim against it: the Dipthongs/paravowel paragraph (:385) and
> "Many of these filters have never existed for musical applications before!"
> (:794). These are **NOT verifiable against anything on disk** and must not be
> cited as settled until the PDF is in `ref/`: "Filters A and B represent two
> different complex filters…", "think of them as the settings on a graphic
> equalizer", the p.89 vocal-tract passage, and the four paravowel formant
> tables. See `ref/morpheus_manual_vocabulary.md` for what the manual on disk
> actually says.

## The authoring grammar E-MU states in the manual

- **A filter is a pair (or cube) of whole frames.** "Filters A and B
  represent two different complex filters. By changing a single parameter,
  the Morph, many complex filter parameters can be changed
  simultaneously." Frames are complete shapes — "think of them as the
  settings on a graphic equalizer."
- **The vocal-tract law** (p. 89): "A vowel is really a configuration of
  many muscles, but we consider it a single object … we don't need to
  consider the frequencies of the resonant peaks! You remember the shape
  of your mouth for each sound and interpolate between them."
- **Two vowel architectures**, stated explicitly in the DIPTHONGS intro:
  1. **True vowels** — resonances with the natural overall lowpass a real
     tract has.
  2. **Paravowels** — "implemented with parametric equalizer subsections",
     resonances at true vowel frequencies but **flat at HF** so sample
     brightness passes. E-MU prints the tables:
     - A: 800, 1150, 2800, 3500, 4950 Hz
     - E: 400, 1600, 2700, 3300, 4900 Hz
     - O: 450, 800, 2830, 3500, 4950 Hz
     - U: 325, 700, 2530, 3500, 4950 Hz
     Five resonances per vowel — on a six-section machine that leaves one
     section over, which is exactly the balance/termination organ the
     plates show.
- **VowelSpace (F043, Dr. William Martens)**: any vowel from two
  parameters — Morph = F1 (150→850 Hz), Freq.Track = F2 (500→2500 Hz),
  Transform 2 = "stress" (low stress collapses every formant to schwa).
  Vowels are authored as whole formant configurations, parametrized.
- Categories: Flangers · Vowel filters · Dipthongs (paravowels) ·
  Standard (2/4-pole models) · Equalization · Complex ("many of these
  have never existed for musical applications before") · Instrument
  models. Per-filter text always describes whole-response behaviour;
  no slot is ever named.

## The same grammar, found in the P2K bytes

**The ROM is a frame library.** 132 corners resolve to **120 distinct
corner-frames**, and the 33 names resolve to **28–31 distinct body
shapes** (Millennium==MeatyGizmo, Ooh-To-Eee==Eeh-To-Aah reversed,
KlubKlassik==AcidRavage share both endpoints; the exact count depends on
how single-endpoint sharing is scored). 10 frames are shared across
bodies — identical hz AND radius to four decimals:

- **Ooh-To-Eee and Eeh-To-Aah are the same two vowel frames with the
  morph direction swapped** (OTE M0 = ETA M100, OTE M100 = ETA M0).
- **TalkingHedz M0 = UbuOrator M100.**
- **Millennium and MeatyGizmo share both M0 and M100 frames** (the
  copy-modify pair).
- Cross-family reuse: KlubKlassik/AcidRavage/ToothComb share one M0
  frame; FuzziFace/CruzPusher, BolandBass/LucifersQ,
  BolandBass/BassTracer each share frames. Q100 corners are the same
  frames with radii lifted toward 1, so they hash separately.
- TalkingHedz's M0 frame [199, 890, 1569, 2348, 4606, 9320] and the
  KlubKlassik-family frame [200, 881, 1603, 2315, 4615, 9395] are the
  same voice re-authored within a few cents per formant.

**Slot assignment is the voice-leading, not a role.** When a frame is
reused, its poles land in *different slots*: Ooh-To-Eee M0 carries
(S1 849, S2 541, S3 2154, S4 2841, S5 3622, S6 4372); Eeh-To-Aah M100
carries the identical six poles as (S1 3622, S2 541, S3 849, S4 4372,
S5 2154, S6 2841). The interpolator morphs each slot from its M0 root to
its M100 root, so **the permutation chooses which formant travels to
which** — same two shapes, different journey. That is the whole meaning
of a slot.

**Zeros are the stable carving rack.** Across those reused corners the
zero list stays slot-ordered low→high and nearly frame-invariant
(S1–S4 zeros byte-equal across the OTE/ETA pair); S6 always carries the
terminal unit zero (132/132); SCALE is a per-corner level trim
(one value across all six stages in almost every corner).

## Consequence for TRENCH authoring

Author **frames** — whole six-pole shapes — and author the **pairing**
(which pole travels to which) as the musical decision. Slot roles,
per-slot targets, and per-family slot medians have no basis in either
the manual or the bytes.
