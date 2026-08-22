# Real-source recording plan — TRENCH originals from the same reality

E-MU's "perfect frequencies" are measurements of real things. We re-derive from
the same things with our own chain: record → MEASURE desk → (MINUS slot where
the character is a DEVICE) → fit → caricature ops → distance gate → ears.

Every capture: dry reference first, 48 kHz, same take for A|B pairs.
The MINUS slot wants two recordings of the SAME material: through the device,
and clean. The difference IS the character.

## Capture method (rules for every device below)

1. **Excitation = exponential log-sine sweep** (Farina), not strums or pink
   noise. Play the sweep through the device, deconvolve against the clean
   sweep: exact impulse response, max SNR, clean phase for pole-zero
   estimation. Musical takes are for EARS afterward, never for measurement.
2. **Park the knobs.** Never sample a moving sweep (time-variance smears the
   spectrum). Capture static positions — heel / 25 / 50 / 75 / toe — one
   sweep each. Endpoints become M0/M100; mid positions verify that the
   engine's packed-domain interpolation passes through the real device's
   middle (the boring-detector for morph travel).
3. **Fit low/mid first.** Combs and phasers out-notch a 6-section budget;
   weight the fit psychoacoustically (Bark/ERB), prioritize 20 Hz–4 kHz,
   truncate upper comb teeth. Three authored notches read as a flanger
   (FlangerLite's own trick); twelve chased ripples read as noise.
4. **Endpoints are half the body.** Static M0/M100 captures define only the
   morph axis. Every capture gets an assigned Q-JOB from the measured grammar
   (scream = radius lift, choke, invert = the MeatyGizmo axis-swap, gearbox =
   DreamWeava direction flip, null = FuzziFace tone-only) and the Q100 corners
   are authored to that job, then certified — stability is the compiler's
   gate, not the recording's.

## The devices (MINUS-slot captures)

- **303 squelch** (BassBox-303, TB-OrNot-TB, AcidRavage) — any 303 clone or
  plugin driven by the same MIDI line twice: filter wide open (reference) vs
  resonant sweep (source). Capture at three cutoffs: closed, mid, open —
  those become M0 / mid check / M100. The resonance-tracks-the-note behavior
  is the thing to keep.
- **Wah pedal** (EarBender, Wah Wah 3) — one strummed loop through the pedal:
  heel-down take, toe-down take, plus a slow rock through. Heel = M0,
  toe = M100. The dossier says the magic is TWO poles beating ~1 st apart —
  check the fit preserves the pair before caricaturing.
- **Fuzz pedal** (FuzziFace) — fuzz ON vs bypass, same riff. We only want the
  EQ skeleton of the fuzz (the fixed razor formant); GRIT supplies the actual
  distortion in-engine. Q lift must stay 0 — Q is the tone knob.
- **Cheap radio / phone speaker** (RadioCraze) — play pink noise + a full mix
  through a real small radio, mic it; same material from the monitor as
  reference. Full mix = M0, radio = M100: the wheel is "how cheap."
- **Flanger/phaser pedal parked** (AngelzHairz, FreakShifta, CruzPusher) —
  pedal with LFO off / rate zero, parked at two knob extremes = the two morph
  ends. A comb is the fitter's weak spot (measured) — expect to author the
  notch count down to ~3, like FlangerLite does.

## Direct captures (no MINUS needed)

- **Mouth** (vowel set, DeepBouche, UbuOrator) — the RECORD session (Tyson at
  the mic, THE FLOW brief): sustained vowels + the French rounds. Harmonic
  envelope lane already proven on Peterson-Barney tables.
- **Talkbox / cupped-hand wah over a speaker** — free vowel-adjacent characters
  no ROM filter owns; candidates for pure originals.
- **Objects with a body** (ZoomPeaks nasal cluster, KlangKling ring) — strike
  resonant objects (tube, tin, glass): LTAS lane handles unpitched hits.

## What we do NOT record

Millennium/MeatyGizmo/LucifersQ/DeadRinger-class synthetics have no physical
source — they are authored geometry. Those originals come from the workstation
using the measured grammar: crossings/relays, one-axis swaps (the MeatyGizmo
trick), Q-as-gearbox (DreamWeava), bloomers.

## The gate

Every original: `python tools/distance_gate.py <body> [datum]` — nearest ROM
neighbor >= 3 dB RMS (validated: Millennium scores 0.00 vs itself, HOWLER 14.6).
Then THE BAR, then Tyson's ears.
