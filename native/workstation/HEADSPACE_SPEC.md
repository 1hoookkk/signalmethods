# HEADSPACE

The authoring tool for TRENCH bodies. A JUCE app in the plugin's CMake tree, built alone by the
`headspace` preset, linked straight to `native/core`. One screen, painted by hand through one look:
charcoal ground, light axes, a fine grid, MATLAB line colours lifted for the dark, no filled
buttons, no captions. Two more windows: the spectrogram on G, and the stage on E.

Rulings of 2026-09-07 not yet built, which the next slices must honour: the look reads
cartoonish and unserious and is not striking; no icons, words in small caps instead; hairline
curves and crosshair markers; the keyboard a rule of keys; one hero on the screen. The stage,
the editing of one corner's poles and zeros, leaves the screen for its own window, opened on E,
like the spectrogram; the screen shows, it does not edit. The stage window carries two plots: the
response with its handles as before, and the ARMAdillo plot, poles and zeros on the circle in the
chip's own encoded coordinates, used last, to place the zeros by hand. Ingest: anything read (a
frame off the surface, a file, an impulse response, a table row) writes poles only and parks its
zeros, the row-six ceiling excepted; anything already in the chip's words (the 132 P2K corners,
the 2,312 Morpheus corners) comes in as a card by byte copy with its zeros; Klatt, Hillenbrand and
Peterson and Barney are pole-only landmarks. A folder of notes of one instrument is one source,
averaged frame by frame as the Massie patent's analysis stage does. The tool is Peevers's
Spectrogram plus the cube: his surface and panel literally (the panel is FORM_Menu_Form.md beside
the decomp, 50 controls on a 344 x 368 form), and one added gesture, a frame off the surface
becomes a corner.

## The law

In a serial cascade section gains multiply and responses add in dB; a corner is six sections of
five words, sixty bytes; a body is four corners and the chip's lerp; the ear decides.

## The loop the tool must pass

Load two very different anchors. Hold notes and play a phrase while morphing with the mod wheel.
Replace one anchor while it keeps sounding. Find an intermediate sound. Copy it to corner A. Keep
exploring without changing A. Audition and export the canonical grid. Reload it and confirm it
reproduces its sound. MIDI active throughout. Nothing is built that this loop does not need
before the loop is green.

## The screen, four quarters

The mother is the largest quarter, top-left. The stage is top-right. The palette is below the
mother. The body, the engine and the keyboard are bottom-right. Nothing else.

1. The mother. Two anchors, A on the left and B on the right, each a name and its curve, and
   three rails under them. The anchors are cards: drop a card on one, or click its caret and
   pick from the palette, and the sound goes on from where it was. The mother's eight corners
   are not pinned; they are made from the two anchors by the chip's three axes as gestures:
   - MORPH, the first rail, A to B. The mod wheel rides it.
   - FREQUENCY, the second rail, the anchor to the anchor transposed. The transpose is
     `transposed()`: every pole and zero by one ratio, radius^ratio so each row keeps its width
     in semitones, the fifth word and the ceiling row untouched. The amount is octaves, shown
     at the rail's right end, one number; the wheel over the rail steps it by a quarter octave
     between -3 and +3. It boots at +1.
   - STRESS, the third rail, the relaxed anchor to the anchor. The relaxed anchor is the neutral
     tube: each live pole of row r slides to 500 (2r + 1) Hz, its zero by the same ratio, widths,
     fifth words and the ceiling row untouched, then DC unity. Full stress is the anchor verbatim.
   The probe is the diamond on each rail. What plays is `interpolate_words` of the eight made
   corners at the three probe positions, the chip's own lerp, and the keyboard strikes it. At
   MORPH 0, FREQUENCY 0 and full STRESS the words are anchor A's exact words, never a geometry
   roundtrip; at MORPH 1 the same for B. The tick at the rails' right bakes the plane at this
   stress into the body: A is M0 Q1, B is M1 Q1, C is M0 Q0, D is M1 Q0, Q being FREQUENCY.
   Evidence: the Morpheus manual's VowelSpace (Martens): Morph sweeps F1, Frequency sweeps F2,
   Transform is stress, "all of the vowel frequencies collapse to a relaxed schwa"; the
   289-cube census `evidence/research-results/morpheus_axis_census.txt`: Frequency moves pitch
   and width together on 81% of poles at 0.86 oct/oct, Transform moves pitch on 45% of poles,
   median 706 cents, widths x0.93.
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
   caret to pick from the palette; drop a card or a .wav on a cell. The engine is a small plot
   of what plays with its name, the arrows that feed what plays into A B C D, the write glyph
   for the 240 bytes, and the sources: play, pluck, saw with its note, noise, loop. Pluck is the
   only source that strikes the 10 ms burst; the others hold their sound while a key is down
   and ring down on release. The keyboard is three octaves. Space drones without a key. The key
   never moves the filter.

