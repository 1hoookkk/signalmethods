# Gain staging (release/v1)

`INPUT -> six-section filter (linear) -> OUTPUT (Mackity) -> zero-latency clip at -0.1 dBFS`

- **INPUT** (`preamp`): clean gain into the filter, unity to +20 dB (`preampGain`, 5 ms smoothing).
- **Filter**: the body's six sections, Morph x Q word interpolation at the 44.1 kHz datum
  (a 65 x 17 grid of re-warped cascades at other host rates). No saturation inside the sections.
- **Soft clip** (`softGuard`): linear to half the ceiling (about -6.1 dBFS), quadratic to a
  -0.1 dBFS ceiling, each channel, no attack or release. It catches pole crossings and resonant
  peaks after the filter. Its activity drives the clip meter.
- **OUTPUT** (`output`): 0 bypasses the stage exactly. Above 0 it is the Airwindows Mackity
  process path (`DeskDrive.h`, source in `DeskDrive-SOURCE.md`) with its two controls tied to the
  knob: In Trim rises to +18 dB at full and Out Pad takes back 40 % of it. On the D Rich loop
  (hot, +4 dBFS peak) full OUTPUT costs about 3 dB; on -14 dBFS rms material it adds about 4 dB.
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
