# The music theory of the loved ROM filters

Extracted 2026-08-02 from the 33 verified P2K dossiers
(`scratchpad/rom_music_theory.py` over `dossiers/characters/P2k_*.json`).
Every claim below is measured from the stored corner words, not from manuals.

## 1. A filter is a chord voicing — but not in concert pitch

Each corner rings 4–6 voices (complex poles, r ≥ 0.90). The median worst
distance from the equal-tempered grid is ~50 cents: E-MU never tuned poles to
A440. **The theory is intervallic, not tonal.** Interval structure carries the
character; absolute pitch is free. (Tonal gravity is our KEY feature's job —
the ROMs never had it.)

## 2. Voicing law: tight cluster in a wide frame

Adjacent-voice spacing across all 33 (M0 corners): the mass sits at **1, 3, 4,
and 5 semitones** — seconds, thirds, fourths, jazz-cluster spacing — with the
octave (12) as the consonant fallback. Perfect fifths exist but do not
dominate. Then **2–3-octave gaps** out to the frame voices. Nothing is voiced
like a textbook triad in open position.

## 3. Register architecture: anchor · mouth · air

Voice census by band: **22 anchors** below 400 Hz, **57 mouth voices**
400 Hz–3.5 kHz, **84 air voices** above 3.5 kHz. The air band is the most
populated — the loved filters keep more voices above 3.5k than anywhere else.
A body ≈ optional bass anchor + the mouth cluster + air.

## 4. Motion taxonomy: three species of voice leading (M0 → M100)

Classified per filter from index-paired pole travel:

- **CONTRARY — the E-MU signature (≈18/33).** Registers trade places: one
  voice rises an octave or more while another falls. The most-loved carry the
  biggest trades: Millennium +77/−62 st, BolandBass +83/−68, EarBender
  +53/−51, TalkingHedz −26/+38. When in doubt, author contrary motion.
- **PARALLEL (7/33).** Every voice slides the same direction — the classic
  sweeps: MegaSweepz, DeadRinger (uniform ≈ −22 st), AcidRavage, ToothComb,
  DreamWeava, RazorBlades, Ace of Bass. FuzziFace is the limiting case: five
  stages exactly +12 st = the coherent retune.
- **OBLIQUE — the talkers.** Most voices hold, one or two speak: DeepBouche
  holds 4 of 6; TalkingHedz and MultiQVox hold 3 and throw one voice across
  two octaves. Vowels hold; sweeps don't.

## 5. Zero grammar: three uses of the hall

130 pole-zero pairs measured; the zero's interval from its own stage's pole
is trimodal:

- **Razor zeros** within ±2 st of the pole (~26/130): carve immediately
  beside the peak — edge, not space.
- **Valley zeros** +3..+10 st above the pole: the formant valley — the
  Talking Hedz move.
- **Parked zeros** ≈ +58 st (≈5 octaves up, n=8): out of the audible fight —
  the section becomes pure pole, the zero is only broadband tilt.
- Rare deep-drop zeros far below (−20..−65 st) for floor shaping.

## 6. Q is an attitude, not a knob

(From the fitted stage law, restated for completeness.) Three clades: same
notes with radii pushed toward the ~66 dB R′ ceiling (38% of stages); re-voice
to new notes (REZ/FLG/PHA); coherent retune (+2 octaves whole-cascade,
FuzziFace). Always authored by editing poles over a held zero scaffold. The
air pole is routinely spared the push (TalkingHedz Q100 keeps it ~30–48 dB) —
that is why Q100 screams without turning to glass.

## 7. The vocabulary method — pose reuse is the house method

Ten tuning-level reuse relations inside just 33 products (identical pole-note
sets, radii/stage-order re-edited):

- Ooh-To-Eee reversed **is** Eeh-To-Aah (M0↔M100 swapped)
- TalkingHedz's soft mouth = UbuOrator's destination corner
- DeadRinger lands on the Ooh mouth
- Millennium = MeatyGizmo verbatim (word-identical corners, different Q table)
- KlubKlassik / AcidRavage / ToothComb share a pose; BolandBass / LucifersQ /
  BassTracer share another; CruzPusher / FuzziFace share their M0

E-MU kept a small library of poses and shipped **new pairings**, not new
poses. A reversal is a free second product. This is the historical precedent
for corner dictation: the operator names two poses from the vocabulary; the
product is the pairing plus a Q attitude.

## The composer rules (for dictating corners)

1. Pick 4–6 voices: optional anchor below 400, a mouth cluster spaced 1–5 st,
   one or two air voices 2+ octaves up.
2. Don't tune to A440 — tune the intervals.
3. Pair poses for **contrary** motion for drama, **oblique** to talk,
   **parallel** to sweep.
4. Zeros: razor beside a peak, valley above a formant, or parked high for tilt.
5. Q100 = same mouth, throat pushed to the ceiling — spare the air pole.
6. Reuse poses across bodies; reverse a good pair and it's a new body.
