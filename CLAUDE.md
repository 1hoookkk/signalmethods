# TRENCH

This file is canonical. AGENTS.md is a stub; do not rely on it.

## What this is
- `plugin/` is the TRENCH VST3, "a musical filter by Signal Methods". It ships.
- `native/workstation/` is HEADSPACE, the authoring tool: one screen, a strip of frames with a log
  lerp between them, four corners, Ctrl+S saves what you hear, WRITE BODY FILE for the plugin.
  A MATLAB R2025b toolbox (`toolbox/+trench`) over two MEX files, `trench_bridge` (the model in
  `native/workstation/Source/model` plus the readers) and `trench_audio` (the engine on the
  sound card). `native/core` is the C++ engine both products share. Run it with `trench.setup`
  then `trench.launch`; see `native/workstation/README.md`.
- `evidence/` is the ONLY evidence root. Bodies, ROM dumps, manuals, patents, papers and
  research results live under `C:\Users\hooki\trench-native\evidence`. Do not go to
  trench-x3-clean or other repos for evidence; if something is needed from there, copy it
  into `evidence/` first.
- Keep work in the product the task names. Do not modify unrelated work.

## The filter, in one paragraph
The Z-plane filter is a SERIAL cascade of second-order sections: 6 sections (12th order)
for the Proteus 2000 / X3 bodies, 7 sections (14th order) for the Morpheus cubes. Section
gains multiply; responses add in dB. There is no parallel bank, no band summing, no
per-band normalisation. Say "serial cascade" before writing anything about level.
A body is a cube: 4 corners (P2K, 240 bytes, morph x q) or 8 corners (Morpheus, morph x
q x z). MORPH and Q are the raw cube axes, played straight; modulation moves MORPH only.
Every body carries a datum sample rate: P2K/X3 44,100 Hz, Morpheus 39,062.5 Hz. The
engine rewarps to the host rate from that datum.

## Body sources
- 33 Proteus 2000 / X3 bodies: `evidence/factory-data/p2k/bodies/p2k.zip`, extracted for
  the dev build at `plugin/presets/p2k/`. The hand-voiced standard.
- 289 Morpheus cubes: `evidence/factory-data/morpheus/` (raw stream, per-cube 560-byte
  native bodies, decoded JSON, six categories). Datum 39,062.5 Hz. Each record carries its
  own per-corner gains; do not apply the P2K DC-unity rule to them.
- 18 `xml_*` WORKHORSE bodies in `plugin/presets/bodies/`: Emulator X Morph Filter
  Designer compiles, one-dimensional (Q corners are copies). Sketches, not the standard.
- There is no E-mu hardware in the room: the Morpheus material is its firmware and the 289 cubes. Level truth comes from code (firmware gain path, the X DLL H-chip table, the proven P2K rule), never from captures.

## Builds and tests
- Every cmake build must run through MSVC vcvars64; a bare shell fails with C1083. Use a
  .cmd wrapper that calls
  `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat`.
- Plugin tree: `out/build/vst3` (Ninja, Release). Targets: TRENCH_VST3 (ship),
  TRENCH_Dev_VST3 (dev build with the drawer and the 33-body roster), TRENCH_Tests,
  TRENCH_ReviewTests, TRENCH_FaceShot (headless face render).
- Workstation: `native/workstation/build_mex.cmd` (or `buildtool mex` in MATLAB) builds
  trench_bridge, trench_audio, TRENCH_WorkstationTests, trench_core_tests and
  trench_core_from_audio_tests in the same `out/build/vst3` tree and copies the MEX files into
  `native/workstation/toolbox/mex`; MATLAB must have released the old files first. ctest names:
  trench_core, trench_core_from_audio, trench_workstation (C++), trench_workstation_bridge,
  trench_workstation_headspace, trench_workstation_envelope, trench_workstation_shot (each
  `matlab -batch` on a `matlab.unittest` class in `native/workstation/Tests`).
- All test runs headless. Never open windows on the user's screen. Tests are acceptance
  tests: fix the code, never loosen a threshold.
- Install: copy the built .vst3 over `C:\Program Files\Common Files\VST3\...`; if FL holds
  it, rename the old file aside with an `.inuse-old-<stamp>` suffix and copy.

## Rulings that stand
- The face is locked (plate, wheels, value boxes, labels, BODY row, knobs, wordmark, mint
  trace and lamp). BITE is the third axis, a drag on the glass, never a third gain knob.
  INPUT and OUTPUT are clean gain. No MIX, no LOW KEEP, no transpose knob.
- Everything must earn its keep on sound and control; never defend a stage because it is
  implemented or was there before.
- Authoring, recording, bisection and baking live in the dev build's drawer; the ship face
  carries none of it. Tyson tunes shipping parameters there and has the final say.
- Commit native work with explicit pathspecs; other chats stage plugin/ in this checkout.
- No code comments, in any language.

## Where the record lives
- `plugin/NEXT_SESSION.md`: the running brief, one entry per decision and measurement.
- Persistent memory: `C:\Users\hooki\.claude\projects\C--Users-hooki-trench-authoring\memory\`.
