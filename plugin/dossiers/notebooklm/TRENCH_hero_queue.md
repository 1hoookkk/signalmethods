# TRENCH hero authoring queue — the ten planned originals (verbatim specs)

These are TRENCH's own bodies to be authored: originals in the species of
loved references, never copies. Each spec states pose vocabulary, motion,
Q attitude and authoring path. Two types (BassTracer, EarBender) were
added 2026-08-09 with anatomy read from the cascade inspector.

## HERO RECIPES — the v1 authoring queue

These are the eight bodies that need building. Each is a TRENCH original in the
species of a loved ROM reference, never a copy. The method: author the two
poses from the behavior spec, null against the reference to prove the species,
then push for daylight. Ears are the gate.

### Open to Wall — MegaSweepz species [LPF]

The DJ drop sweep. THE transition filter.

- POSE VOCABULARY: M0 = an open lattice — poles spread across the top octave
  (near-transparent, barely a filter), zeros parked to cancel rather than
  carve. M100 = the wall — everything pulled under the midrange, one or two
  poles at r→1.0 in the 1–3k band, the "loud" coming from resonant mids not
  broadband level. Scale stays honest; the AGC does no favours.
- MOTION: parallel downward — the whole frame slides 2–3 octaves together.
  The open lattice collapses into the wall. Section order never scrambles
  (zero crossings). The sweep IS the product.
- Q ATTITUDE: asymmetric arming/defusing. Two sections push radius hard
  toward the ceiling. ONE section DEFUSES — its radius drops as the others
  arm, a relay in the Q axis. The defuser keeps the sweep from turning to
  glass at Q100. Never a uniform lift.
- AUTHORING: build the open pose first (aim for near-flat, no single peak
  above +6 dB). Build the wall pose second (target: resonant mids, 20+ dB
  peaks in the 1–3k band). Wire Q as a trade between two nominated sections
  — pick which one defuses. Null against MegaSweepz reference to confirm the
  species, then daylight-push the wall resonance further.

### Sub to Squelch — BassBox-303 + TB-OrNot-TB species [LPF]

The acid line. KEY-tracked squelchy bassline.

- POSE VOCABULARY: M0 = the sub engine — a real low pole anchoring the
  floor, zeros sitting above it as a lid (boost with a ceiling). A harmonic
  ladder above: 3–4 poles spaced at near-harmonic intervals over the root,
  strict order, zero crossings allowed nowhere. M100 = the same ladder
  converged toward the mids from both directions, the sub still holding.
  One section (the register-jumper) leaps register over a stable band —
  the "talk" is one voice jumping while the band plays on.
- MOTION: parallel convergence — the ladder sections move inward toward the
  mids from above and below, never crossing. Order is the acid identity and
  must hold at every wheel position. The register-jumper adds oblique drama
  without breaking the ladder.
- Q ATTITUDE: the SUB owns the Q axis — S1 gets the whole budget (strong
  positive push). The mid sections stay flat or slightly negative (Q cleans
  rather than screams). Anti-scream: more Q = more sub squelch, not more
  mid pain.
- KEY TRACKING: poles tuned to harmonic intervals over a root. The KEY macro
  shifts the frequency datum so the ladder chases the note — the one thing
  the ROM couldn't do. Author at a reference root and let KEY handle the rest.
- AUTHORING: author the sub + harmonic ladder at one root note. Null against
  BassBox-303 for the ladder spacing, TB-OrNot-TB for the register-jumper
  behaviour. Daylight: KEY-track it and test against a real 303 pattern.

### HiFi to Radio — RadioCraze species [EQ-]

The lo-fi flip. "How cheap" on the wheel, "how broken" on Q.

- POSE VOCABULARY: M100 = a real junk speaker measured — band-edge zero
  pairs that kill sub and air, a mid honk pole at r=1.0 where the cheap
  driver rings, interior scale dips that fake the driver's natural scoop.
  M0 = full-range (flat-ish, no band-limiting). The two poses are the same
  spectrum at different fidelity.
