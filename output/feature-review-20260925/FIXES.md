# Fixes for the 25 September feature review

Candidate: `out\build\vst3\...\TRENCH.vst3` SHA256 starting `1765b1aa`. Not yet installed at the time of writing (FL Studio held the DLL); the installed file is `C8DBDA99…` until `Install TRENCH.cmd` runs.

## Changes

1. **Movement feedback** (`PluginEditor.h/.cpp`): the shown Morph is a first-order follower toward the latest effective value on every frame (25 ms constant). The previous code restarted its interpolation from zero elapsed time on each audio update, so it stalled whenever audio updated faster than the 60 Hz timer. Test: follower advances monotonically to the target with updates every 4 ms; rendered check creates the real editor, runs Quarter Arc through the processor and reads the wheel's displayed position through the editor timer (span > 0.1).
2. **Exact recall** (`PluginProcessor.cpp`): recall compares the saved body bytes with the library bytes at the same id; when they differ the saved bytes are installed for playback and the library file is untouched. Test: state saved as `util_lp_12` with different bytes recalls the saved bytes; the library entry is unchanged afterwards.
3. **Movement selection** (`ModulationChip.h`): the browser groups movements under headings 1/4 bar … 4 bars by their authored length; one-shot transitions read "· lands". Choosing a movement applies its authored length and its authored playback; the face shows `Name` and the bars box. No rate, Once/Loop or Restart controls. Tests: Slow Tide listed under "4 bars", Long Return marked "lands"; four-bar one-shot completes exactly at four bars and holds; loops repeat on the bar grid.
4. **Help text** (`DeskKnob.h`, `KeySnapBox.h`): INPUT "level into the filter, N%"; OUTPUT "saturation after the filter, N%"; KEY "scroll to choose, click the offered key to lock, click again for OFF". Label OUTPUT kept (a rename to DRIVE is a user call).
5. **No Filter as colour**: switch-safety now covers Hedz ↔ No Filter both ways with KEY off/on (reference-bounded, lands on the new body); a drive check confirms OUTPUT saturates the dry signal with No Filter and stays under the ceiling.
6. **Ring leveller**: measured from fresh instances, off then on: Flanger 3 at 220 Hz drops 28.4 dB (0.7513 → 0.0287), so the ≥ 6 dB contract holds as written. Toggling the leveller on a cascade that is already running does not engage it in the bridge (0.7165) even though the core runner toggles live; the shipping path never toggles it (off in `prepareToPlay`, no control), so this is recorded as an open core question rather than changed.

## Automated results (headless, release)

TRENCH_Tests 280 PASS, 0 FAIL. TRENCH_SwitchSafetyTests 435 checks, 0 failures. TRENCH_ReviewTests 0 failures. TRENCH_UserBodyTests 0 failures. The dev-only calibration target was not built per instruction; its known inert `cal_grid` control remains.

## Observed UI behaviour

Rendered-feedback test only (wheel position through the editor timer). Not yet observed in FL Studio with this candidate.

## Measured audio

Switch-safety bounds every sample of a switch between the independent outgoing and incoming filters, including No Filter, at 44.1/48/96 kHz and 64/256/512 blocks. No listening.

## Human listening

None on this candidate.

## Remaining risks

- FL Studio behaviour of the follower and the grouped list is unverified until installed and watched.
- Whether OUTPUT should be relabelled DRIVE.
- Bridge live toggle of the ring leveller (dormant feature).
- KEY hold band (0.15 semitone) is not drawn on the graph.
