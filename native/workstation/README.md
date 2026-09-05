# HEADSPACE

The authoring tool for TRENCH bodies, one screen: a field of frames (Klatt 1980 vowels, DVTD
vowels, head notches) laid out by ROOT and dB, a log blend of the three frames around the mark, four corners, Ctrl+S to save
what you hear into the current corner, WRITE BODY FILE for the plugin. A MATLAB R2025b toolbox
over two MEX files, `trench_bridge` (the model) and `trench_audio` (the engine on the sound
card). The plugin plays what this writes; nothing here changes the ship face.

## Run

In MATLAB:

    cd C:\Users\hooki\trench-native\native\workstation
    addpath toolbox
    trench.setup
    app = trench.launch;

Drag the field or use the arrows: left and right step by ROOT, up and down by dB, Shift nudges
1 semitone or 1 dB, Ctrl 0.2. Keys 1 to 4 choose the
corner, Ctrl+S saves the live sound into it, Space plays. WRITE BODY FILE when four corners are
filled writes `plugin/presets/user/headspace_<stamp>.body240`.

## Build

The MEX files come from the plugin's CMake tree and must be built through MSVC vcvars64:

    build_mex.cmd

or in MATLAB `buildtool mex`. The build copies `trench_bridge.mexw64` and `trench_audio.mexw64` into
`toolbox/mex`. MATLAB must not hold the old files: close the workstation, or run `clear mex`, before
the copy.

## Test

All headless, figures invisible. In MATLAB, `buildtool test` runs the C++ suites through ctest and
the four MATLAB suites (`tBridge`, `tHeadspace`, `tEnvelope`, `tShot`); `buildtool check` runs the
gesture check alone. From a shell:

    matlab -batch "addpath toolbox; trench.setup; r=runtests('Tests'); assertSuccess(r)"

## Banks

`banks/HEADSPACE.bank.json` holds the four corners saved with Ctrl+S. `banks/*.bank.json` are the factory banks, written by `trench.io.makeFactoryBanks` (`buildtool banks`):
P2K, Hillenbrand 1995, Klatt 1980, DVTD, X3, HEADS, XL-1, INSTRUMENTS. The musician's banks save
there too. Chord files for the tables and reads live in `native/python/workstation/chords`.

## Record

`DECISIONS.md` here, one entry per decision; `plugin/NEXT_SESSION.md` for the running brief.