- MOTION: collapse from both ends — the sub and air zeros migrate inward
  as the wheel turns toward M100, the mid honk grows, the scale dips deepen.
  The frame closes like a fist.
- Q ATTITUDE: NEGATIVE on the band-edge sections. More Q = the zeros bite
  harder, the band shrinks further, the honk gets nastier. Q makes it
  cheaper, never sweeter. The only filter where Q degrades the signal and
  that IS the feature.
- SOURCE: measure a real junk speaker for the M100 pose — a phone speaker,
  a laptop driver, a cheap Bluetooth pill. The reference is a physical
  object, not a ROM.
- AUTHORING: capture the junk speaker's response (the fitter's ARMA endpoint
  path works — this is one measurement, not a sweep). Author M0 as the
  full-range opposite. Wire Q negative on the edge sections. No null against
  RadioCraze needed — the reference is the speaker you measured.

### Speaker Knockerz — Ace of Bass species [EQ+]

The bass fattener. A fader between two opposite tonal verdicts.

- POSE VOCABULARY: two OPPOSITE verdicts of the same spectrum. M0 = the
  anti-bass pose — a real pole pair pinned at DC with deep scale cut, zeros
  parked high (dark, hollowed, bass scooped out). M100 = the boost pose —
  poles respread across the full band at gentle radii, scale near unity, the
  top re-opened. Same frequency geography, opposite gain geography.
- MOTION: polarity ride — the low shelf swings cut→flat→boost while the top
  re-opens. Travel is all downward (the frame collapses onto the bass, never
  slides up). Every section keeps its FREQUENCY job; only the level
  geography flips.
- Q ATTITUDE: almost nothing. One reserved bloomer in the top section at
  +0.15 — a hint of air as you push. The wheel is the star; Q is a garnish.
- AUTHORING: author the cut pose and the boost pose as two separate tonal
  verdicts, not two positions of one sweep. Null against Ace of Bass to
  confirm the polarity-ride behaviour. Daylight: push the cut deeper and
  the boost hotter than the ROM dared.

### Rumble to Scream — EarlyRizer species [LPF]

The classic riser. Hot Q everywhere, bass floor held by handshake.

- POSE VOCABULARY: M0 = the bass floor held by a pole/zero HANDSHAKE — one
  section's zero sits at the bass frequency, another section's pole sits
  just above it, together holding the low end without a shelf. Top closed
  by gentle-r high poles (not ringing, just present). M100 = the floor
  handed up — the low section releases, the former bass pole becomes the
  new air, the top opens.
- MOTION: anchor relay — one section leaps upward (+5 octaves) while the
  sections above it fall. The low pole hands its job up the stack and
  becomes the new air voice. ONE relay, not a full-frame sweep. The
  handshake IS the character.
- Q ATTITUDE: uniform gentle positive everywhere (+0.04..+0.18). Classic
  "hot Q" — every section lifts a little, no section defuses. The feel is
  analog and predictable at every wheel position.
- AUTHORING: build the handshake pose first — tune the pole/zero pair so
  the bass floor is held flat within ±3 dB to 80 Hz. Build the relayed
  pose second where the same pole has risen to the air band. Null against
  EarlyRizer for the relay law. Daylight: uniform Q across all six stages.

### Clean to Razor — RazorBlades + LucifersQ species [EQ-/REZ]

The screech. Zeros are the melody; violence is the feature.

- POSE VOCABULARY: M0 = the blade set — four zeros parked at specific
  frequencies, each with a pole at r≈1.0 sitting BESIDE it to sharpen the
  cut edge. The zeros are the melody; the poles are the edge-sharpeners.
  One section starts as a real pole (r well below 1.0) — invisible, a
  sleeper. M100 = the blade set slid downward, and the sleeper has woken
  into a complex mid resonance.
