# HEADSPACE

The authoring tool for TRENCH bodies, one screen. A hidden plane of 30 vowel anchors (12 Klatt 1980 vowels and 18 DVTD
tracts read from their model sounds by order-12 LPC at 11,025 Hz, the E-mu P2K method), every one
six pole pairs and nothing else,
laid out on an equal-hop triangular lattice by likeness. Drag anywhere on the plane and hear the
blend of the three anchors around you, stage to stage, pole 1 with pole 1 through pole 6.
The plane is painted by brightness, the gold mark shows where you are, and the three anchors around you are named beneath it; the lattice itself is not drawn. A MATLAB R2025b toolbox over two MEX files, `trench_bridge` (the
model) and `trench_audio` (the engine on the sound card). The plugin plays what this writes.

## Run

Double-click `HEADSPACE.cmd`, or in MATLAB:

    cd C:\Users\hooki\trench-native\native\workstation
    addpath toolbox
    trench.setup
    app = trench.launch;

PLAY, SAW, PINK NOISE. Drag the plane; plain arrows move a twentieth of a lattice edge, Ctrl+arrows
a hundredth. Keys 1 to 4 choose a corner without moving you; Ctrl+S saves the live sound into it;
Space plays. WRITE BODY FILE, once four corners are filled, writes
`plugin/presets/user/headspace_<stamp>.body240`. The corners persist in `banks/HEADSPACE.bank.json`.

## Data

`data/headspace-anchors.json` holds the 30 anchors with provenance per value (measured model sound
or published table); `trench.headspace.bakeAnchors` writes it from the Klatt bank and the DVTD
model-sound WAVs through `trench.headspace.readSound`. `data/headspace-layout.json` is the stored lattice arrangement;
`tools/arrange_headspace.py` computes it (seed 19801995). Neither is rebuilt at launch.

## Build

The MEX files come from the plugin's CMake tree through MSVC vcvars64:

    build_mex.cmd

or `buildtool mex` in MATLAB. The build copies `trench_bridge.mexw64` and `trench_audio.mexw64`
into `toolbox/mex`; MATLAB must not hold the old files (close HEADSPACE or `clear mex` first).

## Test

All headless, figures invisible. `buildtool test` runs the C++ suites through ctest and the four
MATLAB suites (`tBridge`, `tHeadspace`, `tEnvelope`, `tShot`); `buildtool check` runs `tHeadspace`
alone. From a shell:

    matlab -batch "addpath toolbox; trench.setup; r=runtests('Tests'); assertSuccess(r)"

## Record

`DECISIONS.md` here, one entry per decision; `plugin/NEXT_SESSION.md` for the running brief.
