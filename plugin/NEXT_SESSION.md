# TRENCH plugin - next session brief (2026-08-29 wrap)

## State
- Face: BODY, glass (Modulation chip), MORPH, Q, COLOR 1/2/3 (bound to bite/follow/track - audio-dead), KEY picker top-right. UI is NOT ship-ready (Tyson).
- Audio path today: y = 1.6107 * H(body, morph, q, key) * x. Hidden INPUT/OUTPUT/LOW are forced neutral in processChunk while off the face. No ceiling is executed.
- KEY: choice 0 = OFF; 1..12 minor C..B, 13..24 major. Bridge transposes the interpolated cascade by the root's distance from C (authored key assumed C - bodies carry no key). Test: F# shifts Crisp by -5.5 dB at 220 Hz.
- Bridge must call encode_cascade before CascadeRunner::set_target (runner is in the encoded domain since 072bc86; Cascade and EncodedCascade are the same array type, so a raw cascade compiles and produces garbage).
- Tests: plugin/tests/PluginTests.cpp (TRENCH_Tests, CTest "trench_plugin" from out/build/vst3/plugin). TRENCH_MEASURE=<body> TRENCH_RATE=<hz> prints measured vs coefficient response.
- FaceShot renders trench_face.png / _200.png only.

## The open DSP bug (measured, not fixed)
At host 48 kHz the .body240 recompile (import_p2k -> export_p2k_body at 48k) re-quantises roots to the P2K lattice and the low octaves change filter: Crisp morph 0 @120 Hz -4.4 dB (44.1k) vs -8.8 dB (48k); morph 0.5 @500 Hz -7.3 vs -3.1. Fix: at rate != datum design biquads from the physical corner at the host rate (workstation path: packed_interior_corner -> design), never round-trip through words. Bodies are authored at 44.1k only.

## Decisions Tyson owns
- Roster. Proposed: BODY, MORPH, Q, MOVEMENT, OUTPUT (level-compensated desk stage, the only real character), KEY. INPUT folds into OUTPUT drive. COLOR slots are placeholders.
- The fixed +4.14 dB voice gain: parity or inherited loudness.
- LOW: control or gone.
- Wheel tooth-at-rim in the SS3 render: fix upstream in Blender (df2/dev/tmp/thumbwheel_blender/user_live_wheel_locked_spin_embedded_cobalt_257.blend) or accept.

## Install
Copy out/build/vst3/plugin/TRENCH_artefacts/Release/VST3/TRENCH.vst3/Contents/x86_64-win/TRENCH.vst3 over C:/Program Files/Common Files/VST3/TRENCH.vst3/Contents/x86_64-win/. Fails while FL has it loaded.
