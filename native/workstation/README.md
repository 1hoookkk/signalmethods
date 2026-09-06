# HEADSPACE

The authoring tool for TRENCH bodies. A native JUCE app in the plugin's CMake tree, linked
straight to `native/core`. One screen: the strip. Two rows of states, Q0 and Q1; a column is a
state and its Q partner; two neighbouring columns make a square, and a square is a body. Walk
the strip and you hear the plugin's own lerp of the packed words. Ctrl+S keeps the column you
are standing on. W writes the square you stand in as a 240-byte body. The spec is
`HEADSPACE_SPEC.md`.

## Build and run

    native\workstation\build_headspace.cmd

builds `TRENCH_Headspace_App` and `TRENCH_StripTests` through MSVC vcvars64 with the `headspace`
build preset, then runs the strip tests. The app is
`out\build\vst3\plugin\workstation\TRENCH_Headspace_App_artefacts\Release\HEADSPACE.exe`.

## Keys

- Click a library entry to hear it exactly. Enter places it after the selected column.
- Left and Right move MORPH by 1, with Ctrl by 0.2. Up and Down move Q the same. Past 100 is
  the next square. Drag the pad or click the navigator to jump.
- Space plays. PLAY, SAW, PINK NOISE at the top.
- Ctrl+S keeps the column, both rows at this MORPH, inserted where you stood.
- [ and ] move the selected column. Delete removes it. Ctrl+Z and Ctrl+Y undo and redo.
- 1 to 9, Home and End jump to columns, and so does a click on the column list.
- W or WRITE BODY FILE writes the square to `plugin/presets/user/headspace_<stamp>.body240`.

## Files

- `Source/app/Strip.*`: the model. Columns, squares, the walk, the capture, the file, the
  240-byte pack, all through `trench::core::PackedBody`.
- `Source/app/Library.*`: the 132 factory corners read from `plugin/presets/p2k`.
- `Source/app/Session.*`: state, undo, redo, keys, persistence in `banks/HEADSPACE.strip.json`.
- `Source/app/Audio.*`: JUCE device callback into the core cascade; words arrive through a
  lock-free slot, nothing allocates or locks on the audio thread.
- `Source/app/Screen.*`: everything painted by hand, no JUCE widgets.
- `Tests/StripTests.cpp`: the acceptance tests, headless. `ctest --preset headspace`.
- `artifacts/shots/headspace.png`: the screen rendered by the tests, never a window.

## Record

`DECISIONS.md` here, one entry per decision; `plugin/NEXT_SESSION.md` for the running brief.