## The spectrogram window

Alan Peevers's Spectrogram, the 1995 SGI build preserved at
`evidence/research-results/emu-sgi-1993/spectrogram/extracted/spectrogram`, ported verbatim
from its decompilation in `evidence/research-results/emu-sgi-1993/spectrogram/decompiled/`.
G opens and closes it. It analyses what plays, tapped after the cascade, so the keyboard, the
drone, the pluck and the loop excite it, the way his `-l` live input did.

- The analysis is his, routine for routine, in `Source/dsp/Peevers.*`, each function keeping
  its name and its arithmetic: `win_calc` and `winmult` with his nine windows (exact Blackman,
  Blackman, Blackman-Harris 1 to 4, Hamming, Hanning, none), `buildtable`, `bitreverse`, `fft`,
  `ifft`, `mag2`, `magl`, `log_of`, `findmax`, `normalize`, and for Env the 12th-order LPC by
  `gal` and `lattice`, the envelope being the FFT of the synthesis filter's impulse response,
  as his README says. His Filter path is a later slice: `fof_value` and `fof_transf` build one
  formant wave function (Rodet) per drawn trajectory into a surface, Modify multiplies that
  surface into each frame, Impulse replaces the frame with an impulse train's spectrum, and
  `olap` resynthesises through the square-root window; that is how he heard a drawn filter. His defaults: FFT 256, window 256,
  stride 128, window 7 Hanning, order 12, 500 frames.
- The display is his: x frequency, y amplitude in dB, z time, the surface drawn slice by slice
  as `draw_surf_slice` lays it out, tilted by azimuth and declination from a drag as
  `polarview` was; a 2D toggle, LogF, Axes, Gain and Floor, Mesh cycling line, point, polygon,
  mesh. Painted through Look with the plot colours; no GL, no colour lookup sliders.
- Env, FFT size, window size, stride and window are the only numbers; they sit in one line
  under the surface.
- Tests, headless: the window's component renders to `artifacts/shots/spectrogram.png`; the
  window tables match the decompiled formulas at three points each; the FFT of an impulse is
  flat and of a sine at bin k peaks at k; the LPC-12 envelope of a two-pole signal peaks at
  the pole within one bin; a held saw through the i corner shows its harmonics at multiples of
  f0 in the latest frame.

## Words

Mother, anchor, rail, probe, morph, frequency, stress, bake, stage, corner, cell, card, keep,
write, body, spectrogram. Not room, tab, table, column, lattice, field, vertex, depth.

## Keys

The Z row is a keyboard: Z is C, S is C sharp, X is D, and so on to M as B, then comma, L and
full stop for the next C, D and E; Page Up and Page Down lift or drop it an octave and the saw's
note with it. 1 to 4, or the arrows in the engine, put what plays in that corner; with Shift,
the column; Enter puts it in the target. Arrows move the pad by 1, with Ctrl by 0.2. Space
drones. Ctrl+K keeps. Ctrl+W writes. Ctrl+P pluck, Ctrl+S saw, Ctrl+N noise, Ctrl+L loop.
[ and ] step the saw's note by a semitone. Ctrl+H shows the raw words. Ctrl+G opens the
spectrogram. Slash finds a card by name, Enter plays the first match, Escape clears. Ctrl+Z
and Ctrl+Y undo and redo. Delete removes a selected capture or read. Escape closes the menu.

## Files

`Source/app`: `Quad.*` the model, cards, corners, the made corners of the mother
(`Explore`: a, b, morph, frequency, stress, octaves; `motherBodyOf`, `motherWordsAt`), the
lerp, the section geometry, the file, the pack; `Library.*` the vowel banks, the measured
bodies, the made vowel, `transposed`, `relaxed`, the wav reader; `Session.*` selection,
anchors, the probe, captures, reads, undo, keys, the working corner,
`banks/HEADSPACE.quad.json`; `Audio.*` the device callback into the core cascade, the burst,
the sources, MIDI in, the tap for the spectrogram; `Main.cpp`. `Source/ui`: `Look.*` the look
and feel, palette, axes, handles, glyphs; `Plot.*` the axis maths and the cached curves; one
file per quarter, `Mother.*`, `Stage.*`, `Palette.*`, `Body.*`, `Engine.*`, `Keyboard.*`,
`PinMenu.*`; `Screen.*` composes them, lays out the grid, and dispatches the mouse, the keys
and the drops; `Spectrogram.*` the window. `Source/dsp/Peevers.*` the port.
`Tests/QuadTests.cpp` the acceptance, headless, rendering `artifacts/shots/headspace*.png` and
`spectrogram.png` without a window.

