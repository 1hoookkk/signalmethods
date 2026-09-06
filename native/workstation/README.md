# HEADSPACE

The authoring tool for TRENCH bodies. A native JUCE app in the plugin's CMake tree, linked
straight to `native/core`. One surface: the stage is the body, its four corners are the four
sounds with a name box each, PRESET loads a factory body, the puck plays the plugin's own lerp
of the four and the response fills the stage. The rails on both sides are the palette: click a
card to hear it, slide along a rail to morph to its neighbour, drag a card onto a corner. Drop
a .wav to read it. Ctrl+S keeps what you hear as a card. W writes the four corners as a
240-byte body. The spec is `HEADSPACE_SPEC.md`.

## Build and run

    native\workstationuild_headspace.cmd

builds `TRENCH_Headspace_App` and `TRENCH_QuadTests` through MSVC vcvars64 with the `headspace`
build preset, then runs the tests. The app is
`outuildst3\plugin\workstation\TRENCH_Headspace_App_artefacts\Release\HEADSPACE.exe`.

## Keys

- Click a card to hear it. A B C D, or 1 to 4, put it in that corner. Drag a card onto a
  corner. Slide up or down a rail from a card to morph to its neighbour.
- Drag the puck on the stage. Left and Right move MORPH by 1, with Ctrl by 0.2. Up and Down
  move Q the same.
- Space plays. S saw, N noise, or the words at the bottom.
- Ctrl+S keeps the sound you hear as a card. Delete removes a selected capture or read.
- Ctrl+Z and Ctrl+Y undo and redo.
- W writes the four corners to `plugin/presets/user/headspace_<stamp>.body240`.

## Files

- `Source/app/Quad.*`: the model. Stars, the four pins, the puck, the file, the 240-byte pack,
  the formant readout and the collision cells, all through `trench::core::PackedBody`.
- `Source/app/Library.*`: the 132 factory corners read from `plugin/presets/p2k` as stars.
- `Source/app/Session.*`: state, hover, pins, captures, undo, redo, keys, persistence in
  `banks/HEADSPACE.quad.json`.
- `Source/app/Audio.*`: JUCE device callback into the core cascade; words arrive through a
  lock-free slot, nothing allocates or locks on the audio thread.
- `Source/app/Screen.*`: everything painted by hand, no JUCE widgets.
- `Tests/QuadTests.cpp`: the acceptance tests, headless. `ctest --preset headspace`.
- `artifacts/shots/headspace.png`: the screen rendered by the tests, never a window.

## Record

`DECISIONS.md` here, one entry per decision; `plugin/NEXT_SESSION.md` for the running brief.
