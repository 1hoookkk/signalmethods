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
- INPUT desk back 2026-09-04 (Tyson "yes" to the desk on INPUT): the Mackie DeskDrive
  before the cascade again, driveTaper on the curve-mapped preamp, the clean +24 dB gain
  gone. OUTPUT stays clean gain -24..+12 dB. Tests: INPUT at full > +20 dB over unity and
  adds harmonics; INPUT at 0 clean.
- 2026-09-04 (Tyson): AGC lookup smoothed, linear between the 16 entries on the fractional
  index and clamped at 15 (no wrap); threshold stays |x| = 2 (+6.02 dBFS), floor 0.12
  (-18.4 dB). BITE defaults to 18 %. OUTPUT is -12..+12 dB with unity at 50 (Tyson:
  "output defaulting at 67" was wrong). Installed, suites clean.
- Morpheus level law read from the Vulcan firmware by agent; filed as
  evidence/research-results/morpheus_level_law.md. Corner gain x DC-normalised sections,
  cascade DC at 0 dB, no trim. The Morpheus import can now be built on it.

## Session close 2026-09-04 ~03:00 (context nearly full; next chat starts here)
State: ship plugin and TRENCH Dev both installed and committed. Engine: rate-independent,
X3 AGC after the cascade (smoothed lookup, +6.02 dBFS in, -18.4 dB floor), safety ceiling
as guard, INPUT = Mackie desk before the cascade (0 = clean unity), OUTPUT clean gain
-12..+12 dB unity at 50, BITE default 18 % (Rossum law: tanh state, pole pulled down),
kernel-row glide on the 32-sample tick. Gain path at defaults is clean unity (test
"PREAMP 0 = unity through the path" passes).
App (native): TEMPLATE picker (X3 types, Morpheus categories, LADDER piano rungs,
SOUNDBOARD stack) and now a FRAME picker: native/app/templates/frames_x3.json holds all
132 corners of the 33 bodies as frames (130 distinct); picking one drops that corner's
exact words onto the editing corner (applyFrame via import_p2k + documentFrom). "SLOT ->
TRENCH DEV" checkbox writes the working body to plugin/patterns/audition_slot.body240 on
every change (120 ms debounce); the dev roster's "Audition Slot" entry (AUTHORING) points
at that file and the processor's absolute-path hot-reload picks it up: voice in the app,
hear it in the DAW.
Rulings this session (all in memory): serial cascade, never parallel; no fitting, overlays
or RMS; modulation = MORPH only, additive, never locks; earn their keep; E-mu worked in
frames and intervals; CLAUDE.md canonical; evidence/ the only evidence root; no hardware.
Evidence filed: fingerprints sweep, Morpheus level law (from firmware), vowel anatomy,
overlay experiment, pole-state templates, X3 manual, Laroche patent, RBJ paper.
Next (agreed, not built): frame room + grid room layout in the app (frames as the unit,
library seeded from all corners; grid assembler with drop-on-corner); eight corners;
section types LP/HP/EQ/notch; display level-math switch; Morpheus import into the dev
roster using the firmware level law (560-byte bodies, datum 39062.5, corner gain after
the cascade, per-section DC normalisation as flagged); frames from Morpheus (needs 7
sections in the app); taper bisection sessions; bake loops into the ship pattern table;
Theo Lovejoy contact; C ABI for a Python bench (marimo as bench only, app stays the tool).

## App session 2026-09-04 (after the 03:00 close): grammar, templates, rooms
- Tyson: the app layout "sucks"; keep 6 stages; asked "8 corners? 4 and 3d cubes";
  "maybe emu use a higher abstraction"; "vowels ... are just 1 low section and 5 peq".
- FOUND: E-mu's productised authoring grammar, decoded from EmulatorX.dll (Morph Filter
  Designer, FUN_1802c6590; evidence/research-results/disassembly/ghidra-extracts/
  morphdesigner_types.md; its 68 factory templates compiled to 240-byte bodies at
  evidence/factory-data/emulator-x/templates with index.json): per section a TYPE (1 EQ:
  pole and zero on one angle, radius words split rad +/- gain; 2 LP: pole at the frequency,
  zero parked at the top; 3 HP: zero parked at the bottom, pole at the frequency) plus a
  frequency byte and a gain byte at endpoint A and B. No Q dial (rad = 0x76 + freq*0x7c>>8).
  Endpoint A fills corners 0 and 2, B fills 1 and 3: Q axis collapsed. 151 EQ / 83 LP /
  29 HP sections across the 68 templates; "Voxxy LP 2" = LP EQ EQ LP EQ.
- BUG behind "why is this vowel template so far down" and "flangers are very wrong":
  applyPoleTemplate seeded poles only and left every zero where the last body had it; a
  pole without its zero is a low-pass, six in series fall 30 dB. Fixed (Opus executor,
  uncommitted): native/app/pole_templates.{hpp,cpp} gain RowType {EQ, LP, HP}, TypeRow,
  typeRowsFor (shared states -> EQ rows at +12 dB seed, top state -> row 6 LP),
  applyTypeRows (EQ: zero on the pole, bw x 10^(gain/20); LP: unit-circle zero at 20 kHz
  via mag_word_for + kS6ZeroRsqWord; HP: real zero pair at 1 Hz). Types with fewer than 3
  shared states (FLG 1, PHA 2) leave the picker; their corners stay in FRAME. Tests:
  template_pick_seeds_the_editing_corner_poles extended, type_rows_compile_eq_lp_hp new
  (500 Hz EQ row measures +10.3 dB over 250 Hz). Native suite 30/30. App relinked
  (vcvars64 wrapper; VsDevCmd misses the SDK lib path, LNK1181 shlwapi).
- The two P2K flangers decoded: Angelz Hairz = high-Q poles with zeros spread wide;
  Dream Weava M0 Q0 = five descending pole/zero pairs 16.5k..5.9k then LP. Hand ladders,
  not combs.
- Cube recipe pasted from another tool (8 corners = 1 voiced pair x sharpen x transpose)
  checked against the censuses by agent: seed, not law. 0/66 P2K corner pairs share a
  one-dial byte delta; Q axis radius-dominant (98% radii change) but 62% of poles move
  >1/4 octave; Morpheus Frequency axis median 3.3 octaves up (IQR 1.2-5.0), rigid block
  only 26%. Verified: firmware level law (D(1) normaliser gated by payload byte 316 bit 0,
  cascade DC 0 dB, corner gain is the level). Open: morpheus_axis_census.py maps
  Morph=bit2, morpheus_level_law.md says bit1; resolve before any corner-index claim.
  "560-byte bodies" = the core's own native PackedBody (8 x 7 x 5 words x 2), not a
  Morpheus record (332 bytes: 12 name + 320 payload).
- RULING (Fable, one decision; Tyson has not overruled): body = 4 corners x 6 sections;
  a 3D cube in the app = the core's 8 corners seen as two squares with a FACE fader (z);
  export bakes one 240-byte square; Morpheus import = two squares, an idle 7th section
  dropped, a live one flagged; the plugin never grows a third wheel.
- Pasted "replace the AGC with a Morpheus quadratic soft clipper": refused. AGC = the X3
  leveller (Tyson's ruling). Firmware output clip is real (linear to 2^30, quadratic to
  2^31) but a DAC guard; candidate law for the silent safety ceiling later.
- marimo vs Qt: Qt stays the tool, marimo the bench over the C ABI.
- Peevers folder (emu-sgi-1993): Spectrogram, 1995 SGI build of a 1993-94 Berkeley thesis
  project; contextual only; no change.
- Layout: two rooms mocked as an artifact for verdict,
  https://claude.ai/code/artifact/959eca8f-0491-4983-8d7a-68424dcb8aa5 (scratchpad
  trench_rooms.html). Frame room: library (132 corners + 68 Designer + Morpheus later),
  fixed-grid plot, six drawbar rows TYPE / FREQUENCY / GAIN with Q following until
  unlocked, row 6 LP; frame card; drop-on-corner. Grid room: the square with seeds on
  the edges (SHARPEN on Q, COPY/POSTURE/TRANSPOSE on MORPH), pad dot = plugin wheels,
  PATH meter, level-math switch, + TOP FACE and FACE fader for 3D. Awaiting Tyson.
- Rooms mockup REJECTED (Tyson: "same clutter"). Method change: no more pictures; strip
  the real app to E-mu's Morph Designer panel (X3 manual p.149-151, memory
  morph-designer-grammar): one STAGE at a time (1-6 buttons), SHAPE Off/EQ/LP/HP, LO MORPH
  freq+gain, HI MORPH freq+gain (knobs with typed boxes), over the fixed-grid plot and the
  MORPH x Q pad; the pad's Q half picks the Q0 or Q100 pair being edited; free zero / cut /
  real poles behind one UNLOCK. Drawbar columns and the inspector column go.
- Tyson pasted "Goodwin and Massie decomposed audio into damped sinusoids ... top 6
  formant peaks" then "lets try that approach tho". Evidence: Goodwin's E-mu summer 1996
  is documented (1999 IEEE bio) but no artifact ties his matching pursuit to any body;
  treated as engineering, not history. Building FROM AUDIO (Opus executor): core
  body_from_audio (WAV reader, long-term LPC order 2n+6 via Levinson-Durbin, roots via
  Eigen, prominence over the envelope median -> gain, top 6 -> EQ rows + top as LP through
  applyTypeRows), FROM AUDIO button + .wav drop, dashed envelope ghost on the plot; tests
  on damped sinusoids, noise through resonators, WAV round trip.
- The Designer is 1-D (Tyson: "morph designer is 1d you know that right"): endpoint A
  fills corners 0 and 2, B fills 1 and 3; the Q pair is E-mu's hand layer on top,
  sharpening only in the vowel bodies; in the app SHARPEN builds it.
- FROM AUDIO landed (Opus executor, uncommitted): core body_from_audio.{hpp,cpp} (WAV reader
  incl. WAVE_FORMAT_EXTENSIBLE, long-term Hann-framed autocorrelation, Levinson-Durbin, roots via
  Eigen, prominence over the envelope median -> gain 3..24 dB, model order 2n+18 because order
  12 merges 250 and 1200 Hz into one 589 Hz pole), app button FROM AUDIO + .wav drop, dashed
  envelope ghost on the plot (CascadePlot::setReference/clearReference/hasReference),
  MainWindow::seedFromAudio -> applyTypeRows (EQ rows + top as LP). Tests 33/33 (31 baseline):
  damped sinusoids 250/1200/2800 recovered 250.3/1203.1/2806.9 Hz; noise through three
  resonators 250.4/1197.5/2762.2 Hz, bw 40.6/75.7/228.5 vs 40/90/180 (inside +-40%). A short
  click-and-ring clip does not carry its decay in the first `order` lags, so the bandwidth
  check lives on the sustained clip; seeds, Tyson dials. Core tests: sibling executable
  trench_core_from_audio_tests.
- Goodwin paste fact-checked: 1999 paper is Goodwin & Vetterli (Berkeley); E-mu appears once
  (author bio); Massie thanked in the 1997 dissertation; no artifact ties matching pursuit to
  any cube. Damped-sinusoid sum = parallel model: poles carry to the serial cascade, levels
  do not (heights come from the zeros).
- Tyson: "maybe we need to start from first principles". Fable's five: body = 4 x 6 serial
  sections in 8-bit words, DC unity + cuts; plugin plays MORPH and Q; a row = note, ring,
  height (E-mu's tool: ring follows the note until touched); corners are hand work from three
  sources (type, bank corner, recording) with seeds for the partner; hear it now. Surface
  proposed: a six-row TABLE (TYPE, LO note/ring/height, HI note/ring/height, every cell a
  draggable typed number, RING grey while it follows the rule) vs E-mu's one-stage panel.
  Decision pending from Tyson; both specs in the scratchpad (stage_panel_spec.md,
  stage_table_spec.md).
- Tyson: "what does matlab do?" / "lets just look at how matlab would do this" / "is there
  matlab source code" (none in evidence; mpm.exe is MathWorks' package manager) / "the faders
  are the real friction point." RULING applied: no faders. Building (Opus executor, spec
  scratchpad/stage_table_spec.md): RowsTable (six rows: TYPE combo, LO note/ring/height, HI
  note/ring/height as NumberBox cells: drag, wheel, keys, double-click to type; unlock "..."
  per row reveals OFFSET/CUT/POLE/HARM; ROOT cell above), plot handles on the editing corner
  (drag note/height, wheel = ring, double-click births an EQ row), ring rule rsq = 0x76 +
  (mag*0x7c>>8) followed until touched with a constant-Q test that must report its four Q
  values, LO/HI = the Morph pair picked by the pad's Q half, corner picker buttons and the
  splitter gone, row_table.* deleted, every test ported by name map, thresholds kept.
- Tyson: "surely emu used this" (Peevers Spectrogram panel). Yes: the binary carries
  "Copyright (c) 1995 E-mu Systems" and its README's Env mode is a 12th-order LPC envelope
  (six pole pairs) on a LogF surface, saved and re-applied as a filter; symbols lpcenv, Peaks,
  gal/lattice, FOF freq/bw/amp. E-mu-owned six-resonance analysis, not a cube editor. Addendum
  filed in emu-sgi-1993/REPORT.md; memory emu-tooling-witnesses. FROM AUDIO stands on it.
- Tyson: "i wanna try their program" / "Go" / "I wanna try the lpc model on it" / "Go to the
  xtreme lead bank and get a sample". Launcher Launch_SGI_Spectrogram.bat (repo root) boots
  MAME 0.289 indy_4610 (R4600, xl24) from runtime/state/irix53-spectrogram.chd (working copy)
  with runtime/spectrogram-sounds.iso in the CD-ROM; steps in runtime/HOW_TO_RUN.md. Git Bash
  mangles `start /D` (the "Error" Tyson saw); launch through PowerShell Start-Process or
  double-click. Booted to the IRIX login (root, no password); headless check with the disc
  attached exits clean. Disc: the program + README + sounds/ (22.05 kHz mono AIFF, 4 s):
  test-vowel-ah, Iowa cello/bassoon/alto sax/trombone, 303 sweep A, and three Xtreme Lead-1
  waves from the Digital Sound Factory Kontakt library in Downloads (Aud Lead 2 G3 = 30 ms
  loop, Aud Sync 1 C3, Vox Chord Gm7). No XL-1 ROM dump in evidence.
