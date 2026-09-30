# 8Bus main-mix extension

This study extends the accepted line-input candidate through a selected channel and the main left output. It is a schematic simulation with estimated semiconductors, not a hardware capture. The accepted `20260930-0535-8bus-schematic` checkpoint and circuit are preserved.

## Selected route

Balanced line input -> insert normal connection -> channel fader -> hard-left pan -> L/R assignment -> main left summing amplifier -> main insert normal connection -> main master -> balanced XLR output.

EQ and low-cut are bypassed. This is one mono channel routed to one side of the main mix. It does not include a subgroup in series, the whole 32-channel console, or a second preamp. The output is the differential voltage between XLR pins 2 and 3.

The primary reference is the Mackie service drawing PDF at `output/rtneural-workshop/research/Mackie-8BUS-schematics.pdf`, downloaded from <https://audiocircuit.dk/downloads/mackie/Mackie-8BUS-mix-sch.pdf>.

| Portion | Drawing | Implemented components |
| --- | --- | --- |
| Accepted line preamp | 08-1 / 09-1, PDF pages 1 / 5 | Existing `mackie8bus.lib`, unchanged |
| Channel fader and left pan | 09-2, PDF page 6 | R226 10k pot, C232/C221 47u, R274 15k, U202B 4560, R228 1.1k / R227 510 / C226 220p, R229 100k, R231 dual 50k pot at hard left, R232 36k, U203A follower, C215 47u, L assignment R242 5.1k |
| Main summing and insert | 10-7, PDF page 21 | U57A 2068, R218 2.7k / C85 220p, R607 20 / C341 220p, R242 120 / C108 47u, R305 20k, C86 47u |
| Main master | 10-7 | R278A 10k pot, C292 47u, R578 15k, U33A 2068, R219 2.4k / R220 1.1k / C97 100p |
| Balanced main driver | 10-7 | U35A 4560 inverter, R250/R221 5.1k, C100 20p; U35B 4560 follower; C98/C208 47u, R246/R509 120, R508/R579 20k |
| Unbalanced parallel branch | 10-7 | U34A 4560 inverter, R226 5.1k / R225 10k / C89 20p, C90 47u / R227 120 / R236 20k |

References such as R242 repeat on different boards and live inside their corresponding SPICE subcircuits. The four unused odd subgroup assignments terminate their 5.1k feed resistors to ground, so R234/R236/R238/R240 remain as loads on the left pan output. The main balanced negative leg follows U33A directly; it does not follow the separate unbalanced inverter. The two balanced legs therefore oppose each other.

Preamp and output-board supplies are ideal +/-16 V. The channel fader/pan supply is +/-18 V, as drawn on 09-2. Each XLR receiver leg is loaded by 10k to ground. The unbalanced output has its internal 20k load but no external receiver.

## Fixed settings and levels

Line trim is maximum, at 0 ohms. Channel fader electrical wiper fraction is 0.366: this is a fixed resistor division chosen to approximately offset its amplifier's 3.157x gain. It is not a measured knob-travel calibration. Main master is at its upper electrical endpoint, so its amplifier adds approximately 10 dB. These are physical gain-stage settings, not automatic output normalization.

Input uses the same 4 V per digital unit as the accepted preamp. Differential output uses 16 V per digital unit. Balanced output can approach twice a single op-amp's swing; analog rail checks apply to each raw amplifier output, not the differential or decimated waveform. Listening copies receive fixed manual OUTPUT -12 dB after the whole chain. The preamp-only comparison receives the same gain, so the bus gain remains audible.

## Semiconductor assumptions

