# TRENCH

This file is canonical. AGENTS.md is a stub; do not rely on it.

## What this is
- `plugin/` is the TRENCH VST3, "a musical filter by Signal Methods". It ships.
- `native/workstation/` is HEADSPACE, the authoring tool: a JUCE app in the plugin's CMake
  tree, built alone by the `headspace` preset, linked straight to `native/core`, everything
  painted by hand in one monospace font on black. A body is four sounds; the tool chooses
  them and plays what the chip does between them. The stage is the body: corners A B C D are
  M0 Q1, M1 Q1, M0 Q0, M1 Q0, each with a name box; PRESET loads a factory body; the puck is
  the plugin's MORPH and Q and plays `PackedBody::interpolate_words` of the four; the rails
  are the palette, click a card to hear it, slide along a rail to morph to its neighbour,
  drag a card onto a corner; drop a .wav to read it; Ctrl+S keeps what you hear; W writes
  `legacy_bytes`. Nothing transforms on the path, the tool passes no verdicts, the ear
  decides. Read `native/workstation/HEADSPACE_SPEC.md` before touching it.
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
- Workstation: `native/workstation/build_headspace.cmd` configures the `vst3` preset, builds
  the `headspace` build preset (TRENCH_Headspace_App and TRENCH_QuadTests only) and runs
  `ctest --preset headspace` (test name trench_quad). The app is
  `out/build/vst3/plugin/workstation/TRENCH_Headspace_App_artefacts/Release/HEADSPACE.exe`;
  close it before relinking. No MATLAB, no MEX.
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
