# TRENCH Workstation

The authoring tool for TRENCH bodies: a MATLAB R2025b toolbox over two MEX files, `trench_bridge`
(the model: chords, words, voice leading, morphs, banks, the readers) and `trench_audio` (the engine
and the sound card). The plugin plays what this writes; nothing here changes the ship face.

## Run

In MATLAB:

    cd C:\Users\hooki\trench-native\native\workstation
    addpath toolbox
    trench.setup
    app = trench.launch;

Rooms: ANALYSE (open a sound, ENVELOPE, CURSOR, READ FRAME AT CURSOR), FRAMES (the bank as a line of
anchors, the library, the map), MORPH (the pad over the four corners), CORNER (the six rows).
The response panel and the body group stay on screen in every room.

## Build

The MEX files come from the plugin's CMake tree and must be built through MSVC vcvars64:

    build_mex.cmd

or in MATLAB `buildtool mex`. The build copies `trench_bridge.mexw64` and `trench_audio.mexw64` into
`toolbox/mex`. MATLAB must not hold the old files: close the workstation, or run `clear mex`, before
the copy.

## Test

All headless, figures invisible. In MATLAB, `buildtool test` runs the C++ suites through ctest and
the four MATLAB suites (`tBridge`, `tRooms`, `tEnvelope`, `tShot`); `buildtool check` runs the
gesture check alone. From a shell:

    matlab -batch "addpath toolbox; trench.setup; r=runtests('Tests'); assertSuccess(r)"

## Banks

`banks/*.bank.json` are the factory banks, written by `trench.io.makeFactoryBanks` (`buildtool banks`):
P2K, Hillenbrand 1995, Klatt 1980, DVTD, X3, HEADS, XL-1, INSTRUMENTS. The musician's banks save
there too. Chord files for the tables and reads live in `native/python/workstation/chords`.

## Record

`DECISIONS.md` here, one entry per decision; `plugin/NEXT_SESSION.md` for the running brief.