- LPC side by side (python replica of core body_from_audio; E-mu Env = order 12 at 22.05k
  per the README, TRENCH = order 30 at the file's rate), hz/bw(prominence dB):
  vowel_ah: E-mu 648/94 1071/89 2614/219 | TRENCH 643/102 1066/107 2651/166 (same formants);
  bassoon: 479/165 vs 493/178; trombone: 538/325 1002/621 1863/999 vs 522/317 969/552 1808/873;
  cello: no strong peak under either (broad -1..-9 dB); XL-1 Vox Chord: 523/303 vs 449/347.
  Order 12 at 22k and order 30 at 44k agree on every strong formant; they disagree only on
  the weak high bands, where seeds do not matter.
- Tyson: "Plot them and prove it" -> evidence/research-results/emu-sgi-1993/lpc_env_compare.{py,png}:
  nine sounds, averaged spectrum (grey) with E-mu's Env (LPC 12 at 22.05 kHz, blue) and TRENCH
  FROM AUDIO (LPC 30 at the file rate, orange), both relative to their median, six picked
  resonances marked. Same peaks on every sound with a real formant (sung ah 648/1071/2614,
  bassoon 479, trombone 538/1002/1863, sax 560, XL-1 Vox 523); cello is a tilt under both;
  the 30 ms XL-1 loop is too short for either. Sent to Tyson's phone. Mouse in MAME is off by
  default: launcher now carries -mouse. Emulator closed on request ("Exit it for me").
- Tyson: "Look at the lead one!" / "These are the real deal". XL-1 Aud Lead 2: the +37 dB
  peak tracks the key at 7.9 x f0 (G1 388, G2 769, G3 1554, G4 3115, G5 6209 Hz), Q 23-63,
  off the harmonic grid: a key-tracked Audity Z-plane resonance frozen into the ROM wave.
  Census over all 43 Aud families / 224 samples: evidence/research-results/
  xl1_aud_resonance_census.{py,txt}. Memory xl1-aud-waves-baked-zplane.
  Census verdicts: Aud Lead 2 = KEY-TRACKED (7.93 x f0, spread x1.01, Q 31, +36 dB); Aud
  Blend = FIXED formant at 6.40-6.42 kHz on every note C1 upward (Q 42-59, +20 dB): a static
  Z-plane resonance baked in; most other families "mixed" because the single-peak test latches
  on harmonics (needs a whole-envelope comparison across notes, not built yet).
- Tyson: "Wait where did you get them" / "They are samples": the Aud waves are Digital Sound
  Factory's Kontakt samples of the XL-1 playing (Downloads, not evidence, no ROM dump); the
  resonance is E-mu's (Audity at ROM-sampling time or the XL-1 preset filter at DSF's
  recording time; unknown which). "Frozen into the ROM" withdrawn. Copy any sample used into
  evidence/ before building on it.
- ROWS TABLE landed (Opus executor, uncommitted, staged deletions): native/app/rows_table.{hpp,cpp}
  (1112 lines) + number_box.{hpp,cpp} replace row_table.* and word_dial.* (deleted). Six rows:
  TYPE combo (OFF/EQ/LOWPASS/HIGHPASS/POLE/NOTCH; row 6 OFF/LOWPASS) writing both corners of
  the pair, LO note/ring/height and HI note/ring/height as NumberBox cells (drag, wheel, keys,
  double-click to type), unlock "..." per row for OFFSET/CUT/POLE/HARM, ROOT cell above. LO/HI
  = the Morph pair picked by the pad's Q half; corner picker buttons and the splitter gone;
  plot owns the height (645 px of 1000); bottom bay 300 px (pad 170 + meter 68 + seeds row).
  Plot handles on the editing corner: drag = note/height, wheel = ring, double-click births an
  EQ row; the FROM AUDIO reference stays. Ring rule rsq = 0x76 + (mag*0x7c>>8) followed until
  touched. Tests 36/36 (31 baseline), every threshold kept; four assertions restated for the
  new surface (TYPE stays enabled as the on/off control; row 6 HEIGHT is the CEIL cell; chrome
  geometry; ceiling text unchanged). MEASURED: the Designer's rad rule is a constant-Q family,
  Q 2.05 / 2.14 / 2.20 / 2.58 at mag 0x20 / 0x60 / 0xA0 / 0xE0 (max/min 1.26): the "Q that
  turns up with frequency" is constant Q about 2 in Hz terms, the GAIN/Q wheel sharpens from
  there. App exe 04:02. Awaiting Tyson's verdict on the real thing.
- XL-1 pool census (evidence/research-results/xl1_pool_resonance_census.{py,txt}, whole-envelope
  fixed-vs-tracked alignment over 97 multisampled families): 45 tracked / 4 fixed / 48 flat.
  Caveat: a periodic wave's own harmonics align in ratio space, so "tracked at 1.99x/2.03x" is
  the second harmonic, not a filter; non-integer ratios are the filter tells: Aud Lead 2 7.91x
  Q 12 +32 dB, Aud Bell 4 4.05x, Sync 2 3.81x, Ring Mod 2 22.7x; fixed: Aud Blend 6300 Hz Q 14.
- Tyson: "In downloads i have IR packs too" (Eminence Karnivore, JST Anvil, shift-line bass:
  guitar cabinets; DSF Kontakt reverb IRs) and "Trench-authoring has more recipes":
  C:/Users/hooki/trench-authoring/recipes has 112 WAVs (measured_objects/ir_library: violin,
  ukulele, upright piano, cymbals, kalimba, steel pan bodies; openair rooms), holy_sources and a
  sources/ PDF shelf. The 16 body IRs COPIED to evidence/measured-bodies/ir_library with
  PROVENANCE.md (licensed per LICENSE.md). LPC 30 on them: upright piano 88-99 Hz body
  +25..36 dB; violin body 873/2784/4092/5152/6635/8357 Hz (Q 1-8, broad); glockenspiel 2612 Hz
  Q 19 +26; kalimba 579 Hz Q 5 +14.
- Tyson: "What if the presets are literally keyframes . Show me the plots you made. Magnitude" /
  "You think they did that? interpolated across the grid and snapped keyframes?" Answer: yes for
  the cube by construction (Massie 1999: a frame per corner, bilinear interpolation across the
  user-parameter axes; P2K table: one 8-bit word per parameter per corner = keyframes snapped to
  the lattice, interpolated in word space; Designer LO/HI = two keyframes on one axis). The
  multisamples were not interpolated: each note was played through the filter and recorded.
  Plot evidence/research-results/xl1_keyframes_magnitude.{py,png} (sent to phone): every note
  of nine families as an LPC-30 magnitude envelope. Reads: Aud Lead 2 one peak marching an
  octave per note (+29..37 dB) = one key-tracked resonance; Aud Bell 4 two tracked peaks per
  note; Sync 2 a single gentler tracked peak; Ring Mod 2 tracked sidebands; Aud Blend the same
  6.3 kHz peak on all 11 notes = fixed formant; Aud Synth 14 / Vapor Vox / Aud Sync 1 high notes
  show LPC latching on harmonics (combs), not filters. In TRENCH a tracked family = one frame +
  KEY tracking; a fixed family = one static frame.
- Tyson: "Dude that is the emu method". RULING (memory emu-method-keyframes): a cube is
  keyframes at the corners, interpolated in word space; authoring = a sound per state read by
  FROM AUDIO (or a library frame) per corner, SHARPEN for the Q pair, the morph is the
  interpolation. The app has every piece today: pad corner -> FROM AUDIO -> next corner ->
  FROM AUDIO -> SHARPEN -> EXPORT.
