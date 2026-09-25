# TRENCH instrument pass, 25 September 2026 (evening)

Continues `output/switch-safety-20260925/HANDOFF.md`. Checkout was already dirty; the files below also carry earlier uncommitted work.

## Changed

- `plugin/source/dsp/TrenchDspBridge.h`: the per-tick coefficient glide now ramps in the encoded log-word domain (`CascadeRunner::set_target` with the tick length) instead of ramping decoded biquad rows. INPUT is plain gain into the filter (user ruling, evening). OUTPUT is the Airwindows Mackity process path itself (`DeskDrive.h`): In Trim from unity at the knob's first step to +40 dB at full, Out Pad 1, no compensation, no inflator; `Inflator.h`, `Inflator-SOURCE.md` and `DeskCompensation.h` are removed. Body crossfade from the earlier session is unchanged.
- `native/core/{include/trench/core/audition.hpp,src/audition.cpp}`: `set_target` takes an optional ramp length.
- `plugin/source/PluginProcessor.{h,cpp}`: project state embeds the 240 body bytes (`bodyBytes`) and recall recovers a missing body file into User Bodies (or installs the bytes if the library cannot list it); `bodyRecoveryDirectory` is injectable for tests.
- `plugin/source/dsp/Movement.h`: `authoredBars`/`authoredLengthChoice`; host-locked loops anchor to the song's bar grid (once-gestures and Restart still anchor where they are triggered).
- `plugin/source/ui/ModulationChip.h`: choosing a movement applies its authored length and preset playback; the chip reads `Name · length · once|loop`; the movement browser gains Length, Playback and Restart rows.
- `plugin/tools/Render.cpp`: `--output` now drives the OUTPUT parameter (it addressed the retired slam parameter).
- Tests: expectations updated for the authored length, once/loop text and grid anchoring; new PASS lines for grid anchoring, authored lengths, browser rows, encoded-domain round trip (worst 3.9e-16), type change keeping Morph/Q/KEY/INPUT/OUTPUT/movement phase, and missing-file recovery. The drive-slam inflator checks are replaced by Mackity's In Trim law and its full-scale cap.

## Verified (headless)

TRENCH_Tests 275 PASS, 1 FAIL (ring leveller, see below). TRENCH_CalibrationTests (dev) passes with the OUTPUT ceiling check bounded at 1.5 in dev, because the dev build has no final clamp and real Mackity rings to 1.28 on clipped resonant material (release clamps at the ceiling). TRENCH_ReviewTests 30 PASS. TRENCH_SwitchSafetyTests 437 checks, 0 failures. TRENCH_UserBodyTests 14 PASS with `User Bodies\talking_hedz.body240`. KEY graph versus audio with KEY F# on the crisp body: worst 0.0017 dB. Engine cost with the encoded ramp: 0.0193 s per second of audio, KEY on 0.0197 s.

## Ring leveller check

`the ring leveller drops the peak by at least 6 dB` fails only because the roster now ships Flanger 3: its S3 is a +78 dB pole at 220 Hz fed through -68 dB of zeros in S1 and S2. Through the bare core runner the leveller cuts that tone by 54 dB (fresh or toggled live); through the processor via `TRENCH_Render --ring 1` the rms drops 30 dB. Inside the test harness it measures 0.41 dB and I did not find why. The leveller is off in the shipping path (`prepareToPlay`) and the user has said no levellers are wanted, so the check exercises a dormant feature. Threshold untouched.

## Not done

- Install: FL Studio (FL64.exe) still holds `C:\Program Files\Common Files\VST3\TRENCH.vst3`. Built module SHA256 `524c7741a17640eade95ab1f25e01741688e5e3844ebdcb30f99fafbf2192da0`; installed is still `1076811f...`. Save the project, close FL, run `Install TRENCH.cmd`, reopen.
- Listening: the user heard the bare-filter corner renders (`preset-check/`, 5 files) and confirmed the preset. The final chain is in `gain-in-mackity-out/` (10 files: ref, INPUT 100, OUTPUT 50, OUTPUT 100, both at 50, for Hedz and Millennium). Rejected in-loop and glide-domain renders were deleted; the glide difference measured 50 to 96 dB under the signal.
- OUTPUT taper: the knob rides Mackity's own square law, so 50 is already +30 dB into its clip. If that is too hot in the hands, the mapping (`DeskDrive::inTrim`) is the one place to change.
- Dev only: `trench_calibration` fails on `cal_grid audio delta 0`. That control only forces a body re-switch, which the crossfade from the earlier session now makes seamless, so it has been inert since then; not touched.
- KEY hysteresis: audio holds a note within 0.15 semitone of a scale boundary while the graph probe shows the nearest note; static agreement is proven, the hold band is not drawn.
