# HEADSPACE

The authoring tool for TRENCH bodies. A JUCE app in the plugin's CMake tree, built alone by the
`headspace` preset, linked straight to `native/core`. Everything painted by hand, one monospace
font, black ground, green ink. The window resizes and the surface is drawn from its size.

## The workflow, in Tyson's terms

A body is four sounds. The tool is for choosing them and hearing what the chip does between
them. That is all it does.

1. The stage is the body. Its four corners are the four sounds: A top-left is M0 Q1, B
   top-right is M1 Q1, C bottom-left is M0 Q0, D bottom-right is M1 Q0. Each corner has a name
   box; click it and choose from the palette. PRESET at the top loads a factory body into all
   four.
2. The puck is the plugin's MORPH and Q. Drag it anywhere on the stage. What plays is
   `PackedBody::interpolate_words` of the four corners, the chip's own lerp of the words, and
   the response drawn across the stage is those words. Nothing fits, leads, matches,
   normalises or re-scales on the way from a corner to the ear.
3. The rails are the palette: cards with their own curves, factory corners on the left,
   Klatt vowels, reads and captures on the right. Click a card to hear it exactly. Slide up or
   down a rail from a card and you hear the chip's lerp to its neighbour. Drag a card onto a
   corner, or press A, B, C or D, to put the playing sound there.
4. Drop a .wav on the stage or on a corner and it is read the E-mu way, order-12 LPC at
   11,025 Hz, and becomes a card.
5. Ctrl+S keeps what you hear as a card. In-betweens and slight nudges, kept exactly.
6. W writes the four corners as 240 bytes, `legacy_bytes`. A factory body loaded as a preset
   writes its own bytes back. The written file, reloaded through the plugin's lerp, equals
   what the puck played.

## How to read this, so it is not misread again

- Six sections, all active, in whatever role the sound needs. Not six bells. Not F1 to F6 in
  row order. Real pairs are legal.
- The tool measures nothing and warns of nothing. What is on the screen is the words, drawn.
  Peaks stack in a serial cascade because responses add in dB; the factory ships that, and the
  ear decides whether to keep it.
- Only four sounds ever blend, and only the chip's way. A rail slide is two of them.
- Word space is the whole truth. A sound is thirty integers. The file is the words. The plot is
  the words drawn.
- Words: corner, card, puck, keep, write, body. Not column, hop, lattice, field, star.
- The smallest end-to-end surface first. Stop when a slice adds a layer the ear cannot reach.

## Keys

Arrows move the puck by 1, with Ctrl by 0.2. A B C D, or 1 to 4, put the playing card in that
corner. Ctrl+S keeps. Delete removes a selected capture or read. Ctrl+Z and Ctrl+Y undo and
redo. W writes. Space plays. S saw, N noise. Escape closes a menu.

## Files

`Source/app/Quad.*` the model: cards, the four corners, the puck, the lerp, the file, the pack.
`Library.*` the 132 factory corners, the 12 Klatt vowels from `banks/Klatt 1980.bank.json`,
the wav reader. `Session.*` selection, corners, captures, reads, undo, keys,
`banks/HEADSPACE.quad.json`. `Audio.*` the device callback into the core cascade through a
lock-free slot. `Screen.*` the painting. `Tests/QuadTests.cpp` the acceptance, headless,
rendering `artifacts/shots/headspace.png` without a window.

## Later, each only when Tyson asks

The flat grid of bodies sharing edges. The row editor, six parametric bands per corner, which
is E-mu's own manual mode. A note in with FOLLOW. Transpose in word space. Reordering a
corner's rows to a neighbour's. The averaged-spectrum reader for fixed formants.
