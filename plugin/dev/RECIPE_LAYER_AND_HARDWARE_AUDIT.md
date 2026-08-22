# HARDWARE-LAW AUDIT + THE RECIPE LAYER — the next pass
(2026-08-09, from the patent/hardware findings; execute after the de-hack lands)

## A. The checklist, mapped to OUR code — exact fixes

1. DC GAIN ANCHOR (US 5,170,369: unity DC gain; a0 isolated from resonance
   coefficients).
   OURS: SCALE is the isolated per-section gain word (kernel c4 multiplies
   b-row only - a0-style isolation already true by construction).
   FIX-SPEC: add a certify check in the compile path: decoded DC gain of
   each authored body at all 4 corners reported, and the LINTER warns when
   low-frequency passband buildup exceeds the historical envelope. Verify,
   do not invent: measure DC gain of all 33 references first; their spread
   IS the lawful envelope.

2. SATURATED MODULATION SUMMING (fixed-point, hard clip 0..1).
   OURS: PluginProcessor clamps every mod sum with jlimit(0,1) before the
   engine; MorphMod offsets are additive around the wheel then clamped.
   FIX-SPEC: verification only - one test that drives depth+follow+wheel
   to extremes and asserts the engine never receives out-of-range morph/q.
   No soft knees to add anywhere - saturation is the law.

3. DYNAMIC FEEDBACK AGC.
   OURS: exists and is the shipped voice (AGC_DRIVE=1.8, trench_agc_table
   FFI). FIX-SPEC: none. Do not add any downstream master peak clamp in
   compile or plugin - the AGC knee is the only dynamic authority.

4. STRICT 240-BYTE CONSTRAINT.
   OURS: asserted everywhere (probe, exporter, loaders). FIX-SPEC: none.

5. AUTHORED PEAK ENVELOPE (~40 dB) - "clamp authored static peaks so
   normal signals ride the AGC knee".
   OURS: the display envelope is already +/-40 dB (curveDbTop=40) and the
   linter has a Q-attitude axiom.
   FIX-SPEC: make it a certify REFUSAL, not a display convention: any
   corner response peaking above the measured corpus envelope refuses
   loudly at compile with the offending lane. Measure the corpus first
   (max corner peak across the 33) - that number, not 40, is the law.

6. ACTIVE LANE PERSISTENCE (lock each section's role across corners;
   prevent root-crossing phase cancellations).
   OURS: already house law - correspondence-not-frequency-sort
   (STAGE_LAW), fixed lane numbers in every trace, S6 terminal locked,
   recipes forbid zero crossings.
   FIX-SPEC: mechanize it - a certify check that no lane's pole path
   crosses another lane's zero path through the ENCODED interpolation
   (sample the real word-lerp, not straight lines; runtime_probe already
   does this). Refuse with the crossing pair and morph position.

7. NYQUIST/ORIGIN ZERO PARKING (all-pole ladders: zeros at origin r=0 or
   parked out-of-band, mathematically inert).
   OURS: the bypass operation exists in the library grammar; the corpus
   parks S6 zeros out-of-band 132/132.
   FIX-SPEC: the recipe format gets an explicit `zeros: parked` per-lane
   declaration that compiles to the exact parking convention (origin r=0
   for ladders, out-of-band unit for terminals) - never hand-typed words.

## B. THE RECIPE LAYER (the authoring surface - owner-approved direction)

The stack: words (truth) -> library rows (vocabulary, filters/
pose_library.json, 4-word geometry cells at datum) -> seat/wire grammar
(mechanics, strictly enforced) -> RECIPES (the surface Tyson edits).

Recipe file v1 (filters/recipes/*.json): named poses (by library row
reference or measured capture), per-lane operations (seat / wire /
stack / re-pair / bypass / terminal), Q attitude (explicit Q100
frequencies - never "same Hz more radius", per ARMAdillo law 1), motion
constraints (no crossings), scale seasoning per placement, and the
X-to-Y name. One compile step: recipe -> seats/wires -> plan_body ->
body240 -> certify (checks A.1-A.7) -> trace -> null vs reference ->
distance gate.

The bench face becomes the recipe editor with live audition; cell
machinery stays underneath, visible on demand. Tyson picks every pose by
ear - the endpoints law is untouched.

## C. THE VISUAL SHELF (owner's law, 2026-08-09: "at the biquad level they
are visualised. thats how we pick")

The library shelf renders every row as a CARD: the SOS's own small
response curve at its datum (computed through runtime_probe's stage math,
never re-derived), with its pole/zero marks in the ARMAdillo log-polar
plane and the id (p<hz>-z<hz>) as the caption. Picking is by eye - the
shape IS the browse key; the id is only how recipes write the choice
down. Sort axes stay measured facts (pole Hz / radius / reuse /
provenance). No thumbnails of made-up meaning: the card shows the curve
the runtime would produce, nothing else.
