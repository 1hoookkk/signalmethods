# Gain staging (1 October 2026)

`input -> INPUT -> [8-Bus desk] -> six-section packed filter -> OUTPUT -> [8-Bus desk] -> host`

- **INPUT** (`preamp`, -24 to +24 dB, default 0): level into the filter. Above 0 dB it also drives an
  8-Bus desk that sits before the filter.
- **Filter**: the body's six sections, Morph x Q word interpolation, Rossum modified Direct Form II.
- **OUTPUT** (`output`, -24 to +24 dB, default 0): level after the filter. Above 0 dB it also drives an
  8-Bus desk that sits after the filter. Nothing clean follows that desk.
- **At 0 dB and below a knob is plain gain** and its desk is not run at all.
- **Onset**: above 0 dB a seat outputs `dry + blend * (desk - dry)`. `blend` is a smoothstep of the
  knob from 0 at 0 dB to 1 at +12 dB, so the desk's colour eases in and the seat is fully the desk
  from +12 dB up. Crossing 0 dB resets that seat's model and fades it in over 50 ms.
- **Calibration**: the model sees 0.025 units per 1.0 full scale after the knob's gain, so +24 dB
  puts a 0 dBFS signal at 0.4 units, 6 dB past the model's 0.2-unit knee. Small-signal desk gain is
  padded to unity once, at model load.
- **No AGC, makeup, loudness compensation or limiter.** A driven seat gets louder until the model's
  rail: a -6 dBFS tone rises about 13 dB at +24 dB.
- **Safety**: non-finite samples become 0.

Before and after the filter are different sounds. The INPUT seat clips first and the filter then
carves the result, so resonances stay tall. The OUTPUT seat receives the filter's resonant peak and
flattens it.

## The desk model

`assets/trench_8bus_main.json`, an RTNeural 32-unit LSTM with a dense output, run at 48 kHz and
resampled at other host rates. It was trained on a schematic simulation of a Mackie 8-Bus line path
(`trench_8bus_main.provenance.json`). It is not a capture of hardware and no recording validates it.

The model is not clean at low drive, where the schematic is. Fully in circuit with no drive it adds
about 2 % harmonics to a -6 dBFS tone and lifts 5 kHz by about 4.5 dB. The onset blend exists to keep
that out of the first few dB. Retraining with the loss weighted per level would address the cause.

Measured through No Filter on a 375 Hz sine at 0.5 amplitude, either knob, the other at 0 dB:

| Knob | Harmonic ratio | Settled peak |
| ---: | ---: | ---: |
| 0 dB | 0.000 | 0.50 |
| +1 dB | 0.001 | 0.56 |
| +3 dB | 0.018 | 0.71 |
| +6 dB | 0.050 | 0.97 |
| +12 dB | 0.112 | 1.49 |
| +18 dB | 0.196 | 1.98 |
| +24 dB | 0.362 | 2.41 |

At 44.1 and 96 kHz an engaged seat is 11 and 22 samples late against the bypassed path, and the host
is not told. Its dry and desk signals pass through the same resamplers, so they stay aligned with
each other, but both lose about 5 dB above 16 kHz at 44.1 kHz. That loss arrives when a knob crosses
0 dB. At 48 kHz there is no step.

## Migration from earlier projects

`preamp` is INPUT again and its saved value is recalled. `inputSlam` (the old SLAM switch), `desk`
(a knob that existed for part of 1 October) and `slamDrive` are discarded on recall.

Audio is not identical to earlier builds. INPUT above 0 dB now drives the desk before the filter
without a switch, with the gradual onset. OUTPUT above 0 dB drives the desk after the filter as it
did, but now also with the gradual onset over the first 12 dB.

`DriveSlamTests.h` covers the chain. None of this has been judged by ear yet.
