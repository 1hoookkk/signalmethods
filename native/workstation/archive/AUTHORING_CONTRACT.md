# The authoring operation: product and state contract

Supporting `AUTHORING_DECISION.md`. The maximum UI scope. Research findings authorise no
control that is not listed here.

## The reduced surface

Product thesis: one working sound, one audible interpolation, four explicit destinations.
Workflow: choose an origin, choose a destination, move between them, assign the exact result
to A, B, C or D, audition the body, export. The screenshot of 2026-09-08 is evidence, not a
layout to keep. Removed from the visible workflow, implementations untouched: the eight-vertex
FIELD, Z, MASK, Q-LIFT, BAKE SLICE, RELAX, TRANSPOSE, the permanent row table, the full-width
keyboard, the shortcut legend.

```
+------------------------------------------------------------------------------+
| HEADSPACE            sounding: BLEND  Talking Hedz M0Q0 -> Aud Bell 1 C3  8%  |   warm pale grey
+------------------------------------------------------------------------------+
|                                                                              |
|   charcoal response display: the sound that is sounding, one orange curve    |
|   origin and destination as thin grey ghosts while a blend is sounding       |
|   axes 20 Hz to 20 kHz, +30 to -30 dB                                        |
|                                                                              |
+------------------------------------------------------------------------------+
| ORIGIN  [ Talking Hedz M0Q0     v ]   o=======◆===============o   DESTINATION |
|                                       0%       8%         100%   [ Aud Bell 1 C3  v ] |
+------------------------------------------------------------------------------+
| BODY                                                                          |
|  [A  i           ] [B  u           ]     MORPH/Q pad     [ Undo ] [ Export ]   |
|  [C  Talking...  ] [D  empty       ]        ( )                                |
|  each slot: name, small curve, buttons:  Hear   Put here                       |
+------------------------------------------------------------------------------+
| ■ pluck  saw  noise  loop   |  ▶ Play   ■ Stop   |  OUTPUT ──────o── -6 dB     |   dark strip
+------------------------------------------------------------------------------+
```

Every visible control and the decision it enables:

- ORIGIN chooser: which sound the work starts from. Opens the chooser; choosing sets the origin
  and makes the origin sound.
- DESTINATION chooser: what the sound moves toward. Choosing sets the destination and leaves
  the sounding state unchanged at 0 percent.
- Blend slider with percent: how far to move. The only interpolation control. The chip's word
  lerp of two complete 60-byte corner states, `interpolate_words` on a two-corner body, no
  stripping of zeros, no gain normalisation, no section reordering, no datum change.
- Slot A to D, "Put here": commit the sounding state into that shipping corner, byte-exact.
- Slot A to D, "Hear": audition that corner without changing anything.
- Slot letter or name: select the slot as the working target for the next "Put here" and show
  it in the display as an audition. No write.
- MORPH/Q pad: audition the plugin's own lerp of the four corners. No write.
- Excitation words: which source excites the filter. Play, Stop. OUTPUT: final level only.
- Undo: reverse the last commit or chooser change. Export: write the 240 bytes of A to D.

State contract. One authoritative audible state, `sounding`, is a tagged value:
BLEND(origin, destination, t), AUDITION(corner or card), or PAD(morph, q). PAD is the plugin's
own lerp of the four slots at the pad's position; it is an audition, never a source for "Put
here", and its morph and q are saved with the session so the pad reopens where it was. Its name in the header, the
orange curve in the display, the words in the audio slot, and the words "Put here" writes are
the same 30 words, computed once in Session from the same call. Entering an audition: the
BLEND is kept, not lost; the header reads "hearing A" and the curve shows A; nothing is
written. Leaving an audition, by Esc, by touching the slider, or by choosing: the BLEND
returns exactly as it was, same words, through the runner's glide. Nothing ever writes a corner
except "Put here", Undo, or Export, and Export writes only what the four slots show. Choosing a
slot to edit, hearing, and assigning are three actions with three controls.

Persistence: origin, destination, t, the pad's morph and q, and the four slots are saved in `HEADSPACE.quad.json`
and restored to the same words. Excitation and level are saved as preferences.

Change set, smallest: (1) Session: replace `plays` with the tagged `sounding`; make PUSH the
blend with origin selectable, not only "what plays"; make the pad and Hear pure auditions that
never touch `quad`; persist origin, destination and t; (2) Bridge: state carries `sounding`,
`origin`, `destination`, `t`, `slots`; dispatches `setOrigin`, `setDestination`, `setT`,
`hear`, `put`, `stopHearing`; (3) page: the layout above; (4) fix `Field::setRadius` per
section 2, which the blend does not use but the test suite must not enshrine.

Acceptance, mechanical, in `trench_quad`: audible and visual agreement, the words published
to the audio slot equal `sixOf(sounding)` and equal the curve's source at every state change;
reversible assignment, Put then Undo restores the slot's previous star index and words;
save and reopen fidelity, origin, destination, t and slots round-trip through the JSON with
identical words; byte-exact export, the written 240 bytes equal `bodyOf(slots).legacy_bytes()`
and reload to the same four corners; audition isolation, Hear and the pad change no slot and
the BLEND returns bit-identical. Listening acceptance is separate and is the experiment in
section 3.
