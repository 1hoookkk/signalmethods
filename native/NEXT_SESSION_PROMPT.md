# Transitional brief for the next workstation session

Written 2026-09-05, end of a long session. Tyson's closing verdict: "the issue is in the
monolithic files." Read this before touching native/workstation.

## Where things stand

- Tree compiles at the last commit plus the analyser (uncommitted at the moment of writing,
  committed right after this file). `TRENCH_Workstation --check` passes 17 gesture checks
  headless; `TRENCH_WorkstationTests` passes 17 model checks. Keep both green before any
  build reaches Tyson's screen.
- Rooms today: FRAMES (a scan line of the bank), MORPH (the pad), EDIT (six rows in notes),
  READ (spectrogram, reader, IN/OUT loop). Right column: response plot, LISTEN, FILTER,
  SAW/SAMPLE/NOISE source, ANALYSE (measured output trace over the plotted curve), meter.
- A draft of the next FRAMES room sits at native/workstation/drafts/WorkstationRooms.plane.cpp.txt:
  the bank as a monolithic flat grid on ROOT x VOICING, cursor set by three sliders (ROOT,
  VOICING, RESONANCE), sound at the cursor = chord-space blend of the nearest frames. It
  needs `blendChords` in model/Morph (weighted mean of notes, log widths, dB gains after
  leading every parent to the heaviest), the header members it uses, and its input wiring.
  It was not compiled. Tyson's words for it: "maybe flat grids or something on a monolithic
  plane its almost eerie. but the navigation should happen elsewhere. with 3 knobs or
  controls for the 3 axis"; "the sounds need a 0-100 logarithmic morph between each other".

## The structural problem, which is the first job

`ui/Workstation.h` is one class with ~70 members: every room's state, every mode, every
flag that decides what is live (`playBody`, `pairMode`, `spot`, `openFace`, `playFrame`,
`compare`, `analyse`, ...). `live()` walks those flags in a priority order and gestures flip
them and hope. Layout, keys, input, paint and scene are split by concern, not by room, so
every room change touches five files. This is why each build arrives with a new mismatch.

Do this before any new feature:

1. One `Live` value: an enum of what is sounding (Frame, Scan/Plane cursor, Body wheel,
   Corner compare, Pair, Read slice, Nothing) plus its parameters. Every gesture sets it.
   `live()` is a switch on it. Nothing else decides.
2. One class per room, each owning its state, layout, keys, input and painting:
   `FramesRoom`, `MorphRoom`, `EditRoom`, `ReadRoom`. The shell owns the audio, the
   response column, the room keys and the `Live` value. Rooms talk to the shell through a
   small interface (setLive, status, takeCorner, capture).
3. Keep `Canvas` and the scene batches; keep `Layout` per room inside the room.
4. Keep the gesture check and extend it per room; it caught real faults three times tonight.
5. Delete what is dead: the 3D stitch drawing in `WorkstationScene.cpp` and `View3D`, the
   timeline in the frame rooms, the old `spot`/`Spot` free-point plumbing if the plane draft
   does not use it, `faceAtUnused`.

## Laws that stand (do not re-litigate)

- A frame's truth is its chord: six stages, pole and zero voices (note, width in semitones),
  gain in dB. Words are compiled at a datum. Round trip on the 33 bodies is byte-exact.
- Voice leading: a partner's stages are reordered to the pinned frame's nearest notes;
  partnerless voices dissolve in place as a cancelling pole and zero. Section order commutes,
  so EXPORT writes the led corners and the plugin's word lerp plays the voice leading.
- Reads are bells: razor pole (0.25 st) with a zero on the same note sixteen times wider.
- Palette: MATLAB grey ground, white axes, black frames, blue data, orange chosen, gold live.
  Arial for labels, Lucida Console only for columns. Everything through the GL shaders.
- No 3D browser, no bank visible while playing, no automatic rearrangement, no pole plot in
  the hero, no sentences on the surface. Words on the surface: filter, morph, Q, frame,
  note, dB, root, voicing, resonance.
- The plugin plays bytes. Nothing here changes the ship face.

## Open questions for Tyson, in his order

1. The plane draft: flat grid of the bank on ROOT x VOICING with three controls. Build it as
   the FRAMES room after the restructure, or revise first?
2. Whether the pad in MORPH should extend past the ends (the caricature) by default.
3. The four-corner fill rule: one frame fills all, a second fills the opposite edge. He saw
   the all-same case in EDIT and read it as broken; consider showing "copies of M0 Q0" on
   the copied corners.

## Files

- Record: plugin/NEXT_SESSION.md (append-only, one entry per decision).
- Memory: C:\Users\hooki\.claude\projects\C--Users-hooki-trench-authoring\memory\
  (instrument-ruling.md, chord-representation.md, rossum-module-cubes-decoded.md).
- Outside opinions used tonight: native/PROMPT_HERO_SPACE.md and the two GPT-6 answers Tyson
  pasted (recorded in NEXT_SESSION.md).
- Build: cmake through vcvars64, target TRENCH_Workstation; wrapper pattern in CLAUDE.md.

Tyson's verbatim last line: "the issue is in the monolithic files. wrap the session up with
a transitional prompt"
