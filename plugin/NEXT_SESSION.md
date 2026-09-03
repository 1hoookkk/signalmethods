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


## Fix pass 2026-09-03 (all 25 review checks pass, 0 failures; TRENCH_Tests 0 failures; trench_core passes)
- Dry gate: handleAsyncUpdate no longer clears lastLoadOk before a load; a failed load
  keeps the previous body live (bridge parses first, swaps only on success). 0 of 32584
  blocks dry over 200 switches under load.
- Ramp: CascadeRunner::set_target is a no-op for an unchanged target and re-aims over the
  remaining countdown instead of restarting it; new set_immediate takes raw coefficients
  when no ramp is running. Committed to native core as 79ffe61b.
- Bridge: the cascade is recomputed only when morph, Q, KEY or the body changed; the
  256-sample encoded ramp runs only on a body switch (snapshot generation counter).
  Engine 1.3% of real time at 48 kHz with MORPH moving every sample (was 3.9%); KEY on 1.8%.
- Oversized host block: each prepared-size chunk offsets the transport ppq by its start
  sample, so MOVEMENT no longer repeats the first chunk. Peak diff 0.
- KEY graph: probeCurrentBodyForUi transposes the probed cascade with the KEY ratio;
  worst 8e-5 dB against the audio.
- transpose_cascade measures each lane once (was twice) to hold KEY under 1.5x.
- Plugin files still carry the other chat's uncommitted edits; commit together.
- Still open: 220 Hz level +12.6 dB at 96 kHz vs 44.1 kHz (48k recompile bug, wider).
- 96 kHz level jump FIXED 2026-09-03 (core commit follows 79ffe61b): rewarp kept the raw
  scale word b0 while preserving bandwidth in Hz, so a resonant pole's gain rose with the
  rate (about 6.8 dB per pole per doubling; a bare resonator with real zeros took both).
  Now each rewarped section is re-levelled at its pole frequency (DC for real poles) against
  the datum response. 220 Hz peak 0.080 / 0.075 / 0.075 at 44.1 / 48 / 96 kHz (was
  0.080 / 0.091 / 0.386). Core rewarp test and the plugin 48k test now compare against the
  datum-rate response, not the un-levelled design; both pass at 0.05 and 0.07 dB.
- Known residual: MORPH interior is word-space interpolation, so at a foreign rate the
  interpolated pole lands elsewhere (crisp morph 0.5: section 0 pole 535 Hz at 44.1k,
  686 Hz at 96k; corners exact). About 1.3 dB at 1 kHz. Inherent to the rewarp-then-
  interpolate order; fixing it means interpolating at datum and rewarping per sample.
- Native app audition uses the same un-levelled design(); it plays a body louder at 96k
  than the datum. Not touched.
