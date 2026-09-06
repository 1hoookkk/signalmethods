# HEADSPACE

The authoring tool for TRENCH bodies. A JUCE app in the plugin's CMake tree, built alone by the
`headspace` preset, linked straight to `native/core`. One screen, painted by hand through one look:
figure-grey ground, white axes, a fine grid, MATLAB line colours, no filled buttons, no captions.

## The law

In a serial cascade section gains multiply and responses add in dB; a corner is six sections of
five words, sixty bytes; a body is four corners and the chip's lerp; the ear decides.

## The screen, four quarters

The mother is the largest quarter, top-left. The stage is top-right. The palette is below the
mother. The body, the engine and the keyboard are bottom-right. Nothing else.

1. The mother. Eight cells in two 2x2 faces, front and back, laid out like the body: A B over
   C D on each face, numbered 1 to 8 by the chip's corner order. Each cell is a name and its
   curve. Drop a card on a cell, or click its caret and pick from the palette, to throw a frame
   into that vertex; click its number to open that vertex on the stage. The probe is the diamond
   on both faces: press anywhere on a face to move it in MORPH and Q, drag the depth rail to move
   it between the faces. What plays at the probe is `interpolate_words` of the eight, the chip's
   own three-axis lerp, and the keyboard strikes it. Shift-drag the probe onto a body cell to
   keep that sound there. The tick under the rail bakes the plane at this depth into the four
   corners of the body.
2. The stage. The working corner's response on the fixed frame, +30 to -30 dB, 20 Hz to 20 kHz,
   with its six sections as handles: circles on the poles, squares on the zeros, a blade where a
   zero sits on the circle. Drag a circle for frequency and height; the pole radius is solved
   against the whole re-levelled cascade so the curve follows the finger. Roll the wheel over a
   circle for width. Drag a square for the zero's frequency and depth; pull it to the floor and
   it is on the circle. Drag the blade for the ceiling's frequency. Alt-click drops a zero on the
   nearest zero-less row. Double-click empty plot to wake a rest row there. Carve, top-right,
   swings every zero off its pole into the valley above it by the amount dragged. Every edit
   writes real words through the core's geometry, re-levels the corner to 0 dB at DC, and is one
   undo. H shows the raw words over the plot.
3. The palette. The vowel space, F2 high to low across and F1 close to open down, with the 12
   Klatt vowels and schwa as named landmarks and the 48 Hillenbrand medians as quiet dots. Click
   to hear, click empty space to make a vowel at that F1 and F2, Shift-drag a vowel to transpose
   it by its first formant. Beside it the cards in three tabs, Bodies, Reads, Captures, each with
   its name and its curve; drop a .wav on the list to read it. Keep, above the chart, keeps what
   plays as a capture.
4. The body, the engine, the keyboard. The body is the 2x2 of the shipping corners, A top-left
   M0 Q1, B top-right M1 Q1, C bottom-left M0 Q0, D bottom-right M1 Q0, each a name and its
   curve, and it is the pad: the diamond on it is MORPH and Q, drag it anywhere. Click a letter
   to make that corner the working corner, the one the stage shows and Enter fills; click the
   caret to pick from the palette; drop a card, a probe or a .wav on a cell. The engine is a small
   plot of what plays with its name, the arrows that feed what plays into A B C D, the write
   glyph for the 240 bytes, and the sources: play, saw with its note, noise, loop. The keyboard
   is three octaves; a key strikes a 10 ms burst into the cascade, holds the source while down,
   and rings down on release. Space drones without a key. The key never moves the filter.

## Words

Mother, vertex, probe, depth, stage, corner, cell, card, keep, write, body. Not room, tab, table,
column, lattice, field.

## Keys

Arrows move the pad's diamond by 1, with Ctrl by 0.2. 1 to 4, or A B C D, or the arrows in the
engine, put what plays in that corner; Enter puts it in the working corner. Ctrl+S keeps. Delete
removes a selected capture or read. Ctrl+Z and Ctrl+Y undo and redo. W writes. Space drones.
S saw, N noise, L loop. [ and ] step the saw's note by a semitone, Page Up and Page Down by an
octave. H shows the raw words. Escape closes the menu.

## Files

`Source/app`: `Quad.*` the model, cards, corners, the eight-vertex cube, the lerp, the section
geometry, the file, the pack; `Library.*` the vowel banks, the measured bodies, the made vowel,
the wav reader; `Session.*` selection, corners, the mother, captures, reads, undo, keys, the
working corner, `banks/HEADSPACE.quad.json`; `Audio.*` the device callback into the core cascade,
the burst, the sources, MIDI in; `Main.cpp`. `Source/ui`: `Look.*` the look and feel, palette,
axes, handles, glyphs; `Plot.*` the axis maths and the cached curves; one file per quarter,
`Mother.*`, `Stage.*`, `Palette.*`, `Body.*`, `Engine.*`, `Keyboard.*`, `PinMenu.*`; `Screen.*`
composes them, lays out the grid, and dispatches the mouse, the keys and the drops.
`Tests/QuadTests.cpp` the acceptance, headless, rendering `artifacts/shots/headspace*.png`
without a window.

## Later, each only when Tyson asks

The vector: origin to anchor with a puck between, the push past the anchor, the span into four
corners. Row reorder on the stage. FOLLOW, key to MORPH, in the plugin. Peterson and Barney
cards. Measured zeros for reads from an ARMA fit ported to the core.
