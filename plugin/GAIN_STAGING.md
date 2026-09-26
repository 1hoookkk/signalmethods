# Gain staging (release/v1)

`INPUT (level) -> six-section filter (linear) -> Mackity -> OUTPUT (level) -> zero-latency clip at -0.1 dBFS`

- **INPUT** (`preamp`): level into the filter, -24 to +24 dB, 0 dB default, 5 ms smoothing. It sets how hard
  the filter output hits Mackity.
- **Filter**: the body's six sections, Morph x Q word interpolation at the 44.1 kHz datum
  (a 65 x 17 grid of re-warped cascades at other host rates). No saturation inside the sections.
- **Soft clip** (`softGuard`): linear to half the ceiling (about -6.1 dBFS), quadratic to a
  -0.1 dBFS ceiling, each channel, no attack or release. It catches pole crossings and resonant
  peaks after the filter. Its activity drives the clip meter.
- **Mackity** (`DeskDrive.h`, source in `DeskDrive-SOURCE.md`): always in, at its own unity setting
  (In Trim 1, Out Pad 1). Its rails are the saturation; INPUT decides how hard they are hit.
- **OUTPUT** (`output`): level after Mackity, -24 to +24 dB, 0 dB default, 5 ms smoothing.
- **No dynamic limiter.** The X3 CStereoLimiter (-4 dBFS, slow release) was tried on 26 Sep and
  removed: on a hot loop through Talking Hedz it held the mix 12 dB down (copy in
  output/instrument-20260925/parked).
- **Safety clamp**: non-finite samples become 0; the release build clamps to the ceiling.

Retired: the Distortion parameter (dropped on project recall), per-section saturation, AGC,
output trim and the separate SLAM control. The Dev build's guard settings shape the soft clip.

`DriveSlamTests.h` covers the chain: the soft clip on hot and quiet input, INPUT's +20 dB,
OUTPUT changing audio while staying under the ceiling and surviving recall, the Mackity In Trim
law and its full-scale cap, and old Distortion values being discarded.
None of this has been judged by ear yet.