- Tyson: "Look at them. Are you using 6 peq?" / "THAT is the emu method". The keyframe plot
  used the full 15-pair model; redrawn as six PEQ per note (five EQ bells + row-6 LP, the
  FROM AUDIO compile) in evidence/research-results/xl1_keyframes_6peq.{py,png} (sent). Honest
  reads: the bodies hold every strong peak; the +24 dB seed clamp caps Aud Lead 2's +30..37 dB
  peaks (raise kGain clamp to ~40 dB in body_from_audio when next touched); "highest
  resonance = the LP row" draws Aud Blend's 6.3 kHz formant as a resonant low-pass corner
  (E-mu's own section-6 posture, acceptable); combs on high notes vanish once only six are kept.
- Tyson: "Whats that nyquist zero doing": the dive above 10 kHz in the six-PEQ plot was my
  artefact: the ceiling zero parked at 20 kHz on files sampled at 32-37 kHz folds to 10-12 kHz.
  Fixed (zero at the file's top edge); the app's 44.1 k datum never had the problem.
- Tyson: "Maybe low shelf is better? Stage 6". Drawn: evidence/research-results/
  xl1_keyframes_5peq_shelf.{py,png} (sent): five EQ bells + stage 6 as a two-pole real-axis
  shelf (r_p 0.985, r_z from the tilt) carrying the note's measured tilt (60-150 Hz vs 5-9 kHz).
  E-mu precedent: Morpheus piano spare stage = 17 kHz r 0.41 tilt; P2K section 6 = ceiling
  zero. Candidate app change: RowType kShelf in applyTypeRows (real pole pair + real zero pair
  via setRealRootAt on both lanes, height from the tilt); FROM AUDIO uses it for bodies, the
  ceiling stays for vowels / hand voicing. Not built yet.
- Tyson: "That didt work. Use the best and neutral method maybe 6 peq?" RULING: FROM AUDIO
  compiles six EQ bells on a flat floor, no low-pass, no shelf; row 6 may be any type (LOWPASS
  stays its default when switched on). Opus executor dispatched: seedFromAudio all-kEq, row 6
  loses its ceiling-only special case in rows_table, tests adapted (slot_six case renamed
  slot_six_defaults_to_the_ceiling_and_can_be_an_eq, new row_six_eq_from_audio_lands_six_bells).
  Plots: xl1_keyframes_6peq.png redrawn as six bells; Tyson "Plot only m0 ... One curve" ->
  evidence/research-results/xl1_frames_m0_6peq.{py,png}: one note per family (nearest G3) as
  one six-PEQ curve over its spectrum (sent).
- Tyson: "Ring mod and synth are good. The last 3 are weird" -> cause: LPC "resonances" wider
  than their own frequency (Q < 1) compiled as EQ bells act as tilts and drag the floor (Aud
  Blend dive, Vapor Vox sag). RULE: only true bells (bw <= hz, Q >= 1) may become rows;
  applied to the plot scripts (xl1_frames_m0_6peq, xl1_keyframes_6peq); to apply in core
  body_from_audio resonances() after the executor lands. Tyson: "Whats the gap": the gap
  between the six-PEQ frame and the spectrum is the SOURCE tilt (saw/sync roll-off), not the
  filter: E-mu's split, ROM wave carries the tilt, Z-plane carries the bells; never paint the
  source into stage 6. Second gap: the +24 dB seed clamp vs Aud Lead 2's +37 dB -> raise the
  clamp to 40 dB in core when touched.
- Tyson: "Give it mych more radius as default" -> seed rule: pole bw = min(measured, hz/10)
  (Q >= 10), gain cap 40 dB. "The last 3 tho" -> rule: a bell needs >= 6 dB height on a
  sampled sound (3 dB on a measured transfer function) or the row stays OFF; Sync 2 / Aud
  Sync 1 are nearly empty frames (sync waves carry no filter at that note). Both applied in
  the plot scripts; to go into seedFromAudio after the running executor lands. Tyson posted
  two earlier evidence plots ("What are these" / "Low ahelf?"): the poles-only anatomy of Ear
  Bender and Lucifer's Q, and the Morpheus cube skeleton library (shared pole postures);
  answer: the dive is the missing zeros (a bare pole pair is a resonant low-pass), not a shelf.
- Tyson: "Do dvdt vowels": recipes/vocal/dvtd = Dresden Vocal Tract Dataset (Birkholz et al.,
  3D-printed tracts, measured transfer functions as freq/magnitude/phase text, 2 subjects x
  22 sounds). evidence/research-results/dvtd_vowel_frames.{py,png,txt}: LPC-30 from |H|^2
  (autocorrelation by inverse FFT of the power spectrum), six-bell frames per vowel (sent).
  tense-a s1: 681/244 (+11), 3274/200 (+8); s2 920/322, 3358/327, 4197/93 (+14.5); tense-i
  s1 2965/174 (+16.5); tense-o s1 364/151 (+15.6); tense-u s1 246/148 (+16.9). Note: /a/'s F2
  near 1.1-1.3 kHz is weak in the printed-tract measurement, so no bell lands there.
- Tyson on the DVTD six-PEQ plot: "Wrong. Do pole only". Vowels are all-pole (the tract is an
  all-pole filter): evidence/research-results/dvtd_vowel_poles.{py,png,txt} = twelfth-order
  LPC from |H|^2, no zeros. LESSON: order 12 at 44.1 k spreads six poles over 22 kHz and
  loses F1/F2 (tense-a: no pole below 2.9 kHz); at 11,025 Hz (E-mu's README: 8-11 kHz for
  speech) the six poles land one per formant: tense-a s1 519/1123/2327/3299 Hz, tense-e s1
  253/1791/2357/2988, tense-i s1 188/1807/2643/3108, tense-o s2 416/907, tense-u s2 336/877
  (bw 20-90 Hz: printed tracts ring sharper than tissue). RULE for FROM AUDIO: a "speech"
  mode = order 12 at a 10-11 kHz model rate, poles only (POLE rows), for vowels; the six-PEQ
  neutral mode stays for instruments and synth samples. Plot sent.
- Tyson: "Perfect" on the poles-only vowels. Rulings consolidated in memory from-audio-modes
  (neutral six bells vs speech poles-only at 10-11 kHz). Next build after the running
  executor: seedFromAudio gains the rules (true bell, >= 6 dB, seed Q >= 10, gain <= 40) and a
  SPEECH mode (order 12 at 11,025 Hz model rate, POLE rows).
- Tyson: "Do the same with vowels" -> evidence/research-results/emu_vowel_poles.{py,png,txt}
  (sent): the six VOW bodies of the bank, corners M0/M1, the hand-voiced body (poles+zeros,
  grey) against its six poles alone (blue). Poles: ooh_to_eee M0 611/958/2432/3207/4089/4936
  (bw 280/83/237/159/86/180); the "ee" frame 254/2018/3272/5479/9261/9986; talking_hedz M0
  225/1006/1772/2651/5201/10523; deep_bouche M0 257/1763/2164/2871/3339/3933. Same pole
  count and register as the DVTD tract poles (F1 190-750, F2-F4 1-4 kHz); E-mu's F1 is
  wider (bw 90-410 vs the printed tracts' 20-60).
- Tyson: "Shift their freq an oct" (E-mu vowel poles vs DVTD tract poles). Checked: matches
  within 0.2 octave are best at NO shift (aah vs /a/ 4/6, oo vs /o/ 4/6, Deep Bouche M0 vs
  /e/ 6/6: 257/1763/2164/2871/3339/3933 against 253/1791/2357/2988/3585/5358); one octave up
  or down makes it worse. E-mu voiced its vowels in the human tract's own register.
  Plot evidence/research-results/emu_vowels_vs_dvtd_octave.{py,png}. A first crude metric
  (pruning poles above 5 kHz after the shift) claimed +1.5 octaves; discarded as an artefact.
- Six-PEQ / row-6 executor landed: 37/37 native; rows_table.cpp row 6 lists all types (LOWPASS
  default when switched on), editor_state.cpp lost its section-6 cage (lockedZero,
  removeZeroAt/setRealRootAt refusals) so a row-6 EQ exists; seedFromAudio all-kEq. Modes
  executor (spec from_audio_modes_spec.md) dispatched.
- FROM AUDIO modes landed (Opus executor, uncommitted): 40/40 (trench_core, trench_core_from_audio,
  38 native). Core: SeedRules {6 dB, Q >= 10, 40 dB}, neutral_rows (true bells only), resample
  (127-tap windowed sinc + linear), speech_poles (11,025 Hz model rate, order 12, poles only);
  app: "fromAudioMode" combo SIX BELLS / SPEECH beside FROM AUDIO, setFromAudioMode(int) for
  tests; pole_templates RowType::kPole (bare pole, zero removed). Measured: synthetic vowel
  500/1500/2500 -> 495/1506/2520 Hz (bw 44/78/122); 1 kHz sine resampled reads 1000.013 Hz;
  neutral clip 250/1200/2800 -> 249.8/1201.2/2805.8 with a Q-0.5 strike at 600 Hz yielding
  no row; min-Q clamp 250 Hz -> bw 25.0. Deviations: a serial resonator cascade buries its
  top bell 22 dB under the envelope median, so the neutral test uses the struck clip; the
  from_audio window test now expects exactly 3 rows (no row without a true bell). App exe
  rebuilt. Everything from today is in the app: rows table, plot handles, FROM AUDIO with
  two modes, six-PEQ compile, row 6 free. Awaiting Tyson's verdict on the real thing.
- Tyson: "Arent those xl samples raw": yes, the DSF "Xtreme Lead Samples" folder is the raw ROM
  multisample pool (no preset filter). The Aud Lead 2 resonance is inside the wave: E-mu
  sampled the Audity 2000 through its Z-plane filter, note by note with key tracking, into the
  XL-1 ROM. "Frozen into the ROM wave" reinstated; the earlier retraction withdrawn.
- Tyson: "Really? Are you sure its just not audity raw samples": not sure. Final wording, in
  memory and SOURCE.md: MEASURED = a key-tracked resonant filter (7.9 x f0, Q 23-63) frozen into
  the raw ROM wave; NOT IN EVIDENCE = whose filter (E-mu's Z-plane at ROM-making time, or a
  sampled analog synth's resonant filter tracking the keyboard; the Audity 2000 ROM was
  marketed as sampled analog synths). Never claim the Z-plane for these waves.
- Tyson: "What source from recipes woll create the coolest filter". Tested the 303 sweeps as
  keyframes (evidence/research-results/tb303_sweep_keyframes.{py,png,txt}): the tracked bell
  rises 200 Hz -> 1.6 kHz over sweep A but the six-bell read finds nothing at the open end:
  a 303 is a low-pass knee with mild resonance, not bells. A LOWPASS read mode (E-mu Type 2:
  knee + resonance) would be a third FROM AUDIO mode; not built. Metamaterial IRs
  (holy_sources/phononic) are haptic-band actuator data, not bodies. SONICOM HRTF
  (holy_sources/hrtf, .sofa, h5py reads it) is the candidate: a head as keyframes, MORPH =
  direction.
- HRTF as keyframes: evidence/research-results/hrtf_head_keyframes.{py,png,txt} (sent). SONICOM
  P0001 left ear, six directions, LPC-30 on the 256-sample HRIR, six-bell rules. Front:
  3230/4378 Hz (ear canal, +9/+10) and a pinna cluster 10.4-12 kHz (+9..+13), 14.5 kHz; left
  side: 4607/5627/6840, 11.4 k; behind: 4013/5114 only; right side (shadowed ear): no bell
  >= 6 dB; front 45 up: 3077/4190/5467, 10.8/11.7 k; front 30 down: 2788/4192, 10.7/11.5 k.
  The bells move with direction by an octave in the pinna band: two directions = a morph
  pair, MORPH = turning the head. Needs a copy into evidence/ before use (CLAUDE.md).
- Tyson: "So they would have had to work from a mush higher dimension than cubes" (yes: the
  sound's own control space; the cube picks two or three axes and keyframes their extremes),
  then "Its all about anchoring isnt it. Logarithmic lerp. So you could swap in a certain frame
  just to get that strong frequency anchor", then "You need to make the workstation match this
  exactly". MEASURED on the head: averaging front and side responses crossfades (both sets of
  peaks, 4.6 dB rms vs the true 45-degree frame); log midpoints of slot-paired poles 3857/4963
  land within 2% of the measured 3951/5052. RULING in memory anchoring-log-lerp. Opus executor
  dispatched (scratchpad anchors_spec.md): EditorState planAnchors/anchorCornerToPartner
  (nearest log-frequency pairing within an octave, greedy, deterministic), ANCHOR toggle +
  ANCHOR NOW in the toolbar, applyFrame/applyTypeRows anchor on landing, plot glide lanes per
  paired slot with the wheel's current pole, ROW FROM FRAME context menu (same slot / nearest
  slot), tests incl. word_lerp_is_a_log_lerp (the core lerp must land within 3% of the
  geometric mean or it is reported as a finding).
- Tyson: "And have a strong list of 'keyframes'?" Built native/app/templates/keyframes_from_audio.json
  (evidence/research-results/keyframe_library_build.py): 110 keyframes in the grammar
  (TYPE/hz/bw/gain rows): HEAD 35 (SONICOM P0001 left ear, az every 30 deg x el -30/0/30/60;
  SOFA copied to evidence/measured-bodies/hrtf), XL-1 AUD 31 (families with bells, note nearest
  G3), VOWEL DVTD 30 (poles), BODY 8, INSTRUMENT 5, VOWEL 1 (sung ah, speech). With the 132 bank
  corners: 242 keyframes. App loading spec ready (scratchpad keyframe_library_spec.md: FRAME
  combo groups, ROW FROM FRAME entries, applyKeyframe with ANCHOR); dispatch after the anchors
  executor lands.
- CORRECTION 10:25: the anchors executor was never dispatched at ~05:30 (spec written, ruling
  filed, dispatch omitted) although the brief and two replies said it was running; nothing
  touched the app after 05:00. Dispatched 10:25. The keyframe-library build waits on it.
- ANCHORS landed (Opus executor, uncommitted): 42/42 native + 2 core. EditorState planAnchors /
  anchorCornerToPartner (each partner slot in ascending order claims its nearest free incoming
  row within an octave; the spec's cost-ascending greedy tied numerically and paired 4607 to
  slot 1, so slot-order claiming was chosen), ANCHOR toggle + ANCHOR NOW, applyFrame /
  applyTypeRows anchor on landing, plot glide lanes with the wheel's pole, ROW FROM FRAME
  (same slot / nearest slot). MEASURED: the core's packed-word lerp at morph 0.5 between poles
  3230 and 4607 Hz lands at 3781.7 Hz vs the log midpoint 3857.5 (-1.96%): the hardware lerp
  is a log lerp. Test 4 checks the four root words (the gain word is recomputed by the
  corner-wide DC rule + cut). Keyframe-library executor dispatched (keyframe_library_spec.md).
- KEYFRAME LIBRARY landed (Opus executor, uncommitted): 45/45 native + core. pole_templates
  Keyframe / loadKeyframes / applyKeyframe / keyframeSlotWords (slot words by compiling the
  keyframe into a scratch EditorState); TRENCH_KEYFRAMES_FROM_AUDIO define; FRAME combo = 250
  items (132 bank frames, separator, six group headers, 110 keyframes); ROW FROM FRAME gains
  per-group keyframe submenus and KEYFRAME NEAREST SLOT. Tests keyframe_pick_lands_rows
  (3230.1/4377.8/10403.6 Hz), keyframe_pick_speech_lands_poles, row_from_keyframe_writes_that_slot.

## Session close 2026-09-04 ~10:50 (next chat starts here)
State: the workstation is the E-mu method end to end, UNCOMMITTED in the native tree (about
25 files incl. new number_box.*, rows_table.*, body_from_audio.*, templates/keyframes_from_audio.json;
row_table.* and word_dial.* deleted, staged). Ship plugin untouched today. App exe rebuilt.
What the app does now: rows table (TYPE/NOTE/RING/HEIGHT for LO and HI, NumberBox cells, no
faders, row 6 free, RING follows the note by the Designer's constant-Q rule until touched),
plot handles (drag note/height, wheel ring, double-click births a row), FROM AUDIO with SIX
BELLS (true bells, >= 6 dB, seed Q >= 10, gain <= 40) and SPEECH (order 12 at 11,025 Hz,
poles only), ANCHOR on landing (slot-order nearest-log-frequency pairing) + ANCHOR NOW, glide
lanes on the plot, ROW FROM FRAME (bank frames and keyframes, same / nearest slot), FRAME
picker with 242 keyframes by group, SLOT -> TRENCH DEV hot reload. Tests: 45 native + 2 core.
Rulings today (memory): morph-designer-grammar, cube-corners-ruling, no-faders-drag-the-peak,
emu-method-keyframes, from-audio-modes, anchoring-log-lerp, xl1-aud-waves-baked-zplane.
Evidence filed: Designer grammar + panel, Spectrogram Env (E-mu-owned 12th-order LPC), LPC
comparison plots, XL-1 censuses and keyframe plots, DVTD vowel poles, E-mu vowel poles,
octave check, 303 keyframes, head keyframes, keyframe library builder; measured-bodies
(ir_library, hrtf) and factory-data/xl1-dsf-aud copied into evidence with sources.
Open: Tyson's verdict on the real app; commit (explicit pathspecs, native only); LOWPASS read
mode for knees (303) if wanted; rounded vowels need order 14 or 8 kHz in SPEECH; Morpheus
import (two squares, 7th section policy); Morpheus axis bit-map conflict; taper sessions;
bake loops; the SGI Indy launcher works (Launch_SGI_Spectrogram.bat, -mouse).
- Tyson on the first real capture: "Too many lines" / "It should be the full grid". Cut inline
  in cascade_plot.cpp: partner curve and dashed wheel curve removed; the six glide lanes
  removed from inside the grid (the frame is the full +-30 dB again); one glide for the
  selected row only, drawn in the axis band under the grid (LO tick, HI tick, dot at the
  wheel's pole). Plot now: black editing corner, rust selected bell, handles, FROM AUDIO
  ghost when present. 45/45 native; app relinked 11:00 (the running app held the exe: LNK1104
  until closed).
- Tyson: "There only needs to be one line". The selected row's rust curve removed too; the
  plot is the black cascade of the editing corner, the handles (selected one rust), the one
  glide under the axis, and the FROM AUDIO ghost only right after a read. 45/45; app relinked
  and relaunched.
- Tyson: "I think theres a real connection with the literature of martens and such and the emu
  method". Documented: the Morpheus manual credits Dr. William Martens with cube F043
  VowelSpace (Morph = F1 150-850 Hz, Freq Tracking = F2 500-2500 Hz, Transform = vowel stress,
  schwa to full excursion); Martens is co-inventor with Massie, Sun, Friedman and Rossum on US
  5,943,427 (3D audio spatialization, filed 1995, Creative/E-mu); his 1987 paper is the PCA and
  resynthesis of spectral cues to perceived direction. The cube as a reduced perceptual space
  with basis frames at the corners: Massie's "8 basis filters" in the same words. Papers copied
  to evidence/papers/martens with SOURCE.md; memory martens-vowelspace-basis-frames.
- Tyson: "What needs to be changed?" (after Martens). Answer: nothing in the plugin; in the app
  (1) anchor across BOTH axes so slot identity holds over the whole square (bilinear glides need
  it), (2) name the two axes per body (.trenchbody keys morph_axis / q_axis, painted on the pad,
  AXES dialog), (3) Q may carry a second perceptual axis by dropping a different keyframe on the
  Q corners (no law change). Opus executor dispatched with scratchpad square_anchors_spec.md
  (anchorCornerToSquare, ANCHOR SQUARE button, axis names round trip; tests listed).
- Tyson: "It should maybe be more reactive and dynamic". RULING: the one line is the sound at
  the wheel (the interpolated cascade at the pad), handles ride the interpolated poles, a handle
  drag moves the editing corner's slot by the drag ratio. Spec scratchpad/live_plot_spec.md with
  tests plot_follows_the_wheel and plot_handle_rides_the_interpolated_pole; dispatches after
  the square-anchors executor lands (same window file).
- SITE (Tyson: "Hows the website?" / "Its stale. The animatiobs should read as 2008 era styling.
  Crude animations" / "Rewrite the description too" / "Might be having secong thoughts on the
  namin"). Source: the Claude Design project "Direct Form II Musical Filter" (claude.ai/design,
  not readable from here); its export "Direct Form II Musical Filter (1).zip" (Downloads,
  2026-08-25) holds "Direct Form II - Product Page.dc.html" + assets. Page copy says TRENCH,
  "Programmable Filter Processing System", Program Systems, Melbourne, A$199, support@
  programsystems.com.au; memory signal-methods-brand says the brand is Signal Methods: NAMING
  CONFLICT left untouched, Tyson's call. Edits (scratchpad df2site/work/Main.dc.html, patch
  script df2site/patch_site.py): description rewritten ("TRENCH is a filter you play. Six
  resonances make a body, two bodies make a morph, and one wheel walks between them ..."),
  2008 motion: glossy bevelled buttons with jump hovers and a 1 px press, blinking NEW tag,
  stepped loading dots, tour redrawn at 8 fps with a venetian-blind cut and captions typed with
  a blinking cursor, a chunked progress bar, LED blink while playing, row hover. Reseeded as a
  new canvas (this preview cannot update the claude.ai/design project); images downsampled.
- Tyson: "too much technobabble. use emu manual speak terminology" (memory emu-manual-voice).
  Site description rewritten in manual voice ("TRENCH is a morphing filter. Each filter holds
  two frames, and the Morph wheel sweeps smoothly between them ... Q raises the resonance of
  the whole filter at once. Movement sweeps the Morph wheel for you ... The display shows the
  filter response as you play."), canvas republished. Next app build after live plot: fold
  ANCHOR NOW + ANCHOR SQUARE into one, AXES onto the pad; one executor at a time from here.
- Tyson: "redo the sound captions in that voice too ... slightly uncanny ... slightly on the nose
  ... 'we have nothing to do with any australian native bird companies TRENCH is a musical
  filter'". Captions now: drums "Morph: sweeps the drums from lowpass to highpass over two bars.
  The kick leaves first. Nothing else was touched."; chord "Lowpass with the Q raised until it
  sings. The note on top is not in the chord. It is the filter."; speaking "Bandpass, twice
  over. The first sets the mouth, the second the tongue. The sawtooth does not know it is
  saying anything."; section note "Dry is the sound before the filter. Processed is after.
  Nothing else was added."; Technical gains "TRENCH is a musical filter. We have nothing to do
  with any Australian native bird, or any company named after one." Canvas republished.
- Tyson: "is that a good idea or is it asking for trouble" (the bird disclaimer) -> advised cut;
  then "We are vague on purpose. And almost downplaying of the product . Its a musical filter."
  RULING (memory signal-methods-brand): understated, deliberately vague public voice. Technical
  line is now "TRENCH is a musical filter."; description cut to "TRENCH is a musical filter.
  Each filter has two frames and a Morph wheel that sweeps between them. Q raises the
  resonance. Movement sweeps the wheel for you. The display shows what the filter is doing."
  Canvas republished. Tyson: "What about the name trench" (naming open).
- LIVE PLOT landed (Opus executor, uncommitted): 50 native + 2 core = 52/52. The black curve is
  the cascade at the wheel; handles ride the interpolated poles and sit on that curve; a
  horizontal drag applies its ratio to the editing corner's pole. Measured: single poles 1000 Hz
  (LO) / 2000 Hz (HI): pad 0 -> peak 1007 Hz, pad 0.5 -> 1410 Hz (log midpoint 1414), pad 1 ->
  1977 Hz; a 60 px drag at pad 0.5 took the LO pole 1000 -> 1322 Hz, HI untouched. refresh()
  builds one cascade instead of three. Next single build: fold ANCHOR NOW + ANCHOR SQUARE into
  one button, AXES onto the pad.
- Tyson: "Update the image with current face. Can you record some better demos? Use the d rich
  pattern in my downloads and then do a brass and pad. From the x lead samples" / "Can you
  record a gif? With crude transition animation". Done and republished: face from the
  plugin's own TRENCH_FaceShot (default and MORPH 0.68 "active" renders); tour GIF from those
  renders (Pillow: 400x200, 6 fps, 75 frames, 64 colours, venetian-blind cuts, stepped zooms,
  typed captions; 819 KB); demos rendered offline with evidence/research-results/
  render_demo.py (the body's packed words, bilinear word lerp per 32-sample tick, six biquads
  in series, peak-normalised; filter only, no desk / BITE / AGC): "Two bars of drums" = d rich
  loop 140.wav (Downloads) first two bars through the Designer template "Low to High Pass"
  morph 0->1; "Brass" = P5 Brass C1 (XL-1) resampled to a C/G/C chord, 4 s, through
  talking_hedz morph 0-1-0-1-0 at Q 0.35; "Pad" = Jup 6 BuzzPad G3 (XL-1) stacked, 5 s,
  through lucifer_s_q morph 0->1 at Q 0.7. Clips embedded as base64 22.05 kHz WAV decoded by
  WebAudio (1.4 MB); captions in manual voice. Sources in scratchpad/demos (copy to evidence
  if kept). Tyson: "I really think this plugin will not totally flop".

## Close addendum 2026-09-04 ~13:40 (next chat starts here)
App (uncommitted, 52/52): rows table, live plot at the wheel, handles, one glide, FROM AUDIO
SIX BELLS / SPEECH, ANCHOR + ANCHOR NOW + ANCHOR SQUARE, AXES, keyframe library (242).
Site: canvas https://claude.ai/code/artifact/d91a6726-d1cc-4f96-b2b2-a1a693b4c692 (current
face, GIF tour, three offline-rendered demo pairs, understated manual-voice copy; names as on
the Aug export). Domains checked: signalmethods.com and programsystems.com taken; the .com.au
of both, trench.audio and trenchfilter.com unanswered (likely free).
Open, in order: (1) commit the native work with explicit pathspecs when Tyson says; (2) tidy
the toolbar (one anchor button, AXES onto the pad); (3) Tyson's verdict on the real window;
(4) Tyson's own bodies for the ship roster (he says covered); (5) real recordings of the three
demos through TRENCH in FL, then drop into the page; (6) tagline + company name, buy
trench.audio + the company .com.au, pick a store (Gumroad / Lemon Squeezy / Paddle), wire Buy
and Demo, host the static page; (7) leveller table -> a knee formula on his two numbers, by
ear in the dev build; (8) Morpheus import, taper sessions, bake loops, Theo Lovejoy.
- Tyson 13:45: "Finish the site. The app work can be paused and written down. Do the renders".
  APP PAUSED at 52/52 uncommitted. Written down: next app build = toolbar tidy (one anchor
  button doing the square, AXES onto the pad by clicking the axis label), then Tyson's verdict
  on the real window. Site: demos to be re-rendered through the REAL plugin (a console render
  tool beside FaceShot) with ship-roster bodies, then a deployable static folder.
- Site, deployable: scratchpad/df2site/make_static_site.py turns the canvas artboard into a plain
  static folder df2site/site/ (index.html + assets/: banner, current face, tour GIF, clips/*.wav
  fetched by WebAudio). Render tool executor dispatched (plugin/tools/Render.cpp + TRENCH_Render
  target; spec scratchpad/render_tool_spec.md): drums through Cross Band (LP -> HP), brass
  through High Rise (mouth), pad through Peak Rise at Q 0.6, all ship-roster bodies, through the
  real processor. On landing: peak-normalise, downsample to 22.05 k, rebuild the static folder
  and the canvas, republish.
- RENDER TOOL landed (Opus executor): plugin/tools/Render.cpp + TRENCH_Render (ship roster) in
  plugin/CMakeLists.txt; params mapped INPUT=preamp, OUTPUT=slamDrive (unity 0.5), BITE=chew
  (0.18), MOVEMENT=movePreset (0 = OFF), FOLLOW=envAmount (0), KEY=keySnap (0 = OFF); morph
  curve per block. Added TRENCH_RenderDev (TRENCHPluginCommonDev, the 33-body roster) inline.
  Tyson: "Do one with talking hedz, one with ear bender, and one with bassbox 303": rendered
  through the real plugin: brass -> Talking Hedz (morph 0-1-0-1-0, Q 0.35, rms -7.9), pad ->
  Ear Bender (morph 0->1, Q 0.6, rms -10.7), drums -> BassBox 303 (morph 0->1, Q 0.5, rms
  -11.6); all peak at the plugin's ceiling (-0.1 dBFS). Tyson: "Play them here": six 16-bit
  clips sent (scratchpad/demos/play). Ship-roster renders (Cross Band / High Rise / Peak
  Rise) kept as *_plug.wav.
- Tyson: "These are wrong. Make them 4-8 bars and the pad doesnt sound good". Rebuilt at 8 bars
  (13.71 s at 140): drums = the whole loop twice, BassBox 303 morph 0->1 over four bars and back,
  Q 0.5; brass = P5 Brass C1 chord, Talking Hedz mouth 0-1-0 every two bars, Q 0.35; pad = Pad
  Life Dm9 (XL-1) crossfade-tiled with a sub octave copy, 1.2 s attack, Ear Bender morph 0->1
  over eight bars at Q 0.3 (rms -18 dB before normalising). Six clips sent again. The earlier
  pad was Jup 6 BuzzPad hard-tiled (seams) at Q 0.6.
- Tyson: "Whys it distorted" (the eight-bar demos). MEASURED with the render tool, 220 Hz sine
  through BassBox 303 (morph 0.5, Q 0.3): BITE at its 18 % default gives THD 22.8 % at -1 dBFS
  in, 13.3 % at -12 dBFS, 4.3 % at -21 dBFS; BITE 0: 5.1 % at -1 dBFS (the safety ceiling
  clipping the resonant boost above the leveller's +6 dBFS threshold), 0.00 % at -12 dBFS. So
  the default BITE saturates hard on resonant bodies at normal levels (the earlier bite_probe
  ran a bare sine at 0.3 with no body boost). Demos re-rendered with --bite 0 and sources at
  -12 dBFS: clean, sent. OPEN for Tyson: the BITE default / law (ceiling 4.0 -> 0.35 vs states
  that reach 10-30x on a resonant body) and the leveller threshold (+6 dBFS) sitting above the
  ceiling (0 dBFS): a 6 dB clipping zone. Render tool gaining --move and --bpm (a playhead) for
  the supersaw-with-MOVEMENT demo.
- Tyson: "What about a harmonic supersaw with the agc and modulaton". Render tool gained --move
  <preset index> and --bpm (a RenderPlayHead feeding bpm/ppq per block; MOVEMENT presets: 1 Rail
  Switch, 2 Backbeat Bloom, 3 Triplet Relay, 4 Eighth Sway, 5 Quarter Arc, 6 Broken Ladder,
  7 Pendulum Teeth, 8 Long Return). Supersaw source synthesised (C2 G2 C3, seven saws each,
  +-18 cents, 8 bars at 140), -12 dBFS, through Lucifer's Q (morph rest 0.3, Q 0.4) with Quarter
  Arc at 140: BITE off (rms -15.6) and BITE 0.18 (rms -16.1); the leveller rides the movement
  (50 ms envelope swings -9..-23 dB). Three clips sent. TRENCH_RenderDev rebuilt.
- Tyson: "It clips in an unmusical way". Diagnosis: the leveller wakes at +6 dBFS (table index
  >= 2) while the safety guard hard-clips at -0.1 dBFS: a 6 dB zone chopped flat before the
  leveller acts (measured 5 % THD on a -1 dBFS sine with BITE off). Opus executor dispatched:
  leveller lookup at twice the level (wakes at 0 dBFS, same curve), guard becomes the Morpheus
  firmware's soft clip (linear to half scale, quadratic to full: y = C(1 - (1.5 - a)^2/2) for
  0.5 < a < 1.5), tests restated, a THD measurement test (< 1 %), suites headless, no install.
  BITE's own law (tanh on the state, ceiling 4 -> 0.35) is the next question for Tyson's ears.
- LEVEL CHAIN landed (Opus executor, uncommitted, plugin tree, NOT installed): TrenchDspBridge.h
  leveller lookup at twice the level (kLevellerScale 2.0: wakes at 0 dBFS); SlamStage.h
  trench::softGuard = the firmware curve (linear to half scale, quadratic to full) replacing a
  tanh knee from -0.5 dBFS that behaved as a hard clip, applied at PluginProcessor.cpp:435.
  Measured: 220 Hz sine at -1 dBFS, BITE 0, BassBox 303: THD 5.06 % -> 0.27 % (peak -0.10 ->
  -0.57 dBFS, rms -2.4 -> -8.0: the leveller now does the work); ship-roster sweep worst body
  5.06 % (Drift 2) -> 0.29 % (Low Shape). TRENCH_Tests 105/107 (the two failing before and
  after are face tests "BITE is not a third gain knob" / "absent from the face: Bite", not
  touched, pre-existing); ReviewTests 28/28. New tests: the ruled sine check plus a roster THD
  sweep under 1 %. Eight demo clips re-rendered through it (BITE 0 and 0.18) and sent.
  OPEN: install after Tyson's ear; BITE's own law next.
- Tyson: "Those peaks get really harsh". Read: the Q wheel at 0.3-0.5 sits most of the way to the
  Q100 corners (bw 12-14 Hz). Four clips re-rendered with Q 0.10-0.18, BITE 0, sent. Candidate
  law if still harsh: BITE as the peak tamer with its tanh knee scaled by the section's own
  resonance gain (state / (1 - r)), so saturation limits the resonance gently at the section's
  output level instead of slamming a fixed ceiling of 2.6 with states of 20 (Rossum's analog
  behaviour); not built.
- Tyson: "Can we handle it in a more musical way with agc or something". Answer: the leveller
  after the cascade cannot change the ring-to-body ratio; the musical way is a RING LEVELLER
  inside the serial cascade: per section, a smooth gain holding the section's output at most
  12 dB above its own input envelope (1 ms catch, 120 ms release, gain only, no harmonics),
  internal, always on, no knob. Opus executor dispatched (spec in the prompt): core runner,
  tests (held peak, transparency at low Q, THD < 0.5 % and >= 6 dB lower peak on BassBox 303 at
  Q 0.5), suites, four renders *_ring.wav at the original Q values.
- Tyson: "I dont like the renders ill make my own. Just get the page done". Page finished as a
  drop-in: repo folder site/ (index.html 11 KB, assets/ banner + current face + tour GIF,
  assets/clips/ empty with README naming the six files drums/brass/pad _dry/_proc .wav); the
  player fetches them and says "recording not here yet" until they exist; Buy Now / Download
  Demo still href="#" (store link and demo installer are Tyson's); names as on the Aug export.
  Canvas republished without embedded audio. My renders stay in the scratchpad only.
- Tyson: "Lets make it seem like its a slightly cheesy and quirky hardware demo. Like we are
  talking about it likes its an actual module". Copy rewritten (canvas + site/index.html):
  "Windows x64 · VST3 · one unit"; description "TRENCH is a musical filter, supplied as a module.
  On the front panel: a Morph wheel ... a Q wheel ... and a glass. The display shows what the
  filter is doing. Movement sweeps the wheel for you, in time, while you attend to other things.
  There is no rear panel."; "Ships as a file. Weighs nothing."; "The unit in operation";
  "Demonstration recordings"; Specifications: "Filter: twelfth order, six sections in series, two
  frames per program. Controls: two wheels, one glass. Rear panel: none. Power: none required.
  Format: VST3 ... The front panel is a fixed bitmap set ... TRENCH is a musical filter."
- Tyson: "Just say it morphs through complex filter spaces in real time with only one parameter
  the user has to control". Description now: "TRENCH is a musical filter, supplied as a module.
  It morphs through complex filter spaces in real time, with only one parameter the user has to
  control. There is no rear panel." Canvas republished; site/index.html rebuilt.
- RING LEVELLER landed (Opus executor): CascadeRunner::ring_level in native/core/src/audition.cpp
  (e_in/e_out followers, 1 ms / 120 ms, gain only), test-only set_ring_leveller. FINDING: at a
  12 dB ceiling formant bodies are gutted (Talking Hedz -27 dB, static boost capped with the
  ring); ceiling set to 24 dB inline (voiced peaks pass, Q-wheel screaming held); core test
  restated to 24; plugin ring test being restated to a Q where the ring exceeds 24 dB. Memory
  ring-leveller-law. Next: rebuild all, suites, install ship + dev builds, one "restart FL".
- Level chain complete and INSTALLED (ship TRENCH.vst3 and TRENCH Dev.vst3 into Common Files):
  leveller wakes at 0 dBFS; guard = firmware soft clip; ring leveller inside the cascade at 24 dB
  over each section's input (1 ms / 120 ms, gain only). Suites: core 27/27 + from_audio 51/51,
  native 50/50, ReviewTests 28/28, TRENCH_Tests all but the two pre-existing face failures
  ("BITE is not a third gain knob", "absent from the face: Bite"). The plugin ring test is
  conditional: a >= 6 dB drop when a ship body rings past 24 dB at 220 Hz, else transparency
  (<= 0.5 dB); on the ship roster no body rings past 24 dB at 220 Hz even at Q 0.9 (Speaker
  17.1 dB), so it measured transparent (0.02 dB). BITE default unchanged (18 %); its law open
  (bite_tamer_spec.md). Tyson to restart FL and judge.
- Tyson: "What other datasets could be great for a filter" -> ranked: measured analog filter
  grids (cutoff x resonance = a cube by construction), brass/woodwind bore impedance (harmonic
  ladders), more heads (CIPIC, Listen), singers (VocalSet), Hillenbrand tables (child-to-adult
  axis), horns/cans/car cabins, xeno-canto animals. "Search pls. And optimise the keyframe
  selector matrix": Sonnet web-research agent dispatched for real links/licences; Opus
  executor dispatched with scratchpad keyframe_grid_spec.md: FRAME + TEMPLATE combos replaced
  by one FRAMES button opening a popup matrix (group tabs, search, 132x64 tiles with a
  sparkline of each entry's response, click lands on the editing corner with ANCHOR,
  shift-click on the Morph partner, hover previews as the plot ghost).
- Dataset search filed: evidence/research-results/datasets_for_filter_keyframes.md. No downloadable
  analog-filter cutoff x resonance corpus exists (DGMD is a tool to record one from hardware);
  UNSW sax/clarinet impedance per fingering; heads CIPIC 45 / ARI 221 / SADIE II 20 / HUTUBS 96 /
  RIEC 105 all SOFA; VocalSet 20 singers x 17 techniques x 5 vowels CC BY; OpenAIR was down.
  Hillenbrand 1995 + Peterson-Barney 1952 raw tables copied to evidence/factory-data/hillenbrand-1995
  (h95.csv, pb52.csv, vowel_medians.csv = 48 vowel x speaker-type keyframes, three formants each).
- Tyson "Yes": Hillenbrand speaker type becomes a keyframe group. VOWEL H95 added to
  keyframe_library_build.py (48 entries, poles at median F1..F3, bw 50 + F/20). JSON regenerated
  after the FRAMES grid executor lands (its test counts 110 keyframes today; count becomes 158).
- FRAMES grid landed (Opus executor): native/app/keyframe_grid.{hpp,cpp}; FRAME + TEMPLATE combos gone,
  one FRAMES button opens the matrix (tabs BANK 132 / TYPES 14 / HEAD 35 / XL-1 AUD 31 / VOWEL DVTD 30 /
  VOWEL H95 48 / BODY 8 / INSTRUMENT 5 / VOWEL 1 / ALL 304; search; 132x64 tiles with the response
  sparkline, matched the cascade to 0.00 dB; click lands on the editing corner with ANCHOR, shift-click
  the Morph partner; hover ghosts the plot). Library regenerated with VOWEL H95: 158 keyframes. 53/53
  native cases pass, exe relinked. Uncommitted. Note from the build: with ANCHOR on, landing az 90 on
  corner 1 pairs 11417 into slot 2 against 10404 and pushes 6840 to slot 3; slot 0 = 4607 as planned.
- Capture switch: trench_native.exe --open <body> --pad m,q --frames "<tab>" --capture out.png grabs
  the FRAMES grid on that tab (main.cpp). Grid pictures in the session scratchpad frames_h95.png /
  frames_all.png. Tab order today: BANK, TYPES, VOWEL H95, HEAD, XL-1 AUD, VOWEL DVTD, BODY,
  INSTRUMENT, VOWEL, ALL 304.
- Tyson: "Whats the 132 bodies?" (33 bank bodies x 4 corners) / "What about a complex perceptual way
  to index?" (answered: Martens' way, PCA over the 304 response curves, every frame a dot on the first
  two axes = a MAP tab; recommended, awaiting his go) / "clicking on the frame you can hear what it
  sounds like": Opus executor dispatched with scratchpad frames_listen_spec.md (hover = the frame on
  all four corners through the AUDITION runner, grid opens the audition if closed, HEARING status line).
- Hearing in the grid landed (Opus executor): MainWindow::frameView(entry) = the frame on all four corners,
  hover hands it to the audition, HEARING status, the grid starts the audition when closed (not under the
  offscreen platform, so tests stay silent), leave restores heardView. az 0 el 0 through design_audition:
  10.51 / 9.70 / 18.75 dB at 3230 / 4378 / 10404 Hz, matching the tile. 56/56 native pass.
- Tyson's screenshot: mproctor.net/tools/vspace, the Clickable Vowel Space (F1 200..850 down, F2
  2600..500 across, click gives F1..F4). That is the "perceptual index" he means, not a PCA map.
  Opus executor dispatched with scratchpad vowel_space_spec.md: VOWEL SPACE tab in the grid, log
  trapezoid, 48 H95 marks (men labelled), mouse move hears the vowel, click lands four poles (F3 from the
  three nearest man marks, F4 = 1.4 F3, bw 50 + F/20). frames_perceptual_map.py + --frames-dump switch
  (main.cpp) kept as an evidence experiment, not run.
- Tyson: "Modulation or follow drives the 3rd axis". Answered: FOLLOW into BITE is the analog thing
  (harder hit, more saturation) and needs no new control (glass sets how much, FOLLOW whether it moves);
  MOVEMENT into BITE reads as a distortion trick, keep it on MORPH; a true third cube axis needs
  8-corner bodies and stays in the app. Proposed a dev-build render A/B (drums, bass: BITE static vs
  following) before any law change. Awaiting his call.
- Tyson: "have it clean and coherent. controls should line up with the workflow and topology.
  signal-so-far". Spec written (scratchpad clean_bar_spec.md), dispatch after the vowel space lands: bar
  in workflow order OPEN RESET | FRAMES FROM AUDIO(menu) | ANCHOR(menu: NOW, SQUARE) | AUDITION SOLO ...
  SLOT SAVE EXPORT; AXES button and dialog gone, axis names on the pad's edges, click to rename; rows
  gain a SO FAR column (cascade through rows 1..k at the pad, row 6 = the plot line, OFF rows pass
  through, hover ghosts it); path meter moves under the rows as the OUT line. Reading of
  "signal-so-far" = the per-row partial cascade; stated to Tyson.
- FL plugin list showed TRENCH_2..12 duplicates: 20 stale TRENCH.vst3.inuse-old-* / shipping-old-* files
  from August in C:\Program Files\Common Files\VST3 were being scanned. Moved (not deleted) to
  out/vst3-stale/. Left: TRENCH.vst3, TRENCH Dev.vst3 (15:30 builds), Trench Capture.vst3 and the
  older FIELD / engineField / Sound Module plugins. Install rule from now: rename-aside files go to
  out/vst3-stale, never stay in the VST3 folder. FL needs a plugin rescan to drop the numbered entries.
- VOWEL SPACE landed (Opus executor): native/app/vowel_space.{hpp,cpp}, a tab in the FRAMES grid; log
  trapezoid F1 200..850 / F2 2600..500 top, 1800..880 bottom; 48 H95 marks, men labelled; mouse move hears
  the vowel, click lands four poles. F3 rule changed by the executor and accepted: inverse-distance over
  all twelve man marks, not the nearest three, because "er heard" (F3 1701, rhotic) sits nearest (500,
  1500) and made F3 jump; now F3 2345 / F4 3283 there and continuous under the cursor. Grid grew to
  800x440 for the tab row. 60/60 native pass. Picture: scratchpad frames_vowel_space.png.
- Clean bar executor dispatched (clean_bar_spec.md) on the free tree.
- Tyson "why default at 50 output?": OUTPUT is a -12..+12 dB trim, unity in the middle, readout says 50.
  Offered: dB readout in the value boxes (recommended) or boost-only 0..+12. Awaiting his pick. Found:
  forceCleanAudioUiState sets slamDrive 0.0 = -12 dB in the clean-audio test build; fix to 0.5 next
  plugin touch.
- Tyson's own demo recordings arrived (FL masters 16:52 brass, 16:56 reese, plus dry sources in
  Downloads): 12.80 s each, 44.1k stereo float, aligned. Level-matched all four to -16 dBFS RMS
  (brass dry +12.2, brass proc +3.7, reese dry -1.4, reese proc -7.2 dB; no clipping) and wrote 16-bit
  WAVs to site/assets/clips/{brass,reese}_{dry,proc}.wav. Page rows now Brass and Reese bass (drums and
  pad rows removed), captions from a wet/dry spectral-motion read: brass = Morph climbs and returns
  twice with Q up; reese = four seconds low, then all the way up to air, back. README updated with the
  local-server note (file:// blocks fetch).
- Clean bar landed (Opus executor): bar = OPEN RESET | FRAMES [FROM AUDIO · mode menu] | [ANCHOR menu:
  NOW, SQUARE] | AUDITION SOLO ... SLOT SAVE EXPORT; AXES button + dialog gone, axis names on the pad's
  edges with inline rename (axisEntry); SO FAR column in the rows (soFar0..5, cascade through rows
  1..k at the pad; k=3 -10.40 dB, k=6 -7.88 dB at 958 Hz on Ooh To Eee pad 0.3/0.2, k=6 = the plot
  line); path meter = OUT line under the rows. 62/62 native pass. Picture: scratchpad
  window_clean_bar.png.
- Tyson "I dont like the demos" -> "The sounds themselves" (his brass/reese recordings). Rendered set2
  (scratchpad demos/set2): drums/brass/pad/saw x {a: low Q, slow Morph, BITE 0; b: MOVEMENT + BITE
  0.18}, 8 bars at 140 through TRENCH_RenderDev 15:28 (new level chain). Peaks -0.2..-16.8 dBFS; pad_b
  rms -37.6 (Long Return on Ear Bender sits quiet). Sent as 16-bit copies. Awaiting his pick.
- Tyson "do u think marimo is better": answered no for the instrument (drag, hover-to-hear, audition
  need the in-process Qt loop); marimo fits the research scripts in evidence/research-results.
- Vowel space readout fixed inline (vowel_space.cpp: readout room 52, cell stride 160, cell width 96 =
  NumberBox width, row dropped 22 px): labels and corner values no longer collide. 62/62.
- Tyson "wym readout? it should be unity. whats the signal chain in what ships". Traced from
  processBlock / TrenchDspBridge / DeskDrive: input meter (untouched buffer) -> MORPH per sample =
  wheel + MOVEMENT + FOLLOW offset, smoothed; KEY transposes the cascade -> INPUT = DeskDrive when
  INPUT > 0 (gain 1 + 99 x taper, up to +40 dB; IIR low cut; ultrasonic lowpass pair; quintic
  saturator whose curve fades to a hard clamp at full drive; clamp +-8), bypassed at INPUT 0 ->
  cascade of six sections at (morph, q) with BITE (tanh on the delayed state) and the ring leveller
  inside -> leveller on the cascade output (X3 table, gain only down, wakes at 0 dBFS) -> OUTPUT
  -12..+12 dB, unity at the centre -> soft guard -> output meter. So OUTPUT at rest is unity; only
  the printed number said 50. Changed: face value box and host string print dB ("+0.0" at rest,
  PluginEditor.cpp formatValue, TrenchParameters.cpp attributes); forceCleanAudioUiState now 0.5.
  FLAG: INPUT is a drive stage (DeskDrive), not clean gain; the ruling says clean gain. Told Tyson.
- Tyson: "why is it +? its confusing? 0-100 it should be" -> asked where unity sits -> "0 is unity, up
  is boost". OUTPUT law restated: 0..+12 dB, unity at 0, printed 0..100 like INPUT (kOutputMinDb 0,
  default 0.0, pctAttribs; the dB readout removed; forceCleanAudioUiState 0.0 is now right). Tests
  restated: the OUTPUT cleanness probes drive 0.35 at amplitude 0.25 (0.6 x +4.2 dB hit the guard knee
  at half scale, harmonics 4 %: guard, not OUTPUT) and the unity probe at 0.0; +12 dB at full still
  asserted. TRENCH_Tests: all pass except the two pre-existing face cases. Face shot: OUTPUT reads 0.
  Installed 17:15 into the bundles' Contents/x86_64-win (the ship file was held by FL: old moved to
  out/vst3-stale). Saved sessions with OUTPUT 50 now sit at +6 dB.
- Tyson: "the filters arent as level as they are on the x3 ... find out from the bytes". Findings from
  the bytes (scratchpad frames_responses.csv = every bank corner's response; scripts inline):
  (1) the 33 .bin word files are E-mu's, a lossless re-encoding of the Audity ROM bytes, and our
  decode is the DLL kernel, so shapes = the X3's. (2) X3 gain words: 116/132 corners carry one
  identical word x6 = DC 0 dB per corner; 16 corners the same with 6 dB exponent cuts. Peaks then
  sit +13..+55 dB over DC (Klang Kling M0Q0 +54.6). (3) The hardware ROM row's byte 48 is a per-Q-row
  level trim (-3..-67 dB, shared by both Morph ends, k0->k15 moves e.g. -47->-67 Early Rizer);
  the port dropped it; it does not track our peak growth (r 0.18); the chip's section scaling is
  unknown so it cannot be applied as a rule yet. Flag word 0x4000 on Fuzzi Face, Cruz Pusher,
  Dream Weava, Klang Kling (the absent-zero bodies), 0xC000 elsewhere. (4) Vulcan (Morpheus)
  firmware law already read: per-section pole DC normaliser + one corner gain after the cascade =
  also DC-unity per corner. (5) MORPH/Q curve tables are identity. So the difference to the X3 is
  after the cascade: the DLL leveller wakes at |x|>=2 and wraps its index (a hard limiter above
  +6 dBFS); ours wakes at 0 dBFS and clamps. Emulator X VST2 (Common Files/VST3/EmulatorX.dll,
  VSTPluginMain) cannot be hosted from code here: pedalboard refuses VST2, DawDreamer segfaults
  on load twice (activation?). Asked Tyson for an X3 render of one body at four corners + dry.
- Tyson: "Trench x3 clean and downloads has renders" / "No thats an acid 303 i wanted to fit" (the
  Downloads c0R0/cutoff*/distorted303 files are 303 fitting targets, not references). The x3-clean
  era plugins (out/vst3-stale Aug 27/31) load in pedalboard and DawDreamer but ignore body/morph/q:
  their body load runs on the message thread neither host pumps, so they play "no filter" (+4 dB
  desk). Their roster never held the P2K bodies (9 X3F factory types + 17 xml + MEAS). Dead end for
  a scripted A/B. Tyson: "measuring db doesnt take care of the actual sound fidelity" -> the
  fidelity test inside today's engine: TRENCH_Render/RenderDev gained --ring 0|1 (dspBridge
  .setRingLeveller) so the bare cascade, ring on, and BITE 18 % can be heard side by side.
- Fidelity renders (scratchpad demos/fidelity): Talking Hedz bare peak -1.9 dBFS vs ring on -13.5
  (the ring leveller pulls the formants ~12 dB inside the cascade); Ooh To Eee: ring no effect,
  BITE 18 % -2 dB. Tyson: "Amazing. Try with no morph smoothing. Agc on. No distortion":
  TrenchDspBridge gained setGlide(bool) (glideOn false = set_immediate instead of the 32-sample
  kernel glide); Render gained --glide 0|1. Rendered talking_hedz / ooh_to_eee / bassbox_303 saw +
  drums through BassBox 303 with --bite 0 --ring 0 --glide 0 --input 0, leveller on. Sent.
- Tyson on the four no-glide renders: Talking Hedz and Ooh To Eee disliked, BassBox 303 best. Then:
  "It should be at the sample rate. Float." Built: core PackedBody::interpolate_biquads_float (float
  trilinear on the words, decode_fractional = piecewise-linear between adjacent codes, so the
  log-lerp of the codes stays, no integer truncation), section_values_to_biquad split out; bridge
  perSample (default ON): the cascade is re-evaluated every sample the morph moves, set_immediate,
  no 32-sample tick or glide (falls back to the word path when datum != host rate, rewarp is on
  words); processor: the wheel ramps linearly across each block from the previous value when
  MOVEMENT and FOLLOW are off (wheelRampFrom), so a host's per-block morph becomes per-sample.
  Render: --persample 0|1. Cost 0.002 s engine per 1 s audio at 44.1k. TRENCH_Tests: only the two
  face cases fail; core+native 65/67 (two ctest entries Not Run point at plugin exes, pre-existing).
  Per-sample renders differ from block-stepped ones by -52..-82 dBFS rms (slow sweep; shows on fast
  moves). Not yet installed.
- Tyson, with two X3 FILTER panel screenshots: "its too wide make it thinner more relative to our
  reference and make it a little bit smaller" -> "The ui. Make it more thinner". Theme.h
  kEditorWidth/Height 352x543 -> 300x480 (the layout table maps from the source space, the plate
  stretches with it). FaceShot at 300x480 in scratchpad face_thin/. Awaiting his verdict before
  the plugin is rebuilt and installed; the site's face image and the two 200 % shots follow.
- Tyson: "The gain room sucks. Lets synthesise 3 radically different approaches for the theme and
  general ux" -> "Render some with swatches of color etc. curve and wheel glow same color". Three
  directions mocked as an artifact page (scratchpad face-directions.html): the module (plate, wheels,
  gain as a line-level strip with in/out LED ladders), the instrument (one glass, drag a point on the
  MORPH x Q square, curve behind it, gain as edge faders, no wheels), the page (manual page, figure +
  a settings list you drag or type). Swatches: mint, phosphor, amber, ice, rose, ember, bone set the
  curve and every glow together; plate tones beige/graphite/black/cream. Recommendation: 2, fallback 1.
- Tyson: "Just change the current face in faceshot not html". UiLayout::defaults() now honours
  TRENCH_GLOW=#rrggbb (accent, curveColour, rollerIllumination, modulationLamp = the glow,
  curveHighlight = glow.brighter(0.6)); FaceShot rendered the real 300x480 face at mint 3cc8be,
  phosphor 6cf58a, amber f0b043, ice 9ad7ff, rose ff5fa2, ember ff4b3e, bone f2f2ee; contact sheet
  scratchpad swatches/swatch_sheet.png. The curve and the wheel glow share the colour by rule.
- Face variations on the real face (FaceShot, env switches, all uncommitted): TRENCH_GLOW (family:
  glass/grid/curve/wheel from one hue; wheel strip recoloured in WheelControl::recolourGlow, grid plate
  tinted, glassTop/glassBottom/gridTint theme keys), TRENCH_GLASS / TRENCH_CURVE / TRENCH_WHEEL /
  TRENCH_GRID (independent), TRENCH_GAIN = pocket | row | numbers | corners | stack (BayKnob boxOnly,
  room frame off, GAIN word hidden; the plate bitmap's notch stays in all). Sheets in scratchpad
  swatches/family_sheet.png, gainroom/gainroom_sheet.png, glass/glass_sheet.png. Tyson: "Bone kinda
  works. The glow doesnt rlly tho"; "Half assed and weak" (before the real sheets reached him: I had
  rendered three sheets and sent none; sent all three then).
- Tyson: "Make the grid visible" (grid plate was 10 % alpha at 45 % opacity: invisible; now alpha x
  gridBoost 6, opacity 1, TRENCH_GRID_BOOST) -> "They all suck. And the grid should be black" -> lit
  glass set (scratchpad glass3/lit_glass_sheet.png): mid-tone glass sage / slate / olive / warmgrey /
  steel / teal_x3 / midgrey / bluegrey, black grid at boost 8, bone curve, saturated wheel. Awaiting
  his pick.
- Tyson: MATLAB R2025b installed, his scripts (corner.m, sound_to_skeleton.m, voice_body.m,
  first_pole_zero.m, fit_body.m, match_body.m) copied to evidence/research-results/matlab with
  SOURCE.md. corner.m = 50 ms slice, pre-emphasis 0.96, Burg order 12 at file rate, roots -> lanes.
  Proposed to him: pre-emphasis and Burg into body_from_audio's SPEECH mode (keep 11,025 Hz).
- Tyson pasted a 2008 UI playbook (pill gradients + top lip, crescent sheen on the glass, 1 px etched
  type, knob glint at 10 o'clock, slate or steel glass, keep the pocket). Built on the real face:
  pills (Theme.h drawFrostedGlassControl), sheen (GraphDisplay), etched labels (LabelsLayer,
  drawBayCaption), glint (BayKnob), defaults now slate glass 4f6478 / black grid boost 8 / bone
  curve / ice wheel (UiLayout). Sheet scratchpad face2008/face2008_sheet.png.
- Tyson pasted three lower-deck directions (kinetic rig, cartridge bay, test bench) and then the
  Rossum Morpheus module photos: "Make it like this. Striking". Built TRENCH_THEME=rossum (silver
  plate by luminance lift, blue 2f72d8 wordmark and accent, black glass, blue pixel grid at boost 14,
  white curve, red/green/blue dotted graticule at +18/0/-18 dB, silver knob caps, 11 blue dots per
  knob) and TRENCH_GAIN decks rig / bay / bench painted in FaceplateView::drawDeck (rocker ENGAGE +
  stamped MUSICAL FILTER; recessed bay with DIP bank, LEDs, patch strip; stepped -12..CRUSH ticks,
  IMPULSE/PHASE bat toggles, BNC). Sheet scratchpad rossum/rossum_sheet.png. All env-switched, nothing
  installed; the ship default is the 2008 slate set above.
- Tyson: "Gain room needs to be entirely rewritten. Use the same curve rendering as them". Built:
  BayKnob::paintDeck (TRENCH_GAIN=deck: two Morpheus-style knobs at (36,362) and (156,362) 108x96,
  black knurled skirt, silver cap with pointer, 11 blue dots lit to the value, label beneath, value
  in small text, no boxes, no frame) and GraphDisplay::drawPixelTrace (pixelTrace / pixelCell theme
  params: the response as 2 px cells per column with vertical runs between rows, as the Morpheus
  OLED draws it). Sheet scratchpad rossum2/rossum_deck_sheet.png.
- Tyson: "Its too much like the morpheus. Also make use of juce 9 svg rendering". Built
  TRENCH_THEME=methods (dark plate by luminance, bone labels, mint accent/curve/wheel, black glass,
  dim bone grid boost 7, mint pixel trace, dotted 0 dB line, zeroLine param) and the first SVG assets:
  plugin/assets/svg/knob_body.svg, knob_pointer.svg, tick_ring.svg in TRENCHAssets; BayKnob::
  paintDeckSvg draws them through juce::Drawable (createFromImageData, replaceColour for accent and
  ink, pointer rotated -135..+135 about the centre) when knobSvg is set. Sheet scratchpad
  methods/methods_sheet.png.
- Tyson: "I hate it. Bring it back try something else" (the dark methods theme) -> "Keep the knob
  filmstrip" -> "Solve the rest of the parameters or cull them" -> "How do we solve it realistically".
  Default face unchanged (beige plate, 2008 slate set). Deck rebuilt on the filmstrip knob: tick ring,
  label, pill beneath; a third BayKnob FOLLOW (envAmount) joins INPUT and OUTPUT in TRENCH_GAIN=deck
  (18/106/194 x 352, 88x116). Cream SVG knob variant (TRENCH_KNOB=cream) kept as an asset, unused.
  Sheet scratchpad deck2/deck2_sheet.png. Plan given: one home per parameter (glass = MORPH, Q wheels,
  BITE drag, MOVEMENT word, FOLLOW lamp; deck = INPUT, FOLLOW, OUTPUT; KEY top right), lock, install.
- Plate drawn by code (TRENCH_PLATE=drawn, FaceplateView: rounded plate, brushed lines, sheen,
  bone inner line, dark edge, four screws) so the bitmap notch no longer runs through the deck.
  Sheet scratchpad deck3/deck3_sheet.png.
- Tyson pasted a critique of the three-knob deck (notch collision, FOLLOW knob duplicates the glass
  toggle, tick rings generic, keep the cutout's asymmetry; cull the FOLLOW knob; Fix A = two knobs
  in the left half, Fix B = vertical stack under the wheels with pills right) and "That sucks" (to the
  drawn plate, deck3). Built both fixes on the notched bitmap plate: TRENCH_GAIN=deck (INPUT 26,356
  / OUTPUT 126,356, 88x108, filmstrip knob, label, pill, no ticks) and TRENCH_GAIN=stack (knobs at
  the Morph wheel's left edge, 352 and 400, pills right, no frame). FOLLOW knob hidden everywhere.
  Drawn plate kept only as TRENCH_PLATE=drawn. Sheet scratchpad fixab/fixab_sheet.png. Awaiting A or B.
- Tyson: "Im just not a fan of the ux" -> asked which part -> "Too many things on the face". Built
  TRENCH_FACE=lean: BODY, glass, MORPH, Q only; INPUT/OUTPUT knobs, KEY box, glass words and FOLLOW
  lamp hidden; lower deck plain plate (notch kept). Sheet scratchpad lean/lean_sheet.png. Open: where
  INPUT, OUTPUT, FOLLOW, MOVEMENT, KEY live (host automation only, or a drawer opened on purpose, e.g.
  a pull tab in the notch), and whether the face shortens (plate bitmap would squash; needs the drawn
  plate or a re-cut bitmap).
- Tyson (voice): "the automatic gain control is in charge of the level and whenever the filter ...
  close to the ceiling, that's when our saturator is kicking in". LAW: the leveller owns level; BITE
  wakes only near the ceiling. Built: TrenchDspBridge biteAuto (default on): effective BITE = glass
  value x autoDrive, autoDrive = max(leveller reduction / 6 dB, (block peak - 0.5) / 0.5), instant
  attack, 0.85/block release; setBiteAuto, biteDrive(); Render --biteauto 0|1. TRENCH_Tests: only the
  two face cases fail. INPUT/OUTPUT stay host parameters off the face.
- Tyson: "That's so much better" (the lean face). Wants the darker glass with a desaturated mid tone
  ("chemical"); rendered chem_greygreen / slate / olive / neutral (scratchpad chem/chem_sheet.png).
  He sent two x3-clean-era faces: the dark glass with the "Modulation" chip bottom-left, GAIN pocket
  with INPUT/BITE/OUTPUT; and asks: the chip on the screen as the way into the modulation parameters
  and the movement preset; "what else could we possibly do?"
- Tyson (with the X3 panel): "Make the readouts slightly smaller and more secondary" / "Can we make
  it look cleaner?": morphReadout/qReadout rects 150x54 in layout space, 14 pt, ink 4b463e; rail labels
  "MORPH" / "Q" (no percent); glass sheen halved. Rendered on the lean face with grey-green dark glass
  (scratchpad clean/clean_sheet.png). Rendering is Direct2D (JUCE_DIRECT2D=1), per-monitor DPI, trace
  cache at 3x physical scale; bitmaps carry 2.7..5.9x the face's pixels. Tyson queried the
  double-click-to-freeze idea; answered: cut it unless it earns its keep, the chip panel matters.
- Tyson: "Resize it slightly thinner and smaller the whole ui" -> Theme.h 270x440. "Readouts are too
  small now" -> morphReadout/qReadout 172x62 layout space, 16 pt, ink 3f3a33. "Go back to one of these
  glass colors and grid etc" (his two x3-clean faces: black glass, faint grid, mint curve) -> the slate
  default block removed from UiLayout; defaults are the original glass 0c1412/050908, white grid at
  boost 3, pale mint curve bef0d7, mint wheel 3cc8be. Sheet scratchpad mintback/mintback_sheet.png
  (pale vs saturated curve). "So should our agc be different than the x3": answered in chat.
- Tyson "knee.": leveller rewritten (TrenchDspBridge, levellerKnee default on, --knee 0|1 in Render):
  a peak-held envelope (instant attack, release 150 ms, 20 ms once the cascade output has sat under
  0.5 for 50 ms) and a soft knee gain = 1 / (1 + 1.6 (env - 0.56)) above a hold of 0.56 (-5 dBFS, the
  level the X3 table settled at). Two dead ends on the way, recorded: a per-sample feedback knee with a
  gentle slope bends the wave inside each half cycle (4.8-5.7 % THD on Low Shape at -1 dBFS whatever
  the release), and a peak-held knee that lets the output sit at 0.87-0.90 pushes it into the guard's
  quadratic zone (above 0.5) for the same THD: the "unmusical clip" was the guard, and the X3 table
  was keeping the level under it. Now: roster worst 0.70 % (Low Shape, peak 0.60), all other plugin
  checks pass, the two face cases still fail. BITE auto-drive uses the same agcGain. Not installed.
- Tyson: "the lpc with 8 bit compression. was that seen in spectrogram software?": no. The 8-bit
  minifloat is the Audity/P2K ROM table format (proven from bytes); the Spectrogram program's Env
  is an order-12 LPC on screen and its saved filter format was never examined. Nothing in its strings
  or the report mentions 8-bit.
- MARIMO_PROMPT.md written at native/ (grid and interpolation first, engine via the C ABI, MATLAB via
  the Engine for Python as the FROM AUDIO reference, Qt app kept until parity).
- Tyson on knee vs table: "Much of a muchness. Are we using the actual agc": the table path is the DLL's
  leveller (values, per-sample update, gain only down, 1.0001 release) with our wake point (scale 2.0,
  0 dBFS) and a clamp where the DLL wraps. Default set back to the table (levellerKnee false, --knee 0
  default); the knee stays as a switch.
- Tyson: "Recover the agc" / "Ghidra's up" (the MCP bridge on 127.0.0.1:8080 refuses; the server
  plugin is not started). From the bytes with pefile + capstone: FUN_1802c04e0 confirmed line by line
  (index = int(gain x max|L|,|R|) & 0xF, gain *= table[index], reset to 1.0 when >= 1.0, both channels
  scaled); its one caller 0x1802d20d7 is the per-voice routine: filter kernels (virtual, by mode 0..4)
  -> AGC -> FUN_1802d44a0 mixer (per-voice level/pan ramps applied AFTER the AGC, then bus add). So the
  AGC sees the filter output at raw voice sample scale. Constant scan: 1/32768 x19 refs (16-bit
  decode), 4.0 at 0x1802c366a (packed decode runtime scale) and 0x1802ed0df/146/214 (kernel region).
  Open: whether samples enter the filter at +-1; if so the X3 wake point is +6 dBFS (scale 1.0, what
  x3-clean called "the DLL path, no pre-scale"), and our 0 dBFS (scale 2.0) is the departure.
- AGC recovery from the bytes, continued: the 1/32768 at 0x1802ce178 scales an LCG noise source and
  the 4.0 at 0x1802ed0df maps a parameter to a table, neither is sample scale. The decode at
  0x1802c3600 confirms our word codec (1/4096 denormal, 1/8192 mantissa, runtime scale 4.0). The
  mixer (0x1802d44a0) applies each voice's level/pan ramps after the AGC, so the X3 leveller works on
  the raw voice sample at +-1 and wakes at 2.0 = +6 dB over a full-scale sample. Bridge gained
  setLevellerScale (kLevellerScale 2.0 stays the default = 0 dBFS); Render --agcscale. Rendered scale
  1 (X3, +6 dBFS) vs 2 (ours) on the saw and the drums, sent.

## Session close 2026-09-04, evening (stopping point set by Tyson)
Everything from today is uncommitted (Tyson never said commit). Both VST3 builds installed at 17:15 carry
OUTPUT 0 = unity and the leveller at 0 dBFS; everything after (per-sample float MORPH, BITE at the
ceiling, knee switch, leveller scale switch, face work) is built in out/build but NOT installed.
Open, in order:
1. Face: lock the lean face (BODY, glass, MORPH, Q; 270x440; mint on black, faint grid at boost 3, pale
   or saturated mint curve, his pick; readouts 16 pt secondary; labels MORPH / Q). Then: the chip on the
   glass ("Modulation") as the door to MOVEMENT, FOLLOW, KEY; INPUT/OUTPUT to the host. Strip the env
   switches (TRENCH_FACE/GAIN/THEME/GLOW/GLASS/GRID/CURVE/WHEEL/KNOB/PLATE) from the ship path; restate
   the two face tests; re-shoot the site face image and tour GIF.
2. Level law: leveller owns level, BITE wakes at the ceiling (biteAuto, default on). Leveller = X3
   table at scale 2.0 (0 dBFS); the X3's own wake is +6 dBFS on the raw voice sample (scale 1.0),
   renders sent, his pick. Knee leveller parked as --knee. Ring leveller still defaults ON in the
   plugin though Tyson preferred the bare renders: decide.
3. Install + FL listen once 1 and 2 are ruled.
4. Workstation: marimo rewrite prompt at native/MARIMO_PROMPT.md (grid + interpolation first, C ABI,
   MATLAB Engine as the FROM AUDIO reference, 3-D cube view). Qt app stays until parity. Qt app
   today: FRAMES grid (304 tiles, hearing on hover), VOWEL SPACE, VOWEL H95 keyframes, clean bar,
   SO FAR column, axis names on the pad; 62/62 native tests.
5. Site: site/ folder with his brass/reese clips; he does not like those demos; set2 renders in the
   scratchpad were candidates (BassBox best). Names, domain, store, hosting untouched.
6. Research parked: dataset list filed; P2K hardware level byte (byte 48 per Q row) unexplained;
   Ghidra bridge needs the MCP server started in Ghidra to finish the AGC scale question.
- Tyson on the wake-point renders: "Yeah 1 sounds better not by much" -> kLevellerScale 1.0 is the
  default (the X3's own wake point, +6 dBFS on the raw sample); --agcscale default 1.0. Built; tests
  as before (only the two face cases fail). Not installed (face still open).
- Tyson "Install it": installed 20:27 (ship + dev) with kLevellerScale 1.0, per-sample float MORPH,
  BITE waking at the ceiling, X3 table leveller, ring leveller still on, face = default with the pocket
  (face not locked). Cost of the X3 wake point: a -1 dBFS sine through Low Shape now rests at 0.92 and
  the guard's quadratic zone (from 0.5) gives 6.4 % THD, which is the ruling ("the saturator kicks in
  close to the ceiling") happening. Test restated by that ruling: the roster sweep now runs at
  -12 dBFS (0.251) with the same 1 % line; the -1 dBFS Cross Band check stays and passes.
- With the X3 wake point the restated -12 dBFS roster sweep still reads 1.19 % on Low Shape (peak
  0.68): the guard's quadratic zone starts at half scale, so any body boosting a -12 dBFS sine past
  -6 dBFS gets the guard's curve before the leveller acts. Left FAILING on purpose (three failures:
  this and the two face cases). Decision for Tyson: keep (the saturator near the ceiling is the law
  he set) or raise the guard's linear zone from 0.5 toward 0.8, which departs from the firmware soft
  clip but keeps quiet-to-moderate signals clean.
- Tyson (FL screenshot, the installed default face clipped at the OUTPUT knob: the pocket's rows sit
  at 363..461 in a 440-tall face): "commit with this. needs re measureing. gain is not solved.
  modulation is not solved." Committed: 1e253886 (native, evidence without emu-sgi-1993 and the
  102 MB lineage folder, site, this brief) and ed663171 (plugin). Left uncommitted on purpose: root
  strays from other sessions (workstation.py/html, __marimo__, recipes/, tools/, output/, test_*.py,
  pngs, Launch bats, design-qa.md), .claude/, and the two big evidence folders. Note: another chat
  has already started the marimo side (workstation.py, native/core trench_core_c.*, native/python)
  -> the new-chat prompt should read those first.
- 2026-09-04 late, face locked as the lean face. Tyson: "the filter is the gain" -> INPUT and OUTPUT
  are host parameters only; no gain knobs on the face. The clip was the pocket: rows from y 363, 100
  tall, in a 440-tall face, never fitted. Deleted rather than fitted: kBay* constants, the TRENCH_GAIN
  eight-mode chain, TRENCH_FACE, BayKnob.h, KeySnapBox.h, GlassWords, FollowLamp, LightMenuLnF, the
  gainLabel element and its LabelsLayer draw, FaceplateView deck and room frame (1226 lines out, 49
  in). New: ui/ModulationChip.h, paint only, dot + "Modulation" at glass x+12, bottom-26, 110x18,
  lit by isMorphModulatedForUi; from his 2026-07-24 screenshot. No click yet; the door to MOVEMENT /
  FOLLOW / KEY is its own session with his spec. Env switches TRENCH_THEME/PLATE/KNOB/GLOW/GLASS/
  GRID/CURVE/WHEEL/GRID_BOOST/WHEEL_STRIP now sit behind #if TRENCH_DEV_PANEL; the ship lib reads
  only TRENCH_HEADLESS, TRENCH_SHOW_ONBOARDING, TRENCH_ONBOARDING_HOVER. Tests: every visible child
  must sit inside 270x440 (would have caught the clip), chip exists and sits inside the glass; runs
  before the HEADLESS skip so ctest exercises it. trench_plugin still fails on the one deliberate
  case, Low Shape 1.19 % at -12 dBFS (guard zone decision above). Site face PNG re-shot from the 2x
  FaceShot at 489x797; trench-tour.gif still needs a screen recording.
- Level, ruled by ear in FL: guard linear zone 0.5 -> 0.8 (kGuardLinearZone, SlamStage.h) fixed the
  roster THD; BITE auto wake moved to the same constant, then switched off by default: the drag IS
  the threshold (Rossum US10514883, granted 2019-12-24; radius law in core audition.cpp is now
  r - r(1-r)(|v|-vt)/|v| past the threshold, delayed-state soft clamp kept from the 1992 paper).
  Leveller: X3 table at scale 1.0 is inaudible (wakes at +6 dBFS on the raw sample); Tyson "lower
  the hell out of it", then "4-8x is the sweet spot, a bit too low at 8x" -> kLevellerScale 6.0
  (wakes near -15 dBFS, full scale rides to about 1/6). Two acceptance tests restated to that
  law: identity impulse now -24 dBFS (below the wake), no-mute floor = 0.9/scale. Drawer: AGC on/
  off, BITE AUTO, AGC 1x/2x/4x/6x/8x. Mackie desk as limiter: no, it is INPUT drive, host only;
  cut it if nobody reaches for it. KEY: Tyson "getting rid of key was a mistake"; the box and its
  wiring are in fbb48875, one commit to restore at the keyBox rect beside BODY, awaiting the word.
- 2026-09-05, the authoring tool: TRENCH Workstation is a JUCE standalone (target TRENCH_Workstation,
  source native/workstation/, plan native/WORKSTATION_PLAN.md). The HTML model workstation_min.html
  (tools/build_workstation_min.py, core maths checked to 1e-13 dB) is the spec: frame space on a
  musical grid (1st resonance across, 2nd up, C octaves), Delaunay play, an orbiting wireframe cube
  with eight corners each drawn as a crude response, axes named by pose ends (high > low, closed >
  open, relaxed > stressed), plane drives MORPH and Q once eight corners are set, CAPTURE keeps the
  wheel's words as a frame. Skeleton built and rendered headlessly (--shot path.png), 304 frames.
  No sound, no export yet. Leveller: the X3 table is a limiter whose ceiling is 2/scale (6x = -9.5
  dBFS, hence "quieter with AGC on"); recommendation to cut AGC from ship and keep BITE as the
  ceiling, awaiting Tyson. Evidence added: timbre-space papers, Massie collection (SOS 1995 full,
  ICMC 1992 pole-zero paper, patents), UltraProteus manual pages, Martens Palette and PCA pages.
- 2026-09-05, TRENCH Workstation (native/workstation, target TRENCH_Workstation, JUCE 9 + OpenGL,
  SVG through lunasvg for --shot): the frame space is anchors on Martens' plane (first two
  principal components of the response set, computed in C++), eight sort measures, triangle blend
  in word space, PAIR mode for a straight slot-to-slot morph between two chosen anchors, the
  ARMAdillo plot of the blend with slot glides, a timeline with keys and play, the BODY block
  (four corners round a square with the wheel, six row switches, EXPORT to
  plugin/presets/user/*.body240, verified word for word against the bank), and the corner editor
  (six stage rows, each with its own magnitude and pole and zero handles, OPEN as Klatt's F1).
  Not yet: sound in the workstation, FROM AUDIO, loading a user body in the plugin drawer. Rules
  from Tyson this session: no glow, no sentences on the surface, no stock widgets, no chrome, a
  stage is a row, pairing is slot to slot only, one response curve.
- 2026-09-05, workstation, verbatim from Tyson's request: "SPEECH is linear prediction at order
  12 on the sound resampled to 11 kHz, the classic speech envelope, which yields the poles
  directly. BELLS is the core's all-pole fit at a higher order followed by peak picking. Both are
  LPC envelope methods, the same family as the Env and lattice modules in Peevers' program, but
  not his code. We only have his binary, so nothing here is his implementation, and I will not
  claim it is." Resampling: analysis through the core's resample to 11,025 Hz inside
  speech_poles; playback by linear interpolation to the device rate in Audio.cpp; the cascade
  rewarped from the 44,100 Hz datum by the core's rewarp_cascade. Two rooms (FRAMES, SOUND);
  one implementation each (CPU surface and HTML model removed). Audio out is in: LISTEN loops a
  region through the current cascade. GL path and audio path have not been run on screen by
  the assistant; the software shot is the check.
- 2026-09-05, workstation focus pass applied: three rooms (FRAMES, EDIT, SOUND) on keys bottom
  left; one right column in every room (corner block, ARMAdillo, response); tray groups collapse
  by clicking their header; sort is one line (across, up, SORT); PAIR hides every anchor but the
  two; timeline is a 24 px strip until it has keys; the shaded field, the rows table in FRAMES,
  the duplicate cascade, the group keys and the ghost marks are gone. Colour rule: yellow is
  live (probe, wheel, slice, playhead), cyan is chosen (pair, picked anchor, zeros, region,
  keys that are on), white is data, grey is structure. EDIT room: row numbers are the row
  switches, LOCK per row, CEILING on row 6, corner picker top right. Built through vcvars64,
  shot headless (ws.png, ws_field.png, ws_sound.png), export still 240 bytes. GL and audio
  still unproven on screen.
- 2026-09-05, five novel bodies baked by tools/novel_bodies.py into plugin/presets/user/novel
  (240 bytes each, DC unity within 0.006 dB, corners 4-7 mirrored): head (left ear front > left
  on MORPH, behind and above on Q, from the SONICOM frames), anti_vowel (Ooh To Eee against its
  own poles-for-zeros negative, ceiling row kept; at MORPH 0.5 the span is 4.5 dB, flat),
  zeros_only (Eeh To Aah poles held, zeros walk from eeh to aah), chimera (six rows from six
  sounds: Aud Wall, eh head, uh hud, Talking Hedz, left ear, Talking Hedz ceiling), fifth (Ooh
  To Eee with every pole and zero times 1.5 on the Q corners, ratio 1.500 measured). The
  plugin drawer cannot load user bodies yet; audition through the workstation or the dev roster.
- 2026-09-05, workstation caricature (native/WORKSTATION_CARICATURE.md) built: PAIR t runs -2 .. 3,
  the wheel runs -1 .. 2 inside a margin square, timeline keys sit on the extended pair line and
  drive t in PAIR. Out of bounds the push is row by row on the geometry (pitch in semitones times
  t, bandwidth as log(1 - r), zero radius linear and capped at the circle, gain in dB), row 6 held
  at the nearest end, pole radius guarded at 0.9995 in the word domain; in bounds the words are
  the plugin's interpolate_word truncation, matched word for word. Drawn: cyan extension lines
  with end ticks, faint outer rule on the wheel, readout cyan outside, response curve broken at
  the frame with cyan ticks at the excess, guarded poles as cyan squares. GROUP MEAN key on each
  tray header, "vowel schwa" added to VOWELS (uniform tube 500 .. 4500 Hz, 17 kHz tilt pole,
  ceiling zero). Tests: TRENCH_WorkstationTests (ctest trench_workstation), 9 checks pass; worst
  doubling error 0.64 semitones at t = 2 (Multi Q Vox row 2, a 0.9987 pole), rows pushed past
  Nyquist are clamped there. Two spec points refused by the word format: a zero cannot pass
  the circle (word 1 stores 1 - r squared) so no zero is ever drawn outside the rim; the P2K bank
  itself carries poles at r 0.99979 in bounds, left untouched since in bounds nothing may change.
  UI split by concern the same day (Workstation, Keys, Actions, Input, Scene, Paint, Style.h).
- 2026-09-05, workstation look and rendering, two rulings from Tyson: "The black and cyan is
  amateur" -> palette is MATLAB crossed with a high-end Unix workstation: ground #cccccc (the
  classic MATLAB figure grey), white axes panels, black frames, light grid, black text, keys as
  grey boxes that invert when on; data MATLAB blue #0072bd, chosen MATLAB orange #d95319, live
  dark gold #c48f00. "Render the entire thing in custom OpenGL shaders" -> component painting
  is off; text, rules, keys, panels and the spectrogram all go through the GL renderer (a solid
  shader for points, lines, strips and rect triangles; a textured shader with a Consolas glyph
  atlas built at the context's scale, and the spectrogram uploaded as a texture). One scene
  description (render/Scene.h) built through render/Canvas, which mirrors the nine Graphics
  calls the chrome used; the headless --shot path walks the same scene with juce::Graphics
  (render/SoftwareRenderer). SVG and lunasvg are gone. GL path still not run on screen by the
  assistant; the software shots are the check.
- 2026-09-05, workstation typography, Tyson: "Change the typography from ai generic". Ruling
  applied: labels, keys, ticks, names and readouts in Arial 11 (Helvetica as MATLAB and the
  Unix workstations set their axes); Lucida Console 11 only for columned rows (editor stage
  rows, sound room pole list). The GL glyph atlas is now proportional, one atlas per face,
  size and scale, with per-glyph advances; the software shot path uses the same faces.
- 2026-09-05, workstation plots, Tyson: "No wide plots of magnitude". Ruling applied: every
  magnitude plot keeps a figure aspect. EDIT room: cascade 4:3 (300 x 225) top left, the six
  stage plots a 3 x 2 grid of 4:3 axes sized from the room's height, row key above each, pole
  and zero lines and the resonance dB below, LOCK and CEILING under those; corner thumbnails
  4:3 (160 x 120) in a 2 x 2 block. Response plot 3:2 (288 x 192) in a 240 px panel; the wheel
  block shrank to 180 outer / 60 inner to give the column room; the ARMAdillo takes what is
  left and its rings read 20, 40, 60 dB.
- 2026-09-05, the stitch built (native/WORKSTATION_STITCH.md steps 1, 2, 4), after Tyson sent
  two MATLAB figures (slice and stem3) for "the hero abstraction which is the whole factory
  corners". tools/stitch_graph.py builds native/python/workstation/stitch.json: 340 bodies
  (33 P2K, 18 xml as X3, 289 Morpheus cubes), nodes = distinct pole skeletons per floor (exact
  pole bytes, then near match 3% / 0.01 fused), edges MORPH/Q/Z, faces, stubs = loose frames
  attached to the nearest node by slot-paired pole distance, spectral layout per floor (giant
  component by the Laplacian's Fiedler vectors, small components on a ring around it), floors
  stacked on z. Census from the bytes: 132 P2K corners -> 127 exact -> 126 nodes, five shared
  exact pairs (not the ten the spec quoted; that number came from an image, not bytes); 2312
  Morpheus corners -> 1567 nodes; 1763 nodes, 3069 edges, 340 faces, 167 stubs. The FRAMES
  room is now the stitch in a MATLAB 3D axes: boxed axes, dotted grid on the far planes, floor
  names as z ticks, stems from each floor to its node, faces filled with the parula ramp by
  mean resonance, MORPH/Q/Z edges in three greys fading with depth, stubs orange. Orbit by
  dragging empty space, wheel zoom, HOME. Press a node, an edge (t by projection) or a face
  (M and Q by inverse bilinear) to play it through pairMorph / wheelMorph; PAIR picks two
  nodes; click a node to pick it for a corner; drag a node to a corner key or the timeline;
  CAPTURE adds a stub; timeline keys are Spots. The Martens plane, Delaunay and the sort keys
  left the room (Library keeps them for the tray and the tests). Not yet: the walk through a
  shared node into the next face (step 3), EXPORT adding a face (step 5), Morpheus nodes are
  played at the 44.1 kHz datum and truncated to six rows.
- Tyson 2026-09-05 on bookkeeping: E-mu presets stay in the stitch as reference floors, every
  node carries its member list; the organic matrix is the floors that grow (captures, reads,
  exports); factory floors fold from the tray.
- 2026-09-05, Tyson on the first stitch render: "Wrong execution its not easy to understand";
  on my axis options: "Random? Is that better?". Ruling taken: no invented layout; the hero's
  axes are the ones already law. Corners stand on the fixed log grid at their strongest peak
  (frequency across, level in dB along), stem height is resonance in dB, floors are the
  sources. Nothing is drawn but stems and marks; pressing a stem lights that body's square
  (edges and parula fill), so the surface is read one body at a time. The spectral layout
  stays in stitch.json unused. Commit 2 of the stitch.
- 2026-09-05, Tyson: "How do we abstract it properly without confusion" (asked twice). Answer
  built: one idea per level, never two levels at once. Level one, the shelf: one stem per
  filter (340), at the filter's own peak with the wheel centred, height its resonance, on its
  source floor; read frames as orange marks. Level two, the filter: press a stem and the
  shelf dims, the filter opens as its square (corners, MORPH and Q edges, parula fill); the
  filters sharing a corner with it are the only other things lit, and pressing one walks
  into it; drag inside the square is the wheel, press a corner for that corner alone. Press
  empty space to close. Level three is EDIT as before. Words: filter, morph, Q, frame; no
  nodes, edges or faces on the surface. Shots: ws_shelf.png (closed), ws_pair.png (open).
- 2026-09-05, Rossum Electro-Music Morpheus cube dump decoded from the firmware audio
  (evidence/factory-data/morpheus/rossum/cubes_v1.01vc_170120.wav.zip). Decoder:
  evidence/research-results/rossum_morpheus_cubes_decode.py, findings alongside as .txt,
  table as rossum/rossum_cubes_v1.01.json. Modulation: 6 kHz biphase at 48 kHz, half cycles
  of 4 or 8 samples, FM (short-short = 1, long = 0), 6000-bit leader; bytes LSB-first; 289
  records x 332 bytes = 12-char name + 12 header bytes + 7 rows x 44 bytes. The two earlier
  files (mfm.bin, demodulated.bin) were misdecoded and are superseded. Names are the 1993
  manual list in order (record 1 = Null Cube, record 2 = filter 1), 127 exact, the rest
  abbreviated to 12 characters. Rows 0..5 hold sections 2..7; each row carries the pole
  pitch of all eight corners as LSB-first note counts, 64 counts per octave from about 0.29
  Hz (ten bits; corner 6 eight bits at sixteen per octave), at fixed bit offsets keyed by
  1993 corner; agreement with the 1993 poles within three semitones for 97 to 99.8% of
  poles per corner, median 0.8 semitone with approximate scale constants. So the module
  stores Rossum's designer numbers, pitch in notes, not H-chip coefficients, and compiles
  them for its own rate at runtime; it has no datum and the same 289 cubes by name and by
  pole pitch. Unresolved: bandwidth, gain and zero fields, section 1, the 12 header bytes,
  and a per-corner refit of the note scale. Answers Tyson's "what about the Rossum
  Morpheus": rewarp by recompiling from designer numbers is exactly what Rossum did.
- 2026-09-05, the chord representation (Tyson: "Please refactor for that exact representation",
  then "What about chords"). A frame's truth is now a Chord: six Stages, each a pole Voice and
  a zero Voice (on, note as a MIDI number, width in semitones) and a gain in dB; the words are
  compiled from the chord at the app's datum and kept as a cache. decompile(words, datum) reads
  any source at its own rate (P2K 44.1 kHz, Morpheus 39,062.5 Hz); compile(chord, datum) writes
  words for any rate; rows with no conjugate pair keep their raw words so nothing is lost.
  Width = 12 log2(1 + bandwidth / pitch), bandwidth from -ln(r) fs / pi. Round trip over the
  132 P2K corners is byte-exact on all 792 rows; a Morpheus chord compiled at 44.1 kHz lands
  within 0.001 semitone of its 39 kHz pitch, so Morpheus nodes on the shelf now play at true
  pitch. The caricature push is on the chord (note lerp, log width, dB), and a pushed voice is
  narrowed until the words can hold its pitch (fitVoice), since a low wide pole has no bytes;
  doubling error fell from 0.64 to 0.08 semitone. EDIT rows read "pole G7 -29  w 1.1 st",
  the frame's six notes read as a chord line; chordFrom(root, intervals, width, dB) seeds a
  frame from a chord. Tests: 12 checks pass (ctest trench_workstation).
- 2026-09-05, first run on screen (Tyson): GL path renders, chrome and text included. Fixed
  from the screenshot: Latin-1 glyph atlas (the middle dot drew as ?), shelf marks and stems
  sized for 1x, depth fade 30%, glides only for parents above 30%, Morpheus names without
  None. Tyson: "i cant move through the space which is the entire thing" -> free movement
  restored on the shelf: press and drag anywhere on the active floor and the mark follows
  the pointer, the sound is the inverse-distance blend of the four nearest filters' centres,
  thin lines show the parents, the status line names them with their percentages; floor
  keys (P2K, MORPHEUS, X3) pick the floor, opening a filter picks its floor; orbit moved to
  the right button or Alt-drag. Tyson: "the spectrogram room is sick. but there's no sound"
  -> LISTEN now names the device and rate (or the open error) in the status line, and plays
  a built-in 110 Hz saw through the cascade when no WAV is loaded, so the FRAMES room sounds
  without visiting SOUND first. Not yet verified with ears by the assistant.
- 2026-09-05, chord files and the bake tool (Tyson: "build the format and the tool").
  Format trench-chords-v1: {"schema","floor","source","chords":[{"name","source","stages":
  [{"pole":{"note","width"}|null,"zero":{...}|null,"gain_db"} x 6]}]}. tools/bake_chords.py
  reads formant tables (CSV, --group-mean makes the 48 Hillenbrand keyframes by vowel and
  speaker group), impulse WAVs (LPC, speech order 12 at 11 kHz or bells order 24), SOFA heads
  (h5py, one chord per azimuth step at an elevation), and magnitude tables (minimum-phase
  impulse by cepstrum, then the reader). Baked: hillenbrand_1995 (48), head_p0001 (12),
  qsound_right90, magnitude_example, in native/python/workstation/chords/. The workstation
  loads every chords/*.json at start (Library::loadChords) and every library frame that is
  not a factory corner enters the space as a read frame.
- 2026-09-05, the space, third try after "i still cant move through the hero cube. its the
  wrong abstraction. whats the right one": three continuous musical axes. Pitch across, level
  along, resonance up (0..60 dB), one box, no floors. Every filter (at its wheel centre) and
  every read frame stands at its strongest voice, coloured by source (P2K blue, MORPHEUS
  teal, X3 grey, VOWELS orange, HEADS purple, XL-1 gold, INSTRUMENTS red), foldable from the
  tray. Drag anywhere moves in pitch and level at the current height, Shift-drag moves the
  height, the sound is the inverse-distance blend of the four nearest items in the box (z
  weighted 0.6), thin lines to the four parents, a gold stem under the live point. Orbit is
  right-drag or Alt-drag. RAZOR key in SOUND sets every read voice to a 0.25 st width before
  FRAME, the P2K pole-only sharpness. Brief for an outside opinion on the abstraction at
  native/PROMPT_HERO_SPACE.md (Tyson wants it sent to another model at max reasoning).
