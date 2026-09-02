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

## Review handoff 2026-09-02 (read-only pass; nothing in plugin/ was edited)

Four blockers, each verified line by line against the working copy:
1. Body switch drops the whole chain to dry for the load window. PluginProcessor.cpp:549
   stores lastLoadOk=false at the top of handleAsyncUpdate; processBlock :289 returns the
   block untouched until :592 restores it. No filter, no INPUT desk, no 1.6107 voice gain,
   no ceiling in between. The snapshot swap (retireSnapshot) is already atomic, so the
   gate protects nothing. Fix: drop the gate; keep the previous snapshot playing until
   the new one is published.
2. Non-240-byte roster entries never load and leave lastLoadOk false forever.
   TrenchDspBridge.h loadCartridge (:120), loadRuntimePresetBank (:148), bodyBytesFromJson
   (:154) are stubs returning false; getLastLoadOk has no caller on the face. Fix: either
   every shipped body is 240 bytes and the stubs go, or a failed load keeps the previous
   body and the face says so.
3. The audio path redesigns the whole cascade every sample, per channel.
   TrenchDspBridge.h:223-233 interpolate_biquads per sample, then processSample :342-348
   encode_cascade + set_target + decode per channel: ~140 log/exp per stereo sample. KEY
   on adds transpose_cascade per sample (:228). Fix: interpolate the packed words per
   sample (they are already the encoded form), decode once per sample shared by both
   channels, apply the KEY ratio once per block.
4. The coefficient ramp never completes. core/src/audition.cpp:75-90 set_target resets
   remaining_ to kApproachSamples (256) on every call; called per sample it degenerates
   to a 1/256 one-pole that never reaches the target, and it is in samples, so MOVEMENT
   feels different at 44.1k / 48k / 96k. Fix: with per-sample word interpolation the
   runner needs no second smoother; if one stays, make it seconds and let it count down.

Should-fix, cited by the reviewer, not yet verified by hand:
- GraphDisplay.h:158,303-321 rebuilds a ~7 MB supersampled image every frame MORPH moves.
- probeCurrentBodyForUi (PluginProcessor.cpp:683-705) omits KEY, LOW KEEP and the voice
  gain, so the curve differs from the audio with KEY on.
- BodyBrowser preview then re-commit of the original: setSelectedItemIndex no-ops on an
  unchanged index, so the engine keeps the preview (PluginProcessor.h:82-88,
  TypeSelectorView.h:115-118).
- reclaim() (TrenchDspBridge.h:241-245) has no synchronises-with edge to the audio
  thread's acquire load; masked on x86, live on ARM64.
- Movement.h:212-217 random-walk catch-up loop is unbounded on a forward transport jump.
- rescanBodyRoster (TrenchBodyRoster.h:126-147) runs on every BODY click and rebuilds a
  static roster that parameterChanged reads on the audio thread.
- Oversized host block: chunks re-read the same block-start PPQ, MOVEMENT repeats
  (PluginProcessor.cpp:302-318, :366-372).
- processBlock before prepareToPlay writes morphBuffer[0] on an empty vector (:301).
- WheelControl.h:87-91 / BayKnob.h:47-51 / GlassWords.h:280: right-click ends a host
  gesture that never began.
- getTailLengthSeconds returns 0 (:135).
- Small: 0xe000 vs 0xDFFF identity gain word; DeskDrive float denormal threshold on a
  double; FOLLOW arm threshold mismatch; no state version tag; BODY range 0..511 with
  ~19 slots; dead code MorphMod.h, readUiSnapshot, most of SlamStage.h.

Solid, keep: word-space interpolation before decode; allocation-free, lock-free
processChunk; AUTO KEY on its own worker; rewarp always from source bytes; recall by name.

Tests to add: CPU budget assert; click-free body switch continuity; static MORPH
converges to the probed coefficients; block larger than prepared; prepareToPlay twice at
different rates and with samplesPerBlock 0; body recall incl. out-of-range and missing
id; failed-load path; transport start/stop/jump; mono; probe vs audio with KEY on.

Review test suite, built and run 2026-09-02: plugin/tests/ReviewTests.cpp, target
TRENCH_ReviewTests, ctest name trench_plugin_review (CMake block appended after the
TRENCH_Tests block, left UNCOMMITTED in plugin/CMakeLists.txt because that file also
carries this tree's other in-flight edits; commit it with them). Run from
out/build/vst3/plugin with TRENCH_HEADLESS=1. Measured:
- Engine budget: 0.039 s of engine per 1 s of stereo audio at 48 kHz with MORPH moving
  every sample; KEY on 0.047. Under the 5% budget, so blocker 3 above is a cost, not a
  blocker, at this rate. The threshold is in the test; tighten it when 3 is fixed.
- Ramp: one set_target then 256 samples lands exactly; set_target every sample (the
  bridge's pattern) is still 68% off after 512 samples with the countdown stuck at 255.
  Blocker 4 confirmed by unit test.
- Body switch under load: 18 of 1583 blocks passed through dry across 200 switches.
  Blocker 1 confirmed and measured.
- Failed-load path: every roster slot is a 240-byte body, so blocker 2 is latent, not
  reachable from the face today; the test reports it and skips.
- Oversized host block: 2048 = 4 x 512 exactly with MOVEMENT off; with MOVEMENT running
  peak diff 0.0116 (should-fix "chunks re-read the same PPQ" confirmed).
- Graph vs audio with KEY F#: worst 6.9 dB (1000 Hz heard -6.2, shown +0.7). Confirmed.
- prepareToPlay(rate, 0) then a 512 block: no crash, finite. Not reproduced.
- Rate change mid-session: finite and audible at 44.1k / 48k / 96k, BUT the same 220 Hz
  sine peaks 0.080 at 44.1k, 0.091 at 48k and 0.386 at 96k: +12.6 dB at 96k. New
  finding, not in the review; likely the open 48k recompile bug above at a wider rate.
- Body recall by id, out-of-range slot to NO FILTER, mono, refused install: all pass.
Failing today by design (5 checks): ramp x2, dry blocks, MOVEMENT chunking, KEY graph.
They are the acceptance tests for the fixes; do not loosen them.

Face rulings 2026-09-02 (Tyson): value boxes beside the wheels, the "(%)" labels, the
BODY row, the knobs and the wordmark all STAY, locked. Not copying E-mu's teal glass or
brushed silver verbatim; otherwise the X3 grammar is fine. Black glass, mint hairline,
champagne plate are the identity. Only polish left: dim the MOVEMENT / FOLLOW captions
in the glass so the curve owns it.

