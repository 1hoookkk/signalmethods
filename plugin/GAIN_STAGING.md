# DRIVE and soft clipping

Current product decision, 2026-09-21: remove automatic gain control and let the filter output reach a soft clip.

`DRIVE -> filter -> Output saturation (legacy host parameter, default off) -> final soft clip`

- **DRIVE** sets clean input gain, from unity to +20 dB. Its existing taper is retained, with 5 ms smoothing. It changes how hard the signal excites the filter and any enabled filter nonlinearity. The former input desk saturation and coupling filters are removed from this path.
- **Filter** retains its body, Morph/Q, coefficient interpolation and optional Distortion radius expansion. Its output is no longer attenuated by an envelope or table gain controller. Feedback clipping remains a separate Dev experiment and is off by default.
- **Output saturation** (formerly the main SLAM knob) overdrives the Mackity-derived output desk after the filter. It is retained as an advanced host parameter for existing projects and automation, and defaults to zero. It is not shown on the minimal main face. Its existing drive taper, static compensation, and `slamDrive` host parameter ID remain. Zero bypasses that desk. The compensation depends only on the control, never on a detector or recent audio.
- **Final soft clip** has a linear region to half the ceiling (about -6.12 dBFS), a quadratic transition, and a ceiling of -0.1 dBFS. It acts independently on each channel and has no attack, release or gain recovery. Clip activity now counts the same linear threshold that the audio uses.

No Filter with DRIVE and Output saturation at zero, remains sample-exact unity, including values above unity. Raising DRIVE or Output saturation enables the soft clip. DRIVE's return to zero keeps processing active while its gain ramp settles.

AGC is removed from both shipping and Dev processing, including its controls, meters and render options. Retired Dev AGC values are discarded on project restore. Remaining host parameter IDs are unchanged. DRIVE's sound intentionally changes because its former pre-filter desk is gone; old DRIVE automation retains its positions, not its old sound.

The internal ring limiter remains an explicitly enabled Dev experiment; it is off in the normal plugin and by default in the renderer. The standalone shared filter engine is unchanged by this work.

The main face exposes DRIVE only. The added OUTPUT trim is removed from the UI, parameter list and signal path; interim saved `outputTrim` values are discarded. The soft clipper is the final stage.

## Verification and listening

`DriveSlamTests.h` exercises the shipping and Dev paths. It checks unattenuated overload in the bridge, immediate return to quiet input, channel independence, the DRIVE gain endpoint, and processor soft clipping with SLAM at zero. Calibration tests retain interpolation, persistence, reset and control reachability checks.

`TRENCH_CalibrationTests --audition <absolute-directory>` renders the real Dev processor with the default soft clip, no normalization and the same generated source across five settings for each of Talking Hedz and Megasweepz: neutral, DRIVE halfway, DRIVE full, SLAM alone, and both. The source is a pulsed 110 Hz tone plus third harmonic; Morph traverses the body over four seconds at Q 0.8, 48 kHz stereo. These are comparisons for listening, not a perceptually fitted control law or proof of acceptable aliasing.

The roles are implemented; the preferred amount and taper still need human listening on intended material.
