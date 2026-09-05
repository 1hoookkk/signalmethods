# Brief for an outside opinion: the hero space of the TRENCH Workstation

You are being asked for a design opinion. Answer as the laziest senior engineer on the team:
the least thing you would build and still put your name on, no hedging, no menu of options,
one answer. Read everything below, then answer the question at the end in your own words.
Do not assume any of the abstractions tried so far is right.

## The product

TRENCH is a VST3 plugin, "a musical filter by Signal Methods". It plays a Z-plane filter: a
serial cascade of six second-order sections (twelve poles), each section a pole pair, a zero
pair and a gain. A body is four corners of such a cascade; the plugin's wheel morphs between
the corners with a per-word linear interpolation of the packed coefficient words, the way the
E-mu hardware did. MORPH and Q are the wheel's two axes. The plugin plays bytes and nothing
else; the face is locked.

Beside it is the TRENCH Workstation, an authoring tool (JUCE, C++, OpenGL, MATLAB look). It
is not shipped. Its job: let a musician find and make filters, then EXPORT one as 240 bytes
for the plugin.

## The material

- 33 Proteus 2000 / X3 bodies, hand voiced by E-mu, four corners each, datum 44.1 kHz.
- 289 Morpheus cubes (1993), eight corners each, datum 39,062.5 Hz.
- 18 Emulator X Morph Designer bodies (one-dimensional).
- Read frames: vowels (Hillenbrand 1995 formant tables), heads (SOFA HRTF impulse
  responses), XL-1 ROM waves, instrument impulse responses, and anything read from audio in
  the SOUND room (LPC, order 12 for speech, higher for bells).
- 2017 Rossum Electro-Music Morpheus module: its firmware cube table was decoded from the
  update audio. It carries the same 289 cubes by name, with pole pitches stored as note
  counts (64 per octave) rather than chip coefficients, compiled at runtime for the module's
  rate.

## The representation, settled

A frame's truth is a chord: six stages, each with a pole voice and a zero voice (on, note as
a MIDI number, width in semitones) and a gain in dB. Coefficient words are compiled from the
chord at a datum. Round trip on the 33 bodies is byte-exact. A morph between two frames is
voice leading: each voice glides in pitch to its partner. The word lerp the chip does is an
approximation of that glide, measured within two percent.

Consequences: any source reduces to six voices. A formant table is already a chord. An
impulse response or a transfer function goes through the reader. Datum disappears; compile
at whatever rate.

## What the musician must be able to do

- Move through the whole factory and hear it, continuously, with one gesture. This is the
  entire point of the tool. The user's words: "i cant move through the space which is the
  entire thing".
- Find a filter by ear, open it, ride its wheel, edit a corner in notes and dB, export.
- Pair two frames anywhere and morph straight between them, past the ends (caricature: the
  push continues in note space; the chip's words cap what a wide low pole can be).
- Read a sound into a frame and drop it into the space.
- Never see a node, an edge, a face, a coefficient or a Hz value unless asked. The words on
  the surface are filter, morph, Q, frame, note, dB.

## The abstractions tried, and the verdicts

1. Martens plane: PCA of the response spectra, frames as anchors, Delaunay blend. Rejected:
   positions meant nothing to a musician.
2. Quilt of 2x2 grids on an isometric floor, one per body. Rejected: a shelf in file order.
3. One lattice, then a tower of lattices per source group. Rejected: too much time in one
   space, a hyperstructure nobody reads.
4. Flat anchors per floor with a triangle blend. Rejected for the same reason as 1.
5. The stitch: the factory as one graph (shared corners are one node), spectral (Laplacian)
   layout, floors per source, drawn as a MATLAB 3D axes with stems. Verdict: "Wrong
   execution its not easy to understand", and to a proposal of arbitrary axes: "Random? Is
   that better?".
6. The same stitch on musical axes: every corner at its strongest peak, pitch across, level
   along, resonance as stem height, floors per source. Readable, but nothing between the
   stems could be played.
7. The shelf: one stem per filter, press to open it as its square with its neighbours lit.
   Verdict: "How do we abstract it properly without confusion", asked twice.
8. Free movement on a floor, blending the four nearest filters. Verdict: "i still cant move
   through the hero cube. its the wrong abstraction. whats the right one".

The current proposal (being built): three continuous musical axes, pitch across, level
along, resonance up, every filter and frame at its strongest voice, sources as colours that
fold from a tray, drag to move in pitch and level, Shift-drag for resonance, the sound at any
point the inverse-distance blend of the four nearest filters in that space.

## What you have to work with

- The 3D box, orbit, zoom, the stem plot idiom, filled squares for an open filter, the
  ARMAdillo pole plot, the fixed ±30 dB response, the timeline with keys, PAIR, CAPTURE,
  EDIT with six rows in notes, SOUND with a spectrogram and a reader.
- Every filter's chord, every corner's chord, the census of shared corners, the neighbour
  graph, the Rossum note table.

## The question

What is the right abstraction for moving through the whole factory by ear, so that a
musician understands it at a glance and never loses the thread between "where I am", "what
I hear" and "what I can grab"? Answer with the space's axes, what a point means, what one
gesture does, and what is drawn. If the answer is not a space at all, say so. Be concrete
enough that an engineer can build it from your answer alone.

User's own words on the last attempt, verbatim: "i still cant move through the hero cube.
its the wrong abstraction. whats the right one"
