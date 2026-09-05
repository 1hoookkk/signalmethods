# CARICATURE: THE MORPH PAST ITS END

Spec, 2026-09-05. Tyson: "Yes to morph past the limit. Make it visual and out of bounds."
A caricature pushes every feature away from a norm by one factor. The morph already is
that push between two frames. Let it run past 1 and past 0, on the surface, on the
wheel, and in the plugin's own words.

## The maths, which is nothing new

The hardware morph is `w = a + t (b - a)` on every word. Nothing in that formula stops
at 1. At t = 2 every difference from a is doubled; at t = -1 the sound is the anti-b,
pushed the other way. The only thing to guard is the pole radius, which is clamped at
0.9995 after the lerp so nothing rings forever. Zeros may pass the circle; a zero
outside it is only a minimum-phase choice and the response stays finite.

Frames are slot paired, so the push is row by row: pitch distance in semitones times t,
width divided, amount multiplied. Row 6 is exempt: it stays the ceiling of frame b at
any t. Level follows the unity rule at any t.

## Where it lives

- PAIR on the surface. The pair edge is drawn cyan between the two nodes. It continues
  past both ends as a cyan line to t = -2 and t = 3, drawn thinner, ending in a short
  perpendicular tick at each limit. The live mark can be dragged the whole way. Past an
  end the status line reads `Hedz M0 > M1  1.62  caricature`; before the start it reads
  `anti`. Nothing else on the surface changes, because the overshoot is off the surface
  by definition.
- The wheel in the body block. The square gains a margin of one square's width on every
  side, drawn as a fainter rule. MORPH and Q run from -1 to 2 inside that margin. The
  yellow mark is allowed out of the square. Inside the square the sound is the plugin's
  wheel; outside it is the same bilinear formula, unclamped, with the radius guard.
  The M and Q readout shows the value, `M 1.40  Q 0.60`, and turns cyan when either is
  outside 0 to 1.
- The timeline. Keys may sit at out-of-bounds positions. The path draws past the
  square's edge the same way.

## Out of bounds, drawn

The out-of-bounds region is drawn, never hidden and never implied:

- On the surface, the extension lines with end ticks, cyan, 1 px, thinner than the
  edge. The region between t = 1 and t = 3 has no fill and no hatch. The tick at the
  limit is the only mark.
- On the wheel, the outer square is the limit. The inner square stays the body. The
  margin is empty black with a fainter rule.
- On the response panel, the curve is drawn as always. Where the caricature has driven
  a peak above +30 dB or a valley below -30 dB the curve leaves the frame at the top or
  bottom and a small cyan tick sits on the frame edge at that frequency, so the excess
  is visible without changing the fixed grid. The grid never rescales.
- On the ARMAdillo, poles that hit the radius guard are drawn as a cyan square instead
  of a white one. Zeros past the circle are drawn outside the rim, on the same angle,
  at the radius they have.

## The norm

A caricature needs a norm at corner 0. Three ways to get one, all through what exists:

- A frame from the tray: the schwa in VOWELS for voices, the group mean for the rest.
- The GROUP MEAN key in the tray header: adds one frame, the mean words of that group,
  named `P2K mean` and so on, as a capture.
- The far corner of the body, when the body is already a norm-to-sound pair.

So a voice caricature is: read the voice in SOUND, FRAME, PAIR it with `schwa`, drag
past the end. Three gestures.

## In the plugin

Nothing on the face changes. The ship body is four corners and the wheel runs 0 to 1.
A caricature is exported by baking: EXPORT at t writes a body whose corners are the
pushed frames, so the wheel then runs between two caricatures, or between the norm and
the caricature, whichever four corners you set. The push happens in the workstation,
the plugin plays bytes.

## Acceptance

- At t = 2 every pole's semitone distance from frame a is twice its distance at t = 1,
  within the word quantisation, for slot-paired conjugate poles.
- No pole radius exceeds 0.9995 at any t in -2 to 3.
- At t = 0.5 the words equal the plugin's interpolate_words at 0.5, word for word,
  so nothing in bounds has changed.
- A body exported at t = 1.5 loads in the dev roster and passes the containment test.
- The response panel's frame never rescales; the excess tick appears when and only
  when the curve leaves the frame.

## Build order

1. PAIR t unclamped to -2 .. 3, radius guard, status words. One line and a clamp.
2. The extension lines and ticks on the surface.
3. The wheel margin, unclamped MORPH and Q, the readout colour.
4. Excess ticks on the response panel, guard marks on the ARMAdillo.
5. GROUP MEAN and the schwa frame in VOWELS.
