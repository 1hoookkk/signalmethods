# Mackie 8•Bus line-input SPICE candidate

This is a circuit-derived **simulation candidate**, not a hardware capture or a verified complete 8•Bus emulation. It follows channel 1's balanced line-input path to the insert send, with the mic input disconnected and FLIP selecting the mic/line preamp. EQ, low-cut, channel fader, mix bus and the rest of the desk are outside this capture route.

## Circuit provenance

The primary topology and passive values come from the [Mackie 8•Bus service drawings](https://audiocircuit.dk/downloads/mackie/Mackie-8BUS-mix-sch.pdf), printed sheets 08-1 and 09-1 (PDF pages 1 and 5). Component names in `mackie8bus.lib` retain those drawing references.

- Line input: R105/R106 7.32 kΩ, C105/C106 220 pF, C109/C110 47 µF, R114/R115 3.92 kΩ and D101–D104 rail clamps.
- Discrete gain stage: Q103/Q104 2SA1084 and Q101/Q102 MPSA06, R107/R108 510 Ω, R111/R112 2.49 kΩ, R109/R110 825 Ω, C107 470 µF and R113 22 Ω.
- Channel-board trim: R166 5.2 kΩ variable resistance with its wiper tied to one end; C135/C136 100 pF. Resistance is specified directly; no unverified knob-position mapping is applied.
- U1A differential/output stage: NJM4560, R116/R117 3.92 kΩ, C108 47 µF, R118 20 kΩ. Include the channel-board RF load R175 20 Ω / C138 2200 pF and insert-send R121 120 Ω.
- Supply: ideal ±16 V, as labeled on the preamp drawing. The bench supplies balanced signals through 50 Ω per leg and assumes a 10 kΩ external insert load.

## Semiconductor fidelity

**MPSA06:** uses the numerical SPICE model printed in the [legacy Fairchild datasheet, page 3](https://www.bcae1.com/repairbasicsforbcae1/images/a06.pdf). The downloaded manufacturer-authored PDF is in `research/MPSA06-legacy.pdf`. The [current onsemi document](https://www.onsemi.com/download/data-sheet/pdf/pzta06-d.pdf) supports the part's electrical specification but no longer contains that model block. A manufacturer model is still not a measurement of the transistors in one particular desk.

**2SA1084:** an explicitly estimated Gummel–Poon model, constrained by the [Renesas/Hitachi datasheet](https://www.renesas.com/en/document/dst/2sa1084-2sa1085-datasheet): gain range 250–800, nominal VBE around 0.6 V at 2 mA, fT around 90 MHz and Cob around 3.5 pF at 10 V. BF=450 is a selected value within the published range. Early voltage, series resistance, input capacitance, reverse gain and storage time remain assumptions. Both transistors are nominally matched. This is not an authenticated vendor SPICE model.

**NJM4560:** a behavioral dynamic approximation constrained by the [original NJR datasheet](https://dtsheet.com/doc/1304284/njm4560-data-sheet): 100 dB open-loop gain, 10 MHz gain-bandwidth, 4 V/µs slew rate and typical output swing near one volt inside the supply at modest loading. The model includes a dominant pole, slew limiting, rail limits and finite output resistance/current. Its 25 Ω output resistance, 40 mA limiting law and anti-windup behavior are estimates. It does not identify the actual chip's internal overload recovery, crossover behavior, common-mode failure modes or detailed load dependence. These uncertainties particularly matter for a distortion model.

**1N4148:** nominal estimated junction capacitance, storage time and forward curve; not a measured diode lot. Ideal passive values omit component tolerance, electrolytic ESR/leakage, temperature drift, aging, supply impedance/droop and random circuit noise.

Do not call this hardware-accurate merely because the netlist is derived from the real drawing. The next calibration evidence would be an actual desk's small-signal response and driven recordings at known gain and level.

## Execution and files

```powershell
& 'C:\Users\hooki\AppData\Local\Programs\Python\Python310\python.exe' tools/rtneural_workshop/spice/circuit.py --probe
& 'C:\Users\hooki\AppData\Local\Programs\Python\Python310\python.exe' tools/rtneural_workshop/spice/study.py --volts 4
& 'C:\Users\hooki\AppData\Local\Programs\Python\Python310\python.exe' tools/rtneural_workshop/spice/study.py --training
& 'C:\Users\hooki\AppData\Local\Programs\Python\Python310\python.exe' tools/rtneural_workshop/spice/study.py --after-filter
```

PySpice uses the already-installed ngspice 34 shared library. Each simulation saves its complete `.cir` deck and diagnostic log under `output/rtneural-workshop/spice8bus`. DC initialization uses a nodeset as a solver hint, not fixed voltages. The simulator must complete its entire requested time interval with finite output. The adaptive step is bounded at 1/(48 kHz × 4), then output is sampled at 192 kHz and low-pass decimated to 48 kHz. Input file timestamps are preserved; there is no cross-correlation shift or loudness matching. Internal phase and group delay remain part of the circuit response. A half-second zero prefix and 50 ms tail are simulated and cropped from delivery.

`*_volts.wav` stores physical simulated output volts as float samples. These are diagnostic files and can exceed digital full scale. Listening files explicitly map 16 V peak to one digital unit, then apply manual OUTPUT: -6 dB for stage-only and -30 dB when followed by Talking Hedz. The same gain is used for all drive variants within each comparison. Input levels are 0.1, 1 and 4 V per dry digital unit, all with trim at its maximum-gain resistance endpoint. The source phrase peaks at approximately 0.501 digital units, so its differential input peaks are approximately 0.050, 0.501 and 2.005 V.

The 40-second training pair uses 28 seconds of seeded varied-level tones/noise, six seconds of independently seeded validation signals and six seconds of held-out bass. It is rendered continuously at 4 V per input unit, 16 V per output unit, maximum trim, with no listening attenuation baked into the target. It matches the workshop's chronological 70/15/15 split. The final bass segment is not part of training or model selection. This preliminary stimulus is smaller than the official 190-second NAM calibration file, which is also available for later expansion.

`8Bus_SPICE_light_pushed_slammed.wav` concatenates the three stage-only drive levels with one-second gaps; starts are 0, 7.857 and 15.714 seconds. `8Bus_SPICE_before_then_after_filter.wav` compares the hard-driven circuit before and after Talking Hedz, using the same -30 dB final OUTPUT. The latter is a new SPICE render of the filtered input, not a rearrangement of an existing recorded pair.

## Checks completed before training

The nominal bench has near-zero output offset with stable bias around -9.007 V at the differential amplifier inputs. At 1 kHz, line gain is approximately 0.46 dB with 5.2 kΩ trim, 27.02 dB at 100 Ω, and 41.49 dB at the maximum-gain endpoint. Gain falls to about 39.57 dB at 20 Hz at maximum trim because the gain-setting capacitor is included.

At maximum trim, a 1 kHz input changing from 0.01 to 0.2 to 1 V peak produces output peaks around 1.19, 14.80 and 14.91 V. Harmonic measurements show the corresponding move from clean behavior into overload. These values validate behavior within this candidate, not agreement with a physical desk.

For the 0.2 V peak overload tone, halving the maximum transient step from 4× to 8× audio rate changed the decimated output by approximately -70.75 dB ESR. This checks numerical convergence on that tone; it is not an analog accuracy measurement. See `probe.json`, `tone_checks.json` and `time_step_check.json` beside the renders.

## First trained snapshot and listening files

Run `20260929-222453-0eff3e10` completed 3,000 optimizer steps with a 32-unit LSTM. The minimum combined validation objective selected step 2,901. MR spectral loss and LF ESR have weights 1:1; raw full-band sample error is diagnostic only. The model sees the original dry input. Optimizer input conditioning is folded into its exported weights, and no target normalization or automatic output compensation is applied.

| Check | Result |
| --- | --- |
| Selected validation objective | 0.910991 |
| Validation MR spectral / LF ESR | 0.786305 / 0.124686 |
| Held-out bass MR spectral / LF ESR | 2.478825 / 0.105347 |
| Held-out full-band ESR, diagnostic | -9.944 dB |
| Native RTNeural versus PyTorch RMSE | 7.89e-8 |
| Model peak on a one-second zero-input check | 0.000553 |

Export agreement passed. Reference matching is still incomplete: the bass spectral error remains significant, and the zero-input output is not exactly zero. Neither the circuit nor this first fit has hardware-fidelity or listening acceptance. The six-second bass segment was excluded from training and checkpoint selection; its dry validation prefix supplies recurrent context for inference.

`8Bus_SPICE_then_RTNeural_stage_only.wav` compares the held-out SPICE reference at 0 seconds with the native model at 7 seconds, separated by one second of silence. Both use manual OUTPUT -6 dB. `8Bus_SPICE_then_RTNeural_with_filter.wav` uses the same order with Talking Hedz following each stage and manual OUTPUT -24 dB. Both files are 13 seconds, mono 48 kHz float, finite and below digital full scale. Read-back samples were checked exactly, without gain matching. `neural_audition_manifest.json` records their hashes and gains.

The snapshot is `output/rtneural-workshop/runs/20260929-222453-0eff3e10/model_rtneural.json`; `training_report.json` and `result.json` in that directory contain the complete metrics and provenance. The workshop at <http://127.0.0.1:8896> has the completed run available for synchronized-position switching between Reference and RTNeural model.

## Fresh target and training continuation

`train_snapshot.py` renders a fresh 20-second pair as `input.wav` and `target.wav`, verifies the clipping profile, and continues the existing 32-unit snapshot for 3,000 steps. The 14-second training section includes independently seeded broad-band material and bass phrases at varied levels. The three-second validation section spans clean through hard overload. The last three seconds are held-out bass; they are not used for training or checkpoint selection. The simulation is continuous across the split boundaries.

```powershell
& 'C:\Users\hooki\AppData\Local\Programs\Python\Python310\python.exe' tools/rtneural_workshop/spice/train_snapshot.py output/rtneural-workshop/runs/NEW-8bus-run --steps 3000
```

Choose an unused run directory. The circuit is copied into the run, and the pair's hashes, voltage mapping and semiconductor assumptions are recorded in `target_provenance.json`. A completed pair with a passed clipping profile can continue through `--train-existing`; the file hashes must still match. Training preserves the starting checkpoint unless a lower combined validation objective is achieved.

The clipping sweep uses 50 Hz, 1 kHz and 10 kHz at 0.01, 0.05, 0.2 and 1 V differential input peaks. A separate 10 kHz overload probe records the raw op-amp and insert-send nodes. Supply-rail bounds apply to the raw op-amp node. They must not be imposed on the antialias-decimated WAV: removing high-frequency harmonics can raise its peaks above the analog waveform's original extrema. The float training target preserves those peaks. This is distinct from hidden output clipping or gain normalization.

`plot_clipping.py RUN_DIRECTORY` draws the measured simulated profile. The short listening comparison `8Bus_SPICE_then_RTNeural.wav` plays SPICE at 0 seconds and RTNeural at 4 seconds with identical manual OUTPUT -6 dB. None of these files establishes a physical-hardware match.