- MORPH interior at foreign rates FIXED 2026-09-03: the bridge snapshot now keeps the
  datum words; every recompute interpolates at the datum rate and rewarps that one
  section set to the host rate (rewarp_cascade). Crisp morph 0.5 section 0 pole 534.7 Hz
  at 44.1 / 48 / 96 kHz; response within 0.1 dB to 4 kHz. Recompute runs on a 32-sample
  tick with a linear coefficient glide across the tick (the X3's own cadence); body
  switches keep the 256-sample log ramp. Engine 0.4% of real time with MORPH moving every
  sample. Chunk-invariance and KEY graph checks still exact.
- Should-fix pass 2026-09-03, each verified by reading before fixing: preview then
  re-commit of the same body now reloads it (onCommit also restores); snapshot swap and
  audio-side load are seq_cst so reclaim() is safe on ARM; random-walk catch-up reseeds
  past 64 cycles; rescanBodyRoster is a no-op on the fixed roster (no more rebuild under
  the audio thread); processBlock before prepare returns dry instead of writing an empty
  morphBuffer; right-click no longer ends a gesture that never began (wheel, knob, glass
  word); tail length reports the slowest pole's t60; the graph trace image is reused and
  cleared instead of reallocated every frame. Review 25/25, plugin tests 0 failures,
  TRENCH_VST3 links. Plugin tree still uncommitted (shared with the other chat).

## Plate asset brief (Tyson, 2026-09-03 evening) — for the next plate render
Beige plate df2_panel_beige.png stays until a render meets this. A cool-grey grained
render was tried in the build and rejected ("does not look as good at all").
Keep the warm golden-grey base tone and the soft, organic light gradient falling from
top-left to bottom-right. Replace the flat procedural grain with an ultra-fine,
microscopic horizontal hairline brush, so fine you only see it when a soft specular
highlight glints off the chamfered channel. A semi-gloss, baked industrial enamel coating
over a solid cast chassis (warm olive-drab, slate, or warm putty). Zero digital noise or
sandpaper texture. The surface is silky-smooth to the eye, relying entirely on realistic
studio rim-lighting and ambient occlusion to show weight. The stamped groove retains
soft, liquid-like paint buildup in the recesses, giving the impression of physical tooling.
Build notes for whichever plate lands: same 828 x 1280 geometry as the beige (aperture
and slot positions), and the build still paints its own glass bezel and slot outlines
over the plate; those should come off so the asset's recesses show.
Tightened prompt with the measured geometry lives in plugin/PLATE_PROMPT.md.

## Face ruling 2026-09-03 night (Tyson): the GAIN-bay face is back, verbatim
Tyson compared the 29/30 August faces and chose the GAIN-bay one ("Bring it back
verbatim with our current plate"). PluginEditor.cpp/.h, UiLayout.h, ui/FaceplateView.h,
GlassWords.h, GraphDisplay.h, LabelsLayer.h come from 7884785f; BayKnob.h, Theme.h,
WheelControl.h from 5b63ef63. Editor 352 x 543 (kFaceLockedWidth/Height in Theme.h).
INPUT, BITE, OUTPUT in the carved GAIN bay; peak marker on the curve; Rail Switch and
FOLLOW lamps on the glass. Mint trace and mint lamp locked. Plate = the graded 06:59
render at 828 x 1280 (geometry within 0.3% of the beige, so the 7884785f layout seats
without change). Shims: Movement::kGrowlIndex and PluginProcessor::hasLivePhraseForUi
(returns false). Non-visual fixes re-applied on top: gesture flags (wheel, knob), trace
image reuse, commit-after-preview reload, probe through the curve tables. Face test now
expects BITE on the face. Review 25/25, plugin tests 0 failures, VST3 links.
The other chat's 315 x 516 face rebuild that was in the working tree is preserved at
evidence/face-rebuild-backup-2026-09-03/ (untracked).
- GAIN bay polish 2026-09-03 (Tyson "Do it" on the reviewer's two adjustments): captions
  centred over the knob-plus-pill span (BayKnob.h), rows centred inside the frame
  (rowY = room top + pad + half the row gap). Knob size left alone. Glass re-seated to the
  current plate's apertures in the 1010 x 1557 layout space; graticule at 0.45 opacity.
- Filed: evidence/patents/rossum_making_digital_filters_sound_analog.pdf (the paper behind
  BITE: saturate the delayed state before the multipliers, headroom in the accumulator).
- Glass inset 6 x 6.5 layout units inside the plate aperture so the plate's own chamfer
  frames it ("make it sit in the well a bit"). Knob wells tried and reverted.
- Tyson, 2026-09-03 night: "Bite is going to be the 3rd axis so it should maybe be a screen
  interaction." Open: BITE = the cube's Z axis; candidate is the glass word Z (GlassValue on
  ParamID::chew, as the 29 Aug face had) with the GAIN room keeping INPUT and OUTPUT.
  Not done; awaiting the ruling.
- BITE is the third axis, not a gain knob (Tyson, 2026-09-03 night). GAIN bay is INPUT and
  OUTPUT, two rows. BITE = drag the glass up or down (GraphDisplay owns a ParameterAttachment
  on chew; shift = fine, wheel steps, double-click resets); a "BITE nn" readout fades in
  beside the curve's peak during the gesture and fades out after. Nothing on the glass at
  rest. A glass word/track version was tried and rejected as clutter (GlassValue class kept
  in GlassWords.h, zWord constructed but hidden).
- One-time onboarding (ui/Onboarding.h), reworked on Tyson's "Dont grey it out. Just show
  the controls as you hover them": no scrim; on the first session, hovering the glass, MORPH,
  Q, the MOVEMENT chip or the FOLLOW lamp shows a small hint card (word + one line). Once
  all five have been hovered it waits 2.5 s and writes onboarding.axes=true to the TRENCH
  settings file (Signal Methods app-data). Suppressed under TRENCH_HEADLESS; forced with
  TRENCH_SHOW_ONBOARDING=1, and TRENCH_ONBOARDING_HOVER=<0..4> pins a card for FaceShot.
  Face test: BITE not a bay knob. Tyson: "Follow is the only weird control. And modulation
  isnt clear" - those two carry the longest hints.
- Onboarding simplified again (Tyson: "Make it simple. No extra text other than telling the
  user what to do... No text box with filled color", then "Give them a cute little animation
  drop in"): one mint line under the hovered control, action only ("drag up or down",
  "roll the wheel", "click"), no box, no outline; it drops in from 14 px above with a small
  settle over about 220 ms. Same five targets, same completion rule.
- Hint copy is the gesture plus its name ("drag up or down for BITE", "roll for MORPH",
  "roll for Q", "click for MOVEMENT", "click for FOLLOW"); 12.5 pt bold mint with a dark
  halo, no box; sits on the control's own dark surface (glass top, wheel drum, above the
  chip and lamp on the glass).
- Onboarding is the glass hint only (Tyson: "I dont like the extra ones. Just have the z
  axis"): "drag up or down for BITE" drops in on first hover of the glass, first session;
  once seen it is written and never shown again. MORPH, Q, MOVEMENT, FOLLOW carry no hint.
- BITE law rewritten 2026-09-03 night (Tyson: "Distortion is jarring"). Measured before:
  nothing to 0.5 then odd harmonics at one flat level (h3 = h5 at -47 dB), full BITE
  collapsed the output 12 dB with 32% THD. Now Rossum's law: the delayed state runs through
  ceiling*tanh(x/ceiling), ceiling 4.0 -> 0.35 across BITE (geometric), threshold 0.6 of the
  ceiling, and the pole radius is pulled DOWN under drive (r * (1 - 0.25 * excess)) so the
  resonance flattens and recovers. At 0.3 in: THD 0.4 / 1.2 / 4.0 / 22% at BITE 0.25 / 0.5 /
  0.75 / 1.0, harmonics falling in order. Hot input at full BITE still drops about 9 dB;
  that is the ceiling and is for the ear to judge. Core commit 3707640b. Probe:
  scratchpad bite_probe (sine 220 Hz through the runner, harmonics 1-9).
- Agent diff x3-clean vs native (2026-09-03 night): OUTPUT lost its dedicated SLAM law
  (SlamStage.h(A):5-15,49-120) and now reuses DeskDrive (PluginProcessor.cpp(B):484-501);
  KEY went from per-stage scale-degree snap (engine.rs:790) to whole-cascade ratio
  transpose (TrenchDspBridge.h(B):321-335); FOLLOW moved JUCE-side with 1 ms/80 ms
  timing; MIX, GROWL, TRACK, AGC gone; LOW KEEP, voice gain 1.6107, identity curve tables
  added. Open for Tyson: OUTPUT law, KEY meaning.
- Voice gain kX3VoiceGain (1.6107, +4.14 dB after the cascade) removed 2026-09-03 night
  (Tyson: "Its a fudge"). Tests that assumed it now expect unity. KEY stays as it is with
  the detector suggesting ("Key has real value and the neural network works").
- 2026-09-04 (Tyson "Yes" to all three): INPUT is clean gain 0..+24 dB into the cascade
  (readout in dB); OUTPUT is clean gain -24..+12 dB, unity at the knob's two-thirds, which
  is now the parameter default (readout in dB); the input and output DeskDrive stages and
  slamTrim are out of the path; LOW KEEP is gone from the path and the parameter list.
  Tests rewritten accordingly (INPUT +24 dB, OUTPUT +12 dB over unity at 0.1 in, both clean
  of harmonics). Installed.

## Dev build 2026-09-04 (Tyson: recording, bisection and baking live in a dev panel of a
separate VST dev build; he tunes shipping params there and has the final say)
- Target TRENCH_Dev (PLUGIN_CODE Tr0d, "TRENCH Dev.vst3"), built from
  TRENCHPluginCommonDev = the same sources with TRENCH_DEV_PANEL=1. Installed beside the
  ship build in Common Files\VST3. Ship build has none of it compiled in.
- Drawer ui/DevPanel.h (210 px, right of the plate). WHEEL LOOP: choose 1/2/4/8 bars, REC AT
  NEXT BAR arms; recording starts on the bar line while the host plays, samples the final
  MORPH trajectory at 96 ticks per beat, and flips to looping when the length is reached.
  LOOP replays absolute wheel positions locked to the bar it was recorded on (overrides
  base + MOVEMENT + FOLLOW). STOP hands the wheel back. SAVE writes
  plugin/patterns/loops/<name>.wheelloop (JSON: name, ticksPerBeat, beats, values); LOAD
  lists that folder. Engine side: dsp/WheelLoop.h, lock-free, called after the morph
  smoother in processChunk under #if TRENCH_DEV_PANEL.
- Not yet: bake step (loops -> shipping pattern table and Movement playing them), taper
  sessions, BITE law dials. Verdicts pending from Tyson: replace the 8 canned patterns with
  his loops; INPUT 0..+24 dB and OUTPUT -24..+12 dB ranges; loop resolution kept
  continuous (96/beat) vs quantised to 16ths at bake.

## Pole templates 2026-09-04 (Tyson: "Just plot the reocurring states pole only" ->
"Bring their pole templates into the native app" -> "Make them a general type template")
- evidence/research-results/pole_states_armadillo.py: poles only, binned 1/6 octave x 2 dB
  of R' on Rossum's ARMAdillo plane; the 33 X3 bodies grouped by the Mo'Phatt manual's
  types (LPF, EQ+, EQ-, VOW, PHA, FLG, REZ, WAH, DST, SFX; map in the script) read from
  evidence/factory-data/p2k/bodies/p2k.zip; the 289 Morpheus cubes by their decoded
  category. Outputs pole_states_by_type.png and pole_templates_by_type.json (states with
  hz, bw_hz, r, bodies sharing).
