# TRENCH Dev calibration

Updated 2026-09-21. The current gain-stage contract is in [GAIN_STAGING.md](GAIN_STAGING.md).

The shipping and Dev path is DRIVE (clean gain) -> filter -> SLAM (output desk) -> final soft clip. AGC has been removed, including its Dev parameters, meters and position control. Saved AGC values are discarded on restore. The final soft clip has no detector or recovery; its default linear fraction is 0.5 and ceiling is -0.1 dBFS.

Fresh Dev instances use WORDS, an 88-sample control interval, kernel ramp, output desk saturation/compensation and the final clip. Feedback clipping, section saturation and the ring limiter start off. DRIVE, SLAM and Distortion start at zero. No Filter with neutral DRIVE/SLAM remains exact unity. Restored sessions retain their remaining saved settings.

The panel displays feedback clipping and its ceiling. Other experimental parameters remain host-visible. DRIVE now supplies up to +20 dB of clean gain with 5 ms smoothing; its former input desk is gone. SLAM retains the output desk and static compensation. The separate Distortion parameter controls radius expansion and is disabled while Dev feedback clipping is enabled.

## Remaining variables

- Representation: WORDS/GRID, control interval (1-128 samples), kernel ramp, Morph smoothing (0-100 ms).
- Input trim: -24 to +24 dB, separate from DRIVE and final monitoring level.
- Feedback experiment: on/off and ceiling (-36 to +24 dBFS; default +6).
- Section saturation experiment: on/off and threshold (-26 to 0 dBFS).
- Ring experiment: on/off, gain allowance, attack, release and floor. It remains disabled in the normal plugin.
- Output desk: on/off, saturation, static compensation and output coupling.
- Monitor trim: -36 to +6 dB before the final clip.
- Final clip: on/off, linear fraction (0.10-0.99) and ceiling (-12 to -0.1 dBFS).

GRID compares an existing 65 x 17 coefficient grid with packed-word targets. Neither establishes hardware equivalence. The graph is a linear coefficient response, not a distortion spectrum. Optional section saturation uses the runtime section array, including its padded identity stage. There is no oversampling in this harness.

## Audition and recall

STORE A/B captures parameters against the current body, host rate/block size and recorded loop. HEAR A/B recalls those parameters while retaining filter history; it does not restart transport or normalize loudness. Changed context is refused. EXPORT SESSION writes XML under Documents/TRENCH Calibration with parameter values, body bytes, loop, notes and live block measurements; source audio is not embedded.

RESET TO DEFAULTS restores Dev parameters. RESET TO NAKED additionally disables the processing switches, zeros DRIVE/SLAM, disables Movement and key snap, stops the recorded loop and clears the MIDI note latch. Body, Morph/Q, recorded loop data and archive remain. Neither reset clears the filter's history.

Peak pre/post desk and input/output RMS are live block measurements. The clip percentage counts samples outside the actual linear region. Settings received acknowledges audio-thread delivery, not audible engagement. Waiting for audio processing means no new processed blocks have arrived for a second.

## Validation

TRENCH_CalibrationTests covers packed targets at 44.1/48/96 kHz, parameter persistence, controls, reset and the clip. The shared DRIVE/SLAM acceptance checks exercise no AGC gain riding, immediate quiet-signal recovery, channel independence, clean DRIVE gain and final clipping in both builds.

`TRENCH_CalibrationTests --audition <absolute-directory>` now renders five DRIVE/SLAM combinations for Talking Hedz and Megasweepz through the real processor. See GAIN_STAGING.md for source and settings. Previous AGC audition code and this document's prior version were preserved under output/no-agc-20260921-174754 before editing. Historical WAVs are unchanged.

Builds and measurements do not establish listening quality, DAW acceptance or acceptable aliasing. The control roles are implemented; their final voicing remains a listening decision.