- MOTION: the blade set slides down together (parallel, −2 to −3 octaves)
  — which bands bleed shifts as the wheel turns. Meanwhile two sections
  execute the set's most violent register swap: one flies up, the other
  crashes down, 8+ crossings. Maximum turbulence into a nasty landing.
- Q ATTITUDE: NEGATIVE on the blade sections — more Q = deeper cuts, the
  zero wins over the pole. POSITIVE on the sleeper section — Q births a
  ring where there was none at Q0. The danger zone (40–90 on the Q wheel)
  is the sleeper snapping into resonance while crossings are in flight.
- AUTHORING: author the blade-set pose first (zeros as melody, poles as
  edges). Author the sleeper's wake-up on Q100. Null against RazorBlades
  for the cut geometry, LucifersQ for the crossing violence. Daylight:
  push the sleeper's Q arc further — the feature IS the danger.

### Opium — Bat Phaser species [PHA]

The drift. Permanent moving phase, ringy Q.

- POSE VOCABULARY: two pole-zero pairs, each a pole-riding-zero — the zero
  sits on its pole's shoulder at r=1.0, creating a moving notch that rides
  the ringing pole. M0 = both pairs in the low-midrange (~100–250 Hz), the
  zeros just above their poles, the notches shallow and slow. M100 = both
  pairs risen to the high-midrange (~6–12k), the zeros still riding,
  the notches deeper and faster.
- MOTION: both pairs travel upward in parallel — two notches sweeping the
  spectrum together, never crossing, the spacing between them widening as
  they rise (the lower pair travels further). The phase cancellation
  between the two ringing poles creates the permanent drift — the beat
  frequency between them shifts with the wheel.
- Q ATTITUDE: uniform radius push on both stages. Q takes the poles from
  already-hot (~0.998) to near-unity (~0.9999) — the notches deepen, the
  ringing extends toward infinite, the drift becomes more pronounced.
  There is no defuser; both stages arm together.
- STRUCTURE: two active stages. The remaining four are identity — this is
  a 2-stage phaser, not a full cascade. The simplicity IS the sound.
- AUTHORING: place two pole-zero pairs in the low-midrange, zeros riding
  ~0.1–0.3 octaves above their poles at r=1.0. Q pushes both pole radii
  toward unity. Null against the X3F_bat_phaser capture to confirm the
  pole-riding-zero geometry. Daylight: tune the pair spacing to a musical
  interval so the beat frequency sings at a consonant rate.

### Uh to Oh — DeepBouche species [VOW]

Dark round French vowels. Descending formant stack, throat not lips.

- POSE VOCABULARY: a downward formant stack — sections ordered high→low
  (the opposite of every other vowel body). The sub section carries a deep
  scale cut (−18 dB) so the bottom reads as throat resonance, not lip
  rounding. All radii hot (r=0.97–1.0) — a dark face fully armed. M0 =
  'ou' (deep back vowel). M100 = 'est' (fronted, but still round).
- MOTION: nearly static except ONE articulator — a single formant fronts
  by a minor third to a fourth while the throat holds. The face stays; the
  mouth moves. Oblique in its purest form.
- Q ATTITUDE: essentially zero everywhere. The vowel is already at full
  tension at Q0 — Q would break the face by pushing radii past the
  stability ceiling. E-MU left it alone; so do we. This body has no Q
  travel by design.
- SECTION ORDER: descending (S1 = highest formant, S6 = throat). This is
  the architectural signature of the species — reverse the order and
  it stops being DeepBouche.
- AUTHORING: build the 'ou' face first (descending formant stack, cut sub
  scale, hot radii). Front one formant for the 'est' pose. Null against
  DeepBouche to confirm the descending order and the one-articulator law.
  Daylight: author a second round vowel ('eu'→'oe') as a companion body
  using the same descending architecture with different formant targets.
