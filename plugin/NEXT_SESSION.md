# TRENCH plugin - next session brief (2026-08-29 wrap)

## State
- Face (2026-08-30): 352x543 on the full plate. Lower section = one permanent GAIN carve (INPUT > BITE > OUTPUT, 36 px knobs, 42 px rows, GAIN engraved in the frame break). MOVEMENT is a chip on the glass (GlassWords: click = punch in/out keeping the pattern, wheel = step, double-click = list; GROWL disabled, LIVE gated); FOLLOW is a lamp-word beside it (FollowLamp: on = envAmount 0.6, off = 0; depth to be bisected by ear). No doors, no blank state. Off the face: LOW (lowKeep still read live), TRACK, GROWL, DIVISION. BITE bound to chew; GRIT needs native/core hook. No MIX, no COLOR. BODY, glass (locked 6x log graticule, 1 px mint X3 trace, legend dot only while Modulation is OFF), MORPH, Q (SS3 master body, lamp re-laid in the curve mint, no glow outside the recess), KEY = OFF with the heard key offered beside it. COLOR placeholders deleted. Modulation legend only while a preset is live. FaceShot renders trench_face (defaults) and trench_face_active (Crisp, MORPH 68, Q 30, KEY C, Rail Switch, FOLLOW on, INPUT 35, BITE 25, OUTPUT 70) at 1x and 2x. Test knobs: TRENCH_WHEEL_STRIP, TRENCH_SHOT_SCALE, TRENCH_MORPH.
- Audio path today: y = SLAM(1.6107 * H(body, morph + FOLLOW, q, key) * DESK(x, INPUT)) + LOW floor. Tests: INPUT +40 dB and harmonic ratio 0.45 (0 at INPUT 0), OUTPUT 6.2 dB, LOW -4.3 dB, FOLLOW pushes MORPH 1.0 wheel unit on a transient. No ceiling is executed.
- KEY: choice 0 = OFF; 1..12 minor C..B, 13..24 major. Bridge transposes the interpolated cascade by the root's distance from C (authored key assumed C - bodies carry no key). Test: F# shifts Crisp by -5.5 dB at 220 Hz.
- Bridge must call encode_cascade before CascadeRunner::set_target (runner is in the encoded domain since 072bc86; Cascade and EncodedCascade are the same array type, so a raw cascade compiles and produces garbage).
- Tests: plugin/tests/PluginTests.cpp (TRENCH_Tests, CTest "trench_plugin" from out/build/vst3/plugin). TRENCH_MEASURE=<body> TRENCH_RATE=<hz> prints measured vs coefficient response.
- FaceShot renders default + active states at 1x and 2x.

## The open DSP bug (measured, not fixed)
At host 48 kHz the .body240 recompile (import_p2k -> export_p2k_body at 48k) re-quantises roots to the P2K lattice and the low octaves change filter: Crisp morph 0 @120 Hz -4.4 dB (44.1k) vs -8.8 dB (48k); morph 0.5 @500 Hz -7.3 vs -3.1. Fix: at rate != datum design biquads from the physical corner at the host rate (workstation path: packed_interior_corner -> design), never round-trip through words. Bodies are authored at 44.1k only.

## Decisions Tyson owns
- Roster. Proposed: BODY, MORPH, Q, MOVEMENT, OUTPUT (level-compensated desk stage, the only real character), KEY. INPUT folds into OUTPUT drive. COLOR slots are placeholders.
- The fixed +4.14 dB voice gain: parity or inherited loudness.
- LOW: control or gone.
- Wheel tooth-at-rim in the SS3 render: fix upstream in Blender (df2/dev/tmp/thumbwheel_blender/user_live_wheel_locked_spin_embedded_cobalt_257.blend) or accept.

## Install
Copy out/build/vst3/plugin/TRENCH_artefacts/Release/VST3/TRENCH.vst3/Contents/x86_64-win/TRENCH.vst3 over C:/Program Files/Common Files/VST3/TRENCH.vst3/Contents/x86_64-win/. Fails while FL has it loaded.
