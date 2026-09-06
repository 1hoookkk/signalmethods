# HEADSPACE

The authoring tool for TRENCH bodies. A native JUCE app in the plugin's CMake tree, linked
straight to `native/core`. One screen: the vowel chart, F1 down and F2 across, the Klatt 1980
vowels as letters, and every factory corner as a star at its own F1 and F2. Hover a star and
you hear it. Four pins, M0 Q0, M1 Q0, M0 Q1 and M1 Q1, each on a star of your choosing, make
the body; the puck inside the quad plays the plugin's own lerp of the four. Ctrl+S keeps the
puck's sound as a new star you can pin. W writes the four pins as a 240-byte body. Where the
lerp stacks peaks above the corners, the quad shows a red tint, computed from the words. The
spec is `HEADSPACE_SPEC.md`.

## Build and run

    native\workstationuild_headspace.cmd

builds `TRENCH_Headspace_App` and `TRENCH_QuadTests` through MSVC vcvars64 with the `headspace`
build preset, then runs the tests. The app is
`outuildst3\plugin\workstation\TRENCH_Headspace_App_artefacts\Release\HEADSPACE.exe`.

## Keys

- Hover a star to hear it. 1 to 4 pin the hovered star to that corner. Drag a pin onto
  another star. Shift-drag inside the quad moves all four pins; each snaps to its nearest star.
- Drag the puck, or click inside the quad. Left and Right move MORPH by 1, with Ctrl by 0.2.
  Up and Down move Q the same.
- Space plays. PLAY, SAW, PINK NOISE at the top.
- Ctrl+S keeps the puck's sound as a new star. Delete removes a selected capture.
- Ctrl+Z and Ctrl+Y undo and redo pins, captures and deletions.
- W or WRITE BODY FILE writes the four pins to `plugin/presets/user/headspace_<stamp>.body240`.

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
