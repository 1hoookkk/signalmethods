# TRENCH Workstation, the JUCE port

Ruling 2026-09-05 (Tyson): the authoring surface is a JUCE standalone app built from the plugin
tree, with native/core behind it. The HTML model at `workstation_min.html` (built by
`tools/build_workstation_min.py`) is the spec, line for line. Python stays beside the app as
the research bench. The Qt app in `native/app` is parked; the marimo plan is research only.

## What the model does today, and the app must do the same

- Frame space: every frame placed on a musical grid, first strong resonance across, second up,
  both log, C-octave ticks. Triangulated. Dragging on empty space plays the barycentric blend
  of the triangle's three frames.
- Cube: eight corners, wireframe, orbit by dragging its body. Each corner draws its frame as a
  small crude response. Axes named by pose ends: MORPH high > low, Q closed > open, Z relaxed >
  stressed; corner tags are poses.
- Drag a frame from the space onto a corner. A click on a frame fills the armed corner.
- With eight corners set, dragging in the space solves MORPH and Q so the cube's trilinear
  blend lands under the pointer; Z is a bar. The sound is the cube's blend.
- Response: fixed ±30 dB, 20 Hz to 20 kHz, hard 0 dB line, the cascade at the wheel.
- Control: M, Q, Z bars with values and pose ends; CAPTURE keeps the wheel's words as a new
  frame in the space; CLEAR CAPS.
- Everything painted. No stock widgets.

## What the app adds that the model cannot

- Sound: audition through the core cascade at the device rate, BITE included, from a built-in
  source (noise, saw, a loaded file) and from the audio input.
- FROM AUDIO: read a sound into a frame with the core's `trench_audio_speech_poles` and
  `trench_audio_resonances`, drop it in the space.
- Export: bake the cube to a 240-byte body (front face, or the Z position chosen) with the
  core's PackedBody; the plugin loads it.
- Files: open bodies and Morpheus cubes into the space as frames; save and load a cube and
  the capture set as one document.
- Naming: axis pose names and corner names editable per cube.

## Structure

- Source: `native/workstation/` (Source/Main.cpp, Workstation.h, panels as one Component each:
  FrameSpace, Cube, Response, Control). Target `TRENCH_Workstation` declared in
  `plugin/CMakeLists.txt` with `juce_add_gui_app`, linking `trench_core` and the plugin's DSP
  bridge for audition. Build through the same vcvars wrapper into `out/build/vst3`.
- Painting: one `paint (Graphics&)` per panel. Rule-line panels with inverse title bars,
  monospace text, MATLAB axes with ticks and dotted grid, thin strokes. Direct2D is on
  (`JUCE_DIRECT2D=1` already set); the software renderer is the fallback and is fast in 9.
- Input: mouse and pen. Multi-touch on Windows is off by default in JUCE 9; enable with
  `TopLevelWindow::setUsingWindowsMultiTouch (true)` only if a touch surface is wanted.
- State: a Cube document = eight frame references, pose names, wheel position, captures.
  Frames are words plus name plus a source tag (bank, cube, capture, audio).
- Tests: headless. A `TRENCH_WorkstationTests` target checks the word lerp, the trilinear
  blend, the plane solve, the placement (resonances from `geometry_from_words`), and a
  FaceShot-style render of each panel for review by image.

## Order of work

1. Skeleton app, four panels painted, frames from `plugin/presets/p2k` and the Morpheus
   cubes, the space and the cube live, no sound. Render check by image.
2. Audition through the bridge; source menu painted; input passthrough.
3. Drag from space to corner; plane drives M and Q; CAPTURE.
4. Export to body240 and load in the plugin; document save and load.
5. FROM AUDIO into the space.
6. Pose names editable; Morpheus cube import as two faces.

## JUCE 9 notes that matter here

- 9.0.0: new SVG parser (lunasvg), variable fonts, faster software renderer, better multi-touch
  on Windows and Linux, headless CMake improvements. `Drawable` no longer a Component; use
  `DrawableComponent` if a Drawable is ever wrapped.
- 9.0.0 breaking: Windows multi-touch off by default (see Input above).
- 9.0.1: zlib, png, jpeg, flac built as C; no conflict unless the app links its own copies.
- 8.0.11 to 8.0.13 carried the rendering wins this app relies on: font and glyph cache,
  faster Component painting, Direct2D fixes, Windows resizing.
- `juce_animation` exists for eased motion if the wheel or the orbit ever needs it; not used
  in step 1.
