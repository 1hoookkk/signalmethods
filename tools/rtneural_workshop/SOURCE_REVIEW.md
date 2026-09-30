# Mackie 8•Bus reference review — 2026-09-29

Capture-source follow-up on 2026-09-30: the user confirmed no physical desk or capture access and requested an 8•Bus source. The strongest located model-pack lead remains the creator's Mackie 32 8 pack; raw-pair availability and gain/route documentation are unconfirmed. A studio-owned 32×8 is also listed by Nashville Connection / Mark Dreyer Productions. See the [source findings and prepared capture request](8BUS_CAPTURE_REQUEST.md). No verified downloadable isolated 8•Bus hardware pair was obtained, and no purchase or outreach was performed.

## Current status

No verified isolated 8•Bus input/target pair is present. No 8•Bus hardware model has been trained or claimed as complete. The abandoned generic tube/SPICE teacher does not establish Mackie behavior. Further training on a wrong teacher can only fit that wrong teacher more closely.

A schematic-derived **line-input-to-insert SPICE candidate** now exists, with isolated drive renders, a continuous 40-second training pair and an exported 32-unit LSTM snapshot trained on that pair. See [its circuit mapping, assumptions, checks and first-fit results](spice/README.md). This is synthetic reference data, not a hardware recording; it does not change the hardware-verification status above.

## Evidence found

1. **Original Mackie service drawings**, [manufacturer document mirrored by AudioCircuit](https://audiocircuit.dk/downloads/mackie/Mackie-8BUS-mix-sch.pdf). Downloaded to `output/rtneural-workshop/research/Mackie-8BUS-schematics.pdf` (86 PDF pages). The first page is the Mic/Line Preamp Board 08 Rev S, drawing 990-008-00, sheet 1/2. It includes 2SA1084 / MPSA06 discrete transistors, NJM4560 op-amp circuitry and ±16 V preamp supply labels. Additional channel, output and supply sheets follow. Any SPICE model must reproduce the selected route, component dynamics, loading and supply behavior. Schematic possession alone does not validate saturation or guarantee sample alignment. A measured hardware reference remains the useful fidelity test.
2. **An actual 32•8 capture candidate**, [PastToFutureReverbs's own product page](https://pasttofuturereverbs.gumroad.com/l/hdiqg), titled “MACKIE 32 8 ANALOG CONSOLE PLUGIN EXTENSION FOR PROTEUS AND NAM! (CHANNEL & BUS) ALSO IRS!” The publisher lists channel and bus captures, Proteus/NAM models and IRs, at $15 when checked. It has not been purchased, downloaded, license-reviewed or fidelity-tested here. The workshop's Proteus snapshot importer gives a concrete route to audition and, if terms permit, distill such a model. An IR alone cannot represent input-dependent clipping. Pricing and contents may change.
3. **A free CR1604 listing**, [capture author's TONE3000 page](https://www.tone3000.com/tones/mackie-cr1604-1st-gen-55700), describes 62 models across driven settings. This is a first-generation CR1604, not the 8•Bus. Raw training WAV availability was not established. ToneHunt now redirects to TONE3000. Finding a NAM model does not imply its original synchronized audio pair is public.
4. **Mackity's actual target**, [Chris Johnson's original explanation](https://www.airwindows.com/mackity/), is a pre-VLZ Mackie 1202 input stage through its insert output. The native algorithm is useful as a musical comparison, but it is not an 8•Bus measurement.
5. **Official NAM stimulus**, linked by [NAM's GUI tutorial](https://github.com/sdatkinson/neural-amp-modeler/blob/main/docs/source/tutorials/gui.rst) to [the public input WAV](https://drive.google.com/file/d/1KbaS4oXXNEuh2aCPLwKrPdf5KFOjda8G/view). Downloaded to `output/rtneural-workshop/research/NAM-input.wav`: 48 kHz, mono, PCM24, 190 seconds. The current linked file is `input.wav`; it is not relabeled as the older `v1_1_1.wav` mentioned in the initial proposal. A recorded target still needs latency/alignment verification.
6. **Proteus import semantics**, [GuitarML's source](https://github.com/GuitarML/Proteus) and [RTNeuralLSTM.cpp](https://github.com/GuitarML/Proteus/blob/main/src/RTNeuralLSTM.cpp), establish the snapshot's LSTM/dense mapping and input residual. The workshop preserves that residual. Imported neural cores are not automatically equivalent to the whole Proteus plugin, which can include other processing. Its 44.1 kHz model convention must not silently be treated as 48 kHz.
7. **Academic paired data**, [Alec Wright's Automated Guitar Amp Modelling repository](https://github.com/Alec-Wright/Automated-GuitarAmpModelling), offers amplifier/effect examples for checking training methods. Those examples are not 8•Bus ground truth.

## Shortest path to an 8•Bus reference

Acquire an isolated 8•Bus capture with a clear route and drive setting, or a lawfully usable published capture with sufficient provenance. A clean desk recording, onset-of-overload recording and hard-input-overload recording would cover the user's requested behavior. Treat channel insert/direct output and mix-bus saturation as separate targets unless the complete combined route is explicitly the desired sound.

For a hardware capture, send the official stimulus and record the output on the same clock where possible. Document desk model/revision, mic or line input, balanced/unbalanced wiring, trim, pads, EQ, insert/direct/bus route, output fader, interface output reference and converter input range. Prevent interface ADC clipping by attenuation after the desk; do not reduce the intended desk input drive to solve output clipping. Record the attenuation/calibration so any later explicit scale correction is traceable. Keep raw recordings, including silence and calibration events.

Verify polarity, constant delay and any fractional delay/drift, then import the synchronized pair without automatic gain matching. Reserve separate musical recordings, including bass transients and silence, for final listening. The workshop's chronological split is a useful first check, not a substitute for those recordings. Training a single snapshot does not reconstruct every desk control.

The schematic candidate has passed internal DC, small-signal, overload and transient-step checks and now serves as an explicitly experimental training target. Its estimated semiconductor behavior still needs calibration against actual hardware overload; fitting it more closely cannot remove those reference uncertainties.

## Download provenance

| File | SHA-256 |
| --- | --- |
| NAM-input.wav | `70f8ec7f25686a1bd77f25973de8e51a6721e957e81eec121822e5e53366bc41` |
| Mackie-8BUS-schematics.pdf | `41f7cb51aba5009722c8aefbe6185d5e37fb0931bfcd2529a410c697ffe3cef3` |

The local WAV is linked for capture preparation, not redistributed as an original workshop composition. No purchase or community outreach was performed.
