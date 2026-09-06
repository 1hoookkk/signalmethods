# HEADSPACE

The authoring tool for TRENCH bodies. A JUCE app in the plugin's CMake tree, built alone by the
`headspace` preset, linked straight to `native/core`. Everything painted by hand.

## The workflow, in Tyson's terms

A body is four sounds. The tool is for choosing them and hearing what the chip does between
them. That is all it does.

1. The screen is the vowel chart: F1 down, F2 across, the twelve Klatt 1980 vowels as letters,
   and every factory corner as a star at its own F1 and F2. Captures are stars too.
2. Hover a star and you hear it, exactly, its own thirty words. Sounds are picked by ear from
   the map, never by name from a list.
3. Pin four stars to M0 Q0, M1 Q0, M0 Q1 and M1 Q1. Press 1 to 4 over a star, or click a chip.
   Every pin is chosen. Nothing fills in a partner for you.
4. The puck inside the quad is the plugin's MORPH and Q. What plays is
   `PackedBody::interpolate_words` of the four pins, the chip's own lerp of the words. Nothing
   fits, leads, matches, normalises or re-scales on the way from a pin to the ear.
5. Ctrl+S keeps what you hear as a new star, with its parents and its position. That is the
   point of the tool: the in-betweens and the slight nudges, kept exactly and pinned again.
6. W writes the four pins as 240 bytes, `legacy_bytes`, the factory layout. A factory body
   pinned at its four corners writes its own bytes back. The written file, reloaded through the
   plugin's lerp, equals what the puck played.

## How to read this, so it is not misread again

- Six sections, all active, in whatever role the sound needs. Not six bells. Not F1 to F6 in
  row order. Real pairs are legal.
- The tool measures nothing and warns of nothing. What is on the screen is the words, drawn.
  Peaks stack in a serial cascade because responses add in dB; the factory ships that, and the
  ear decides whether to keep it.
- The map is where sounds sit, not how they blend. Only four sounds ever blend, and only the
  chip's way. No lattices, fields, kernels or many-way blends.
- Q corners are chosen like any other corner. The factory made them by copying and re-voicing
  by hand; that is the row editor's job later, not a formula here.
- Word space is the whole truth. A sound is thirty integers. The file is the words. The plot is
  the words drawn.
- Magnitude plots are 3:2 boxes on the fixed grid, 30 dB either side of a hard 0 dB line.
  Never a wide strip, never auto-scaled, never a frame above the ruled range.
- Words: star, pin, puck, keep, write, body. Not column, hop, lattice, field, anchor space.
- The smallest end-to-end surface first. Stop when a slice adds a layer the ear cannot reach.

## Screen

Header with PLAY, SAW, PINK NOISE, WRITE BODY FILE. Four pin chips. The chart with the quad,
the puck, halos on the pins scaled by their bilinear weight, and a quiet red tint on cells
where the lerp's peak rises more than 3 dB above the corners. A 3:2 response box with the
two MORPH ends at the current Q as ghosts under the live curve, the four lowest formants and
the peak level as numbers. One status line.

## Keys

Arrows move the puck by 1, with Ctrl by 0.2. 1 to 4 pin the hovered star. Drag a pin onto a
star. Shift-drag inside the quad moves all four pins. Ctrl+S keeps. Delete removes a selected
capture. Ctrl+Z and Ctrl+Y undo and redo. W writes. Space plays.

## Files

`Source/app/Quad.*` the model. `Library.*` the 132 factory corners as stars. `Session.*` hover,
pins, captures, undo, keys, `banks/HEADSPACE.quad.json`. `Audio.*` the device callback into
the core cascade through a lock-free slot. `Screen.*` the painting. `Tests/QuadTests.cpp` the
acceptance, headless, rendering `artifacts/shots/headspace.png` without a window.

## Later, each only when Tyson asks

The row editor, six parametric bands per corner, which is E-mu's own manual mode. A note in
with FOLLOW so a tracked body can be judged. Transpose in word space. Reordering a corner's
rows to a neighbour's. The reader, averaged across notes for a fixed formant, per note for a
tracked one. A second map by the three principal components of the factory corners.