Rulings of 2026-09-07, late, from the running app: the three rails are right and stay. The G
window is not an analyser of the engine: it has its own source and its own audio path. Load a
family from the XL bank, `evidence/factory-data/xl1-dsf-aud` (224 notes named "<Family> <Note>"),
or drop a .wav or a folder; a key plays the nearest sampled note of the loaded family, mixed
straight to the device, never through the cascade; Peevers's analysis runs on that signal, Env
and Span's averager; a frame off its surface becomes a corner. The main engine plays the body.
The bottom right must let you play the filter and see it: the engine plot goes live, the
output's spectrum over the corner's response with the input's spectrum dimmer, refreshed while
keys are held; the keyboard stays; the arrows, Keep and the file name go; the sources are four
words; the plot is the sound, dragged onto a cell to place it.

Ruling of 2026-09-07, later still: "work directly from the top right and straight into the 2x2
grid while moving the morph there." The stage stays top right; there is no E window. The stage
draws what plays at the pad, live as MORPH and Q move, with the working corner's handles on it;
a drag edits that corner's words and is heard through the morph at once. The ARMAdillo plot for
the zeros lives on the stage behind a key.

Ruling of 2026-09-07, night, from the running app: "i found the key flow. it is within the
sliders. and making variations of a and b. i need to click on each of the two top left frames
and not have it jump back to the 2x2 grid. i tweak them and press the 1234 numbers." A click on
an anchor makes that anchor the stage's edit target: its name in the stage title, its handles,
a drag writes its words, the rails sweep the tweaked frame at once. A library card is never
edited in place: the first edit copies it into a capture and points the anchor at the copy.
1 to 4 copy what plays into the grid.

Ruling of 2026-09-07, night, "what im editing needs to be true here and everywhere": one
target. The target is what the pad points at. Pad on a corner: that corner is the target; the
rails write into it as they move, the stage's handles are its handles, W writes the body with it,
the engine box shows it. Pad between corners: the box and the stage show the lerp, the handles
are off, the rails play so you can search, 1 to 4 put what plays into a corner, and moving the
pad onto a corner makes it the target. An anchor clicked top-left is the target until the pad is
touched. The orange follows the target. There is no separate working corner.

Ruling of 2026-09-07, night, on the stage "the plot is lying to me": handles edit "radius and
angle". A pole handle's sideways drag sets the angle (frequency on the log axis) and its vertical
drag sets the radius directly (log-scaled 1 - r, the chip's own word), nothing solved against the
cascade; the curve is drawn from the words, so a handle sits wherever its radius put it. A zero
handle the same, angle and radius. Nothing else moves during a drag; the 0 dB trim at DC happens
once on release. A read's zeros stay parked unless placed by hand.

## The queue, in order, one slice each

1. The G window as its own instrument: the XL bank sampler on its own audio path, his panel
   and surface from FORM_Menu_Form.md, and the one added gesture: a frame picked off the
   surface becomes a corner.
1a. One target: the pad's corner, or a clicked anchor; rails, stage and W act on it; library
   cards copied on first edit; handles off between corners; handles edit angle and radius
   directly, trim on release.
1b. The engine goes live: output and input spectra over the response while keys are held;
   arrows, Keep and file name removed; sources as words; the plot as a draggable card.
2. Span as a mode of the window: the averaged spectrum of what plays from Span's decomp
   (`evidence/research-results/emu-sgi-1993/span/decompiled/`): `demean`, `xavg` with its
   feedback constant, power or energy, `draw_axes` and `draw_graph`; keys C reset the averager,
   S snapshot, X and Y the axis limits; defaults Blackman, 1024, 1024.
3. The stage shows what plays at the pad with the working corner's handles; the ARMAdillo plot
   behind a key for the zeros.
4. Ingest: reads are poles only with parked zeros; the 132 P2K and 2,312 Morpheus corners as
   cards by byte copy from the canonical export; Peterson and Barney landmarks; a folder of
   notes as one source.
5. His Filter path: drawn trajectories as formant wave functions, Impulse and Modify, olap.

## Later, each only when Tyson asks

Formant overlays on the stage and the spectrogram: the Klatt and Hillenbrand F1 and F2 as
marks. The vector: origin to anchor with a puck between, the push past the anchor, the span
into four corners. Row reorder on the stage. FOLLOW, key to MORPH, in the plugin. Peterson and
Barney cards. Measured zeros for reads from an ARMA fit ported to the core.
