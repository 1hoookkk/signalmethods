# TRENCH instrument pass, 25 September 2026 (evening)

Continues `output/switch-safety-20260925/HANDOFF.md`. Checkout was already dirty; the files below also carry earlier uncommitted work.

## Changed

- `plugin/source/dsp/TrenchDspBridge.h`: the per-tick coefficient glide now ramps in the encoded log-word domain (`CascadeRunner::set_target` with the tick length) instead of ramping decoded biquad rows; INPUT is a Mackity desk (`DeskDrive`) before the cascade (`setInputDesk`), the plain preamp gain stays at unity. Body crossfade from the earlier session is unchanged.
- `native/core/{include/trench/core/audition.hpp,src/audition.cpp}`: `set_target` takes an optional ramp length.
- `plugin/source/PluginProcessor.{h,cpp}`: INPUT knob drives the desk; project state embeds the 240 body bytes (`bodyBytes`) and recall recovers a missing body file into User Bodies (or installs the bytes if the library cannot list it); `bodyRecoveryDirectory` is injectable for tests.
- `plugin/source/dsp/Movement.h`: `authoredBars`/`authoredLengthChoice`; host-locked loops anchor to the song's bar grid (once-gestures and Restart still anchor where they are triggered).
- `plugin/source/ui/ModulationChip.h`: choosing a movement applies its authored length and preset playback; the chip reads `Name · length · once|loop`; the movement browser gains Length, Playback and Restart rows.
- `plugin/tools/Render.cpp`: `--output` now drives the OUTPUT parameter (it addressed the retired slam parameter).
- Tests: expectations updated for the authored length, once/loop text and grid anchoring; new PASS lines for grid anchoring, authored lengths, browser rows, encoded-domain round trip (worst 3.9e-16), type change keeping Morph/Q/KEY/INPUT/OUTPUT/movement phase, and missing-file recovery. The three drive-slam checks that encoded INPUT as plain gain now compare against the desk reference (recall drift 2.4e-5, tolerance 1e-4).

## Verified (headless)

TRENCH_Tests 275 PASS, 1 FAIL (ring leveller, see below). TRENCH_ReviewTests 30 PASS. TRENCH_SwitchSafetyTests 437 checks, 0 failures. TRENCH_UserBodyTests 14 PASS with `User Bodies\talking_hedz.body240`. KEY graph versus audio with KEY F# on the crisp body: worst 0.0017 dB. Engine cost with the encoded ramp: 0.0193 s per second of audio, KEY on 0.0197 s.

## Ring leveller check

`the ring leveller drops the peak by at least 6 dB` fails only because the roster now ships Flanger 3: its S3 is a +78 dB pole at 220 Hz fed through -68 dB of zeros in S1 and S2. Through the bare core runner the leveller cuts that tone by 54 dB (fresh or toggled live); through the processor via `TRENCH_Render --ring 1` the rms drops 30 dB. Inside the test harness it measures 0.41 dB and I did not find why. The leveller is off in the shipping path (`prepareToPlay`) and the user has said no levellers are wanted, so the check exercises a dormant feature. Threshold untouched.

## Not done

- Install: FL Studio (FL64.exe) still holds `C:\Program Files\Common Files\VST3\TRENCH.vst3`. Built module SHA256 `bcc9f7d4caabf7939e219c6380582bbf03ceeef26914e4e7e2cf263008234c7c`; installed is still `1076811f...`. Save the project, close FL, run `Install TRENCH.cmd`, reopen.
- Listening: nothing here was auditioned. Renders for the ear are in `output/distortion-roles-20260925`.
- KEY hysteresis: audio holds a note within 0.15 semitone of a scale boundary while the graph probe shows the nearest note; static agreement is proven, the hold band is not drawn.
