# HEADSPACE, the strip

Tyson, 2026-09-06: the space is the authoring method. It plays one big packed-word preset
whose corners lerp 0 to 100 MORPH, every morph equal. The surface-and-pad screen is replaced.
The bridge, the lerp, the audio engine and the tests underneath stay.

Words used here, and nothing else: a state, a column, a square, a morph.

## From the filter up

1. The filter is six second-order sections in series. Gains multiply, responses add in dB.
   A P2K corner uses all six, always: of the 792 factory sections, 710 are peaks under an
   octave wide, 64 are broad poles, 3 are tilts, 15 are real pairs, none are off.
2. A section is five 16-bit words: pole, zero, gain, as ARMAdillo codes. The pole frequency
   word is linear in octaves, the resonance word in dB of peak height, the gain word in dB.
   A state is 6 x 5 words, 30 integers. That is the whole truth of a sound.
3. The chip lerps the words linearly. Halfway between two states every pole has moved half its
   semitones and half its dB. Measured over every factory pair: DC unity survives the lerp,
   0.7 dB off at the median, 5 dB at worst. Peaks do not: in 23 percent of steps the
   response mid-morph rises more than 3 dB above either end, and Zoom Peaks rises 37 dB,
   because a zero that hides a pole at one corner has moved off it halfway. That is what a
   morph sounds like, and it is heard and kept or not.
4. A body is four states and the bilinear word lerp between them, MORPH one way, Q the other.
   `wheelMorph` in range is exactly this and equals the plugin's `bodyLerp` word for word.
5. The strip is bodies sharing edges. Two rows of states, Q0 and Q1. A column is a state and
   its Q partner. Two neighbouring columns make a square, and a square is a body. The morph
   is the MORPH 0 to 100 between two neighbouring columns. The position is a square, a MORPH
   inside it and a Q. What plays is that square's lerp there. Nothing else exists.

## What you do

- The library is the 132 factory corners, "Body cN". Click one to hear it exactly.
- Place it in a column. A factory corner brings its Q partner into the row above (c0 with
  c2, c1 with c3).
- Walk: Left and Right move MORPH by 1, Ctrl by 0.2; Up and Down move Q the same. Past 100 is
  the next square at 0. Dragging on the strip does the same. Space plays.
- Ctrl+S keeps the column: both rows at this MORPH, as C1, C2, ... It is inserted where you
  stood, between the two columns it came from, and is a state like any other.
- [ and ] move the selected column. Delete removes it. Pair distant states by moving one next
  to the other.
- W writes the square you stand in: its four states, raw words, to 240 bytes in
  `plugin/presets/user/`. What you write is what you walked.

## Screen, always the same four things

- The navigator: the whole strip as one bar, one tick per column, the mark on it. Always
  visible however long the row gets. Click it to jump.
- The squares: the square you stand in and its neighbours, two rows, each its response on
  the fixed grid, 30 dB either side of a hard 0 dB line, names under the columns, the mark
  at its MORPH and Q.
- The live response below on the same grid.
- The column list: every column in order, name and origin, the selected one marked. Always
  visible. The library list beside it. One status line under everything.

## Workflow

- Keyboard first. Every action is one key, no dialogs, no modes. Tab moves selection between
  the column list and the library. 1 to 9 jump to that column, Home and End to the first
  and last. Enter places, Ctrl+S keeps, W writes. Audio is sent before anything is drawn.
- State is one struct: columns, position, selection. Every gesture is a function from that
  state to the next; the screen draws from it; the file is it serialised. Ctrl+Z undoes and
  Ctrl+Y redoes any edit of the strip, unbounded within the session, and puts the mark back
  where the edit was made.
- The keys live in `toolbox/keys.json` and can be rebound. Layout proportions are saved with
  the strip. One screen, no floating windows.

## Open

- Tyson to rule: a key that reorders the selected column's rows to its left neighbour by
  nearest pole, words untouched, sound at the column unchanged, never automatic.

## File and bridge

- `banks/HEADSPACE.strip.json`, `trench-strip-v1`: `columns[]`, each `name`, `origin`
  (factory body and corner, or capture parents and MORPH), `q0` and `q1` as 30 integers.
  Words verbatim both ways; never an empty array, write null. Saved on every change.
- Bridge: `wheelMorph` as is. New `cellBytes(corners)` and `writeCell(path, corners)` pack the
  raw words. `bodyCorners`, `lead` and `unityDc` are not on any path from placement to file.

## Acceptance, headless

1. Keep then recall: the capture's words equal the live words at its position, both rows.
2. Save then reopen: order, names, origins and words identical.
3. Write then reload: `bodyLerp` of the file equals the live words on a 5 x 5 grid per square.
4. Factory round trip: a body's c0 and c1 placed as neighbours write its original bytes.
5. Every state in the strip has six active sections; a real pair passes, an identity section
   is refused.
6. n columns make n - 1 morphs; MORPH 100 of the last is the last column exactly.
7. `trench.shot` renders the strip to `artifacts/shots/headspace.png` without a window.
8. Undo: a sequence of place, keep, move and delete, then undo to the start, restores the
   strip exactly; redo to the end restores the sequence.

## Refused

Voice leading, section matching, cancelling pairs, recompilation, unity DC or any gain rule
on the path. Fields, lattices, blends of more than four states, fitting on capture, Q by
formula, verdicts on a morph, a third axis. The plugin has MORPH and Q; the row is the rest.

## Order

Bridge raw write with tests 3 and 4. Strip model and file with 1, 2, 5, 6. Screen with 7.
Then Tyson's test by ear: a corner, a far corner, a slight nudge, keep, build on from it.
Later and separately: cut `bodyCorners`, `lead`, the stitch model and the unused bridge
commands; the row editor, which is the patent's manual mode; the averaged-spectrum reader.
