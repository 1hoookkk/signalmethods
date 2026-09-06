# HEADSPACE

The authoring tool for TRENCH bodies. A JUCE app in the plugin's CMake tree, built alone by the
`headspace` preset, linked straight to `native/core`. Everything painted by hand, one sans serif
font, warm grey ground, charcoal plots, muted blue and sand accents. The window resizes and the
surface is drawn from its size.

## The workflow, in Tyson's terms

A body is four sounds. The tool is for choosing them and hearing what the chip does between
them. That is all it does. The surface is four rooms, one at a time, with the body and the
audition strip always in view.

1. Picker (1). The vowel space on the left is for vowels only, drawn the phonetic way: F2 across
   from high on the left to low on the right, F1 down from close at the top to open at the bottom,
   so i sits top-left, u top-right, a at the bottom. The 12 Klatt 1980 vowels and schwa are the
   landmarks, named by symbol; the 48 Hillenbrand 1995 medians are small unnamed dots behind
   them, named when lit. Click a vowel to hear it exactly. Click empty space and a vowel is made
   at that F1 and F2 and plays; drag to move it. Shift-drag a vowel and it transposes: its first
   formant is the anchor, every other row goes by the same ratio keeping its width in semitones,
   row 6 left as the ceiling; it plays as it moves. The palette on the right is for everything
   that is not a vowel, in three tabs: Bodies, the 12 measured impulse responses in
   `evidence/measured-bodies/ir_library` (violin, ukulele, upright piano, kalimba, china cymbal;
   sweeps skipped), each read at start the same way a dropped .wav is read; Reads, dropped .wav
   files, order-12 LPC at 11,025 Hz, each pole paired with a zero on its own angle, row 6 the
   ceiling notch, then `unityDc`; Captures, what Keep as card or Ctrl+S kept. Each card shows its
   name, its F1/F2 and its curve; none of them is ever drawn on the vowel space.
2. Stage (2). One corner's rows. The 3:2 magnitude plot on the fixed frame, +30 to -30 dB, hard
   0 dB line, 20 Hz to 20 kHz; numbered handles on the peaks, drag one and the row's frequency
   and gain codes follow the plot's axes. Under it the six rows: number, type, note and cents,
   semitones above the saw's note, width in semitones, peak dB at the note. Row 6 is CEILING.
   Click a type to cycle Rest, Peak, Notch; drag a number to step its code. H shows the raw Hz
   and radius words under each row. A B C D at the top pick the corner. Editing a vowel makes a
   capture named by its formants and puts it in that corner; the vowel is untouched.
3. Perform (3). The pad is the plugin's MORPH and Q. Drag anywhere on it. What plays is
   `PackedBody::interpolate_words` of the four corners, the chip's own lerp of the words. Nothing
   fits, leads, matches, normalises or re-scales on the way from a corner to the ear.
4. Cube, last, the explicit Morpheus mode. Eight name boxes, one per corner of the chip's
   three-axis lerp: 1 to 4 the front face, 5 to 8 the back, MORPH across, Q up, depth the rail.
   Click a box to choose its card; click its number to open its rows. The plane sits at the
   depth; press on the plane to move the point, and what plays is `interpolate_words` of the
   eight at that point. Drag the point onto a cell of the body to keep that sound there. Use
   these four puts the plane's four corners into the body at once and goes to Perform.

Always in view:

- The body, bottom right, a 2x2 of the four corners: A top-left is M0 Q1, B top-right is M1 Q1,
  C bottom-left is M0 Q0, D bottom-right is M1 Q0. Each cell draws its corner's words. Click a
  cell's name to choose from the palette; click its plot to open its rows in Stage. Drag a card,
  a chart point or the cube point onto a cell to put that sound there; drop a .wav on a cell to
  read it straight into that corner. W, or the write key on the body, writes the four corners as
  240 bytes, `legacy_bytes`. The written file, reloaded through the plugin's lerp, equals what
  the pad played.
- The audition strip, bottom: Play, Saw with its note, Noise, Loop; Track; > A > B > C > D put
  what plays into that corner (with the pad playing they keep the puck's sound first); the
  status; the three-octave keyboard. A key, clicked or from MIDI, strikes: a 10 ms burst into
  the cascade so a body rings at its own resonances, then the source held while the key is down,
  then a ring-down on release. Space drones without a key. Track (K) transposes what plays with
  the key, every row by the ratio of the note to A2, so the cascade can be played as a tuned
  resonator; off, the rows stay at their own Hz and the note's harmonics sweep through them.
  Track changes what plays, never the card or the file.

## How to read this, so it is not misread again

- Six sections, all active, in whatever role the sound needs. Not six bells. Not F1 to F6 in
  row order. Real pairs are legal.
- The tool measures nothing and warns of nothing. What is on the screen is the words, drawn.
  Peaks stack in a serial cascade because responses add in dB; the factory ships that, and the
  ear decides whether to keep it.
- Only the chip blends, and only its way: four corners on the pad, eight in the cube, two on a
  pair. A capture is the exact words that were playing.
- Word space is the whole truth. A sound is thirty integers. The file is the words. The plot is
  the words drawn.
- Words: room, corner, card, point, cell, keep, write, body. Not column, hop, lattice, field, star.
- The smallest end-to-end surface first. Stop when a slice adds a layer the ear cannot reach.

## Keys

The tabs, or F1 to F4, change room. Arrows move the puck by 1, with Ctrl by 0.2. 1 to 4, or A B C D, put the
playing sound in that corner. Shift-drag a chart point transposes it by its first formant. Ctrl+S keeps. Delete removes a selected capture or read. Ctrl+Z and
Ctrl+Y undo and redo. W writes. Space drones. S saw, N noise, L loop. K tracks the key. [ and ] step the saw's note
by a semitone, Page Up and Page Down by an octave. H shows the raw words in Stage. Escape closes
a menu or the rows.

## Files

`Source/app/Quad.*` the model: cards, the four corners, the eight-corner cube, the puck, the
lerp, the row grammar, the file, the pack. `Library.*` the vowel banks, the measured bodies, the made
vowel, the wav reader. `Session.*` selection, corners,
cube, captures, reads, undo, keys, `banks/HEADSPACE.quad.json`. `Audio.*` the device callback
into the core cascade through a lock-free slot, the saw, the noise, the loop, MIDI in.
`Screen.*` the four rooms, the body, the strip, the painting; card curves are cached by their words so paint does no filter math for a card it has seen. `Tests/QuadTests.cpp` the
acceptance, headless, rendering `artifacts/shots/headspace*.png` without a window.

## Later, each only when Tyson asks

More basis cards: Peterson and Barney 1952, the published modal tables. The flat grid of bodies sharing edges. FOLLOW, key to MORPH in the plugin. Reordering a corner's rows to a neighbour's. The
averaged-spectrum reader for fixed formants.