The 2068 estimate uses the [manufacturer datasheet](https://www.nisshinbo-microdevices.co.jp/en/pdf/datasheet/NJM2068_E.pdf): 120 dB typical open-loop gain, 300k input resistance, 6 V/us slew and typical 1.5 V output headroom. The one-pole estimate uses the specified 5.5 MHz unity-gain frequency. It does not reproduce the complete open-loop response: the datasheet separately gives gain-bandwidth products of 27 MHz at 10 kHz and 19 MHz at 100 kHz. Treating these three figures as the same bandwidth would be incorrect.

The output-current law, output resistance, anti-windup, overload recovery, common-mode behaviour and device matching remain estimated. The vendor's SPICE downloads require account access; no vendor 2068 model is claimed. The existing 4560 and transistor limitations also remain. Parameter sensitivity explores alternate 2068 bandwidth, slew and headroom assumptions without selecting the most attractive sound as evidence of accuracy.

Omitted: residual loading of bypassed EQ/low-cut branches, meter/monitor branches, insert-detection circuits, other channels, stereo crosstalk, supply impedance/sag, component tolerances/aging, random noise and temperature effects. These omissions prevent a whole-console or 1:1 claim.

## Reproduction

Run `bus_output.py OUTPUT_FOLDER` for AC and nonlinear checks. `train_bus.py --verify` checks 4x/8x transient convergence and semiconductor sensitivity in `output/rtneural-workshop/bus-output-20260930`.

`train_bus.py NEW_RUN --steps 6000 --workers 4` generates twenty independent three-second captures and trains one 32-unit LSTM covering the selected path end to end. Four isolated ngspice processes render different captures; they do not split a continuous circuit simulation. Completed captures are cached with input, circuit and audio hashes. The resulting 60-second pair splits at exact sample boundaries into 42 seconds training, 9 seconds validation, and 9 seconds held out. Training includes independently seeded bass, broad-band material, filtered bass, percussion, overload bursts and silence. Validation has independent seeds and a silence section. Final evaluation contains three seconds of bass already processed by Talking Hedz, three seconds of independent percussion, two seconds of independent bass, and one second of recovery. Those targets are not used for optimization or checkpoint selection. The accepted input-stage checkpoint initializes training, but remains a separate preserved file.

Each capture starts from the circuit operating point and half a second of zero input. Training and evaluation reset recurrent state at those capture boundaries. The first 12,000 audio samples of each capture supply recurrent context and are excluded from scoring; training blocks cannot cross into another capture. The exported model processes a continuous stream normally: the three-second reset rule belongs to this dataset, not to product audio processing. Longer continuous accuracy requires a continuous reference.

Captures use ngspice's INTERP output storage at 192 kHz. The internal adaptive solver and maximum timestep remain unchanged, and a separate comparison must establish agreement with storing all adaptive output points and interpolating offline. Raw amplifier rail checks retain every adaptive sample on the short diagnostic render. Interpolated output extrema are not represented as raw analog rail evidence.

The installed ngspice 34 library retains an internal interpolation flag across circuit loads; removing the option does not reset it. The workshop rejects an adaptive transient requested after a uniform-storage run and requires a fresh process. A regression test exercises that guard. Sensitivity renders use uniform storage explicitly; the first run's metadata was corrected after reproducing this library behavior, with original metadata retained. Its audio comparison is unchanged. The earlier rail profile and the long training render used their intended storage modes.

The objective remains MR spectral loss + LF ESR, equal weights. Native RTNeural/PyTorch parity, finite output, explicit levels and WAV readback are checked. Run history exposes the resulting reference and model in the workshop. No product plugin is changed by these scripts.

`train_bus.py NEW_RUN --refine-from COMPLETED_RUN --steps 12000 --learning-rate 0.0002` preserves a completed run and starts a refinement from a copied checkpoint. It reuses the verified capture files and retains the parent-run provenance. The starting checkpoint is scored and remains eligible, so the refinement cannot select a worse combined validation loss. The decision to refine uses validation progress; held-out results do not determine training duration or checkpoint selection.

The audition path applies OUTPUT only after processing. A regression test compares a native nonlinear model driven by the original held-out input against the workshop WAV, catching an earlier in-place baseline scaling bug that also attenuated model input when the filter was disabled. The main-output study's `--finish` command rebuilds all comparisons from the original held-out samples and updates the displayed audio levels.

## Circuit verification, 30 September 2026

The loaded-path profile in `output/rtneural-workshop/bus-output-20260930/checks-loaded` passed its six checks: small-signal gain, main-stage overload, gain compression, individual amplifier rail limits and opposing balanced output legs. Bounds use all adaptive solver samples. The harmonic measurement fits sine/cosine terms and DC together, avoiding FFT-bin errors on fractional-cycle windows.

On a clipped 10 kHz input, 4x versus 8x rendering produced error power / reference power of 0.00004294 (-43.67 dB), below the predeclared 0.001 limit. Uniform output storage versus adaptive storage followed by offline interpolation differed by 2.23e-14 on the same probe. These checks justify the long-render settings; they do not establish hardware accuracy.

The 2068 sensitivity probe combined 55 Hz, 1 kHz and 7 kHz. Reducing slew and headroom changed the output by ESR 0.002753 and reached 25.43 V differential peak; the alternate higher-bandwidth/higher-headroom estimate gave ESR 0.0002086 and 29.24 V peak. These alternatives are uncertainty probes, not measured device bounds or candidates selected for their sound. The nominal datasheet-based estimate remains the training reference.

The first 60-second transient stopped at 35.25 seconds of simulated time with a timestep-convergence error at the channel fader feedback node. No target WAV or trained model was accepted from that incomplete run. The one-second source excerpt around that event completed in isolation. A larger iteration budget, explicit sample-clock breakpoints, and a 1 mV rail regularization did not establish a reliable long render. Native PWL completed a later-time diagnostic and agreed with the short reference probes, but its source lookup was much slower. These alternatives are retained as diagnostics and are not the selected reference.

The selected reference keeps the original unsmoothed circuit, original filesource, iteration limit 10 and error tolerances. Independent short captures avoid relying on the failing long simulation. `--render-existing --render-only --workers 4` resumes missing captures; `--train-existing --steps 6000` trains the completed pair. A failed transient preserves partial vectors as diagnostics and still raises an error. Partial audio is never accepted as a training capture.

All twenty selected captures completed, and `pair_validation.json` verifies their exact order, split boundaries, circuit and audio hashes, settings and target assembly. The completed target SHA-256 is `2e0648aecc55d254684a37328018fe5034515f93fc8c2d0e7e713ee85ec4b196`. The first 6,000-step fit reduced combined validation loss from 3.0898 to 1.8101; a separate refinement was then started because validation was still improving at the end. Final neural and listening results are recorded separately from circuit verification.

## GPU training and model audit

The machine's RTX 4060 Laptop GPU has 8 GiB of VRAM. An isolated environment at `output/rtneural-workshop/.venv-cuda` contains PyTorch 2.9.1+cu128; the global CPU environment is preserved. Training uses CUDA float32, including the spectral and LF objective, with TF32 and mixed precision disabled. Checkpoints move to CPU before saving and export stays compatible with native RTNeural. The same 64-unit, batch-eight training step measured 0.1600 seconds on CPU and 0.01622 seconds on CUDA. Batch 32 measured 0.01688 seconds with 2.6 GiB peak allocated memory. These are optimizer-step benchmarks, excluding validation, recurrent context and export.

The first GPU run, `20260930-1429-8bus-main-gpu`, completed 6,000 steps in about 130 seconds. It used a 64-unit LSTM widened from the 32-unit model without changing the starting response. Its best combined validation objective was 1.46050 (MR spectral 1.18726, LF ESR 0.27324), versus 1.81006 for the first full-path fit. A subsequent lower-learning-rate pass did not improve validation, so its starting checkpoint was retained. GPU/PyTorch-to-native export RMS disagreement was 2.77e-6, below the unchanged 1e-4 acceptance limit.

The audit of that GPU model found material errors despite the validation improvement. At a 1 kHz, 0.05 V peak line input, its gain was 3.18 dB below the reference and it generated substantial harmonics while the simulated circuit was still clean. At heavier drive it overshot the simulated clipping plateaus. The three musical held-out sections had full-band diagnostic ESR from 0.18 to 0.23; recovery after the bass stopped had ESR 0.996. Much of the circuit's recovery tail was missing. This is an experimental approximation, not a validated replacement for the circuit or a hardware match.

Zero-input and continuous-drive stability checks remained finite. The zero-input floor settled near -67.5 dBFS before manual OUTPUT. A standalone mono 64-unit render took roughly half the audio duration on this machine. A separate xsimd AVX2 runner agreed numerically but did not improve runtime in the measured comparison; the default runner is retained. These timings do not establish plugin-host CPU or latency suitability.

The optional `--random-record-context` experiment samples different positions inside the training captures and refreshes context every eight backpropagation blocks. It primes the complete preceding record state under the current weights, sharing dense recurrent computation for repeated captures in a batch. No context crosses a capture boundary. CPU and CUDA regression tests compare the batched hidden and cell states against independent exact-prefix evaluation, including empty and unequal prefixes. Validation and checkpoint selection are unchanged. Previously inspected held-out diagnostics are development evidence, not a fresh blind acceptance set for later experiments.

The 3,000-step shared-context trial `20260930-1454-8bus-main-shared-context` completed without improving the starting validation objective. The earlier `20260930-1429-8bus-main-gpu` model therefore remains selected; its audit still applies. Twenty workshop tests passed after the context implementation changed. More iterations alone have not resolved the onset/recovery mismatch. The next modeling study needs explicit slow-state representation and training coverage around the clipping threshold, followed by fresh independent reference material. Hardware captures remain necessary to test the estimated circuit against an actual 8Bus.

The current selection and all candidate scores are recorded in `output/rtneural-workshop/bus-output-20260930/model_selection.json`. Each completed run includes `training_report.json`, `auditions.json`, the RTNeural JSON export and fixed-gain listening files. `8Bus_bass_SPICE_then_RTNeural.wav` plays three seconds of the circuit render, a one-second gap, then three seconds of the model. Both receive only manual OUTPUT -12 dB. `8Bus_bass_preamp_then_main.wav` compares the preserved input-stage model with the new main-output model at that same manual output setting. No listening acceptance or release approval is claimed.
