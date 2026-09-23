# Gain staging (release/v1)

`INPUT -> six-section filter -> soft clip -> OUTPUT (desk drive + inflator) -> safety clamp`

- **INPUT** (`preamp`): clean gain into the filter, unity to +20 dB (`preampGain`, 5 ms smoothing).
- **Filter**: the body's six sections, Morph x Q word interpolation at the 44.1 kHz datum
  (a 65 x 17 grid of re-warped cascades at other host rates). No saturation inside the sections.
- **Soft clip** (`softGuard`): linear to half the ceiling (about -6.1 dBFS), quadratic to a
  -0.1 dBFS ceiling, each channel, no attack or release. It catches pole crossings and resonant
  peaks after the filter. Its activity drives the clip meter.
- **OUTPUT** (`output`): 0 bypasses the stage exactly. Above 0 it sets the Mackity desk's input
  gain (1 + 9 x knob) with its static compensation, then the inflator curve (`Inflator.h`,
  source in `Inflator-SOURCE.md`) with Effect equal to the knob. The inflator is normalised to
  the ceiling, so it cannot raise the peak.
- **Safety clamp**: non-finite samples become 0; the release build clamps to the ceiling.

Retired: the Distortion parameter (dropped on project recall), per-section saturation, AGC,
output trim and the separate SLAM control. The Dev build's guard settings shape the soft clip.

`DriveSlamTests.h` covers the chain: the soft clip on hot and quiet input, INPUT's +20 dB,
OUTPUT changing audio while staying under the ceiling and surviving recall, the inflator being
exact at zero effect, monotone and ceiling-bounded, and old Distortion values being discarded.
None of this has been judged by ear yet.