- Native app: native/app/pole_templates.{hpp,cpp} loads native/app/templates/
  pole_templates_by_type.json (path baked as TRENCH_POLE_TEMPLATES); a TEMPLATE combo at
  the top of the inspector seeds the editing corner's poles with the type's strongest
  shared states (top 6 by body count, no two within a quarter octave, sorted low to high),
  enabling exactly those sections; zeros untouched; one undo group. Morpheus states carry
  Hz values from the 39,062.5 datum; the app's Hz/bw geometry is rate-free so they seed as-is.
- Web archaeology (agent): Creative's Aug 2006 Emulator X2 release calls the Morph Filter
  Designer "filter creation tools that E-MU's sound designers have been using for years";
  SOS Aug 2006 says the same. New name: Bob Bliss, senior design engineer (SOS Oct 1995).
  Leads: Gearspace "EMU z-plane filters, that emu sound" thread (403 to fetch, needs a
  browser), NAMM oral histories (video only), USPTO search for Bliss. Notes in scratchpad.
- Dev roster 2026-09-04 (Tyson: "The presets in the plugin just suck. Make the dev only
  build load the 33 p2k"): plugin/presets/p2k/<name>.body240 = the 33 Proteus 2000 / X3
  bodies extracted from evidence/factory-data/p2k/bodies/p2k.zip (240-byte legacy layout,
  identical to the roster's); plugin/presets/PresetRosterDev.inc lists them with E-mu names
  and their Mo'Phatt type as the category, as absolute paths under TRENCH_TABLE_STITCH_ROOT
  (bodyRawBytes already loads absolute .body240 paths from disk). TrenchBodyRoster.h picks
  that roster under TRENCH_DEV_PANEL; the ship roster (18 WORKHORSE bodies) is untouched.
  Measured: roster bodies and the 33 both carry resonance in the high-Q corners only
  (Crisp at MORPH 68: +3 dB at Q 0, +17 at Q 50, +33 at Q 100; about 7 dB per quarter).
- Dev drawer 2026-09-04 (Tyson: "maybe let me draw the modulation"): the step grid is the
  one surface. StepGrid in ui/DevPanel.h: one bar per 16 steps (or 8/32 by the GRID
  choice), click or drag sets a step to the mouse height, shift snaps to eighths, alt fills
  random (E-mu's own tip); BLANK gives an empty 1/2/4/8-bar grid at the wheel's middle.
  Recording still works and lands in the grid quantised; LOAD fills the grid; every edit
  rebuilds the loop (hold or GLIDE). Drawer widened to 320 px. WheelLoop gained
  stepLevels / setSteps / blank; playback is smoothed by the X3's one-pole.
- Engine: CascadeRunner::set_glide now ramps the kernel row (c0..c4) and rebuilds the
  biquad per sample, as the X3 spec describes, instead of ramping b/a coefficients.
- Plugin tree committed as ship candidate 8a0a73e2; CLAUDE.md canonical 663c1498.
- AGC restored 2026-09-04 (Tyson: "thats why the sound was weak. all we need is agc and the
  0.1db safety limiter"). The X3 leveller verbatim from the DLL notes (runtime_hacks.md):
  after the cascade, per sample, one shared gain for stereo: index = (uint)(gain * |x|) & 0xF
  into the 16-float table 1.0001 1.0001 0.996 0.990 0.920 0.500 0.200 0.160 0.120 x8; gain *=
  table[index], reset to 1 when it would exceed 1; drive unity; table square-rooted once
  above 65 kHz and twice above 130 kHz. Sleeps below |x| = 2 (+6 dBFS), releases by 1.0001
  per sample. Lives in TrenchDspBridge::processTrajectory behind bypass.agc (default on);
  agcReductionDb() is real telemetry again. Safety ceiling kept as the silent guard.
  Engine 1.2% with MORPH moving; suites clean. Installed.
