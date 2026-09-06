# HEADSPACE

The authoring tool for TRENCH bodies, one screen. The surface shows every vowel corner: the 24
corners of the six P2K vowel bodies, the schwa, the 12 Klatt 1980 vowels and 18 DVTD tracts read
by the E-mu P2K method (order-12 LPC at 11,025 Hz), placed by ROOT and F2 AND ABOVE. Click a
square to hear that corner exactly. Keys 1 to 4 place it into M0 Q0, M1 Q0, M0 Q1, M1 Q1. The
pad plays the plugin's own MORPH by Q lerp of the four you placed, word for word what WRITE BODY
FILE exports. A MATLAB R2025b toolbox over two MEX files, `trench_bridge` (the model) and
`trench_audio` (the engine on the sound card).

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

`data/headspace-anchors.json` holds the fitted vowel corners with provenance per value (measured
model sound or published table); `trench.headspace.bakeAnchors` writes it from the Klatt bank and
the DVTD model-sound WAVs through `trench.headspace.readSound`. The P2K corners come from the library.

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
