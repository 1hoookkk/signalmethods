# RTNeural workshop

Local capture import, LSTM training, native RTNeural export and A/B listening for the Mackie 8•Bus study. An authentic, isolated 8•Bus reference has **not** been imported yet. The included Mackity reference is the pre-VLZ 1202-style algorithm in `plugin/source/dsp/DeskDrive.h`.

## Start

From the repository root on this machine:

```powershell
& tools/rtneural_workshop/build.cmd
& 'C:\Users\hooki\AppData\Local\Programs\Python\Python310\python.exe' tools/rtneural_workshop/serve.py
```

Open <http://127.0.0.1:8896>. The server binds only to loopback. Close its terminal to stop it. Runs, imports and audio are under `output/rtneural-workshop`; the installed plugin is not changed. Python dependencies are PyTorch, NumPy, SciPy and SoundFile. The build uses the existing RTNeural checkout and MSVC 2022 Build Tools. Filter auditions additionally need the existing `TRENCH_Render` release executable.

This machine also has a CUDA environment at `output/rtneural-workshop/.venv-cuda`, using PyTorch 2.9.1+cu128 from the [official PyTorch package index](https://pytorch.org/get-started/previous-versions/). Use that environment's `Scripts/python.exe` for GPU training. The global CPU installation is preserved. Training selects CUDA automatically when available; scripted studies can require it with `--device cuda`. Loss, model and training tensors use the same device, with full float32 precision and TF32 disabled. Checkpoints are saved on CPU and JSON exports remain portable to native RTNeural. GPU/CPU loss agreement and GPU-to-native export have regression tests. The existing workshop server can display these completed runs; future UI training uses the Python environment that launched that server.

`benchmark_training.py` measures the recurrent model, spectral/LF objective and Adam steps at different batch sizes. On this machine the 64-unit model measured approximately 0.160 seconds per step on CPU at batch 8, 0.0162 seconds on CUDA at batch 8, and 0.0169 seconds on CUDA at batch 32. The last configuration used about 2.6 GiB peak allocated memory. These short benchmarks exclude periodic validation and export; they are not whole-run speed guarantees. Reports live under `output/rtneural-workshop/gpu-benchmark-20260930`.

## References

- **Recorded pair:** two mono 48 kHz WAVs, 12 seconds to 10 minutes, each at most 50 MB. Keep the original signal level. Positive delay means the target arrives later; the importer crops the leading target samples and common tail. It does not estimate delay, compensate fractional delay, fix clock drift, normalize or denoise. Inspect alignment before training. Capture notes should identify the desk, route, controls and interface calibration. An imported file does not independently verify hardware identity.
- **Imported model:** mono LSTM followed by a linear dense output in RTNeural JSON, or a single-input Proteus snapshot. Select the original model rate before import. Proteus's input residual is preserved. The teacher runs at the selected rate with polyphase resampling around it. This imports the neural core, not Proteus's entire plugin processing chain. Conditioned multi-input models, NAM `.nam` architectures, GRUs and arbitrary JSON formats are not currently accepted. Proteus JSON is useful if a capture package provides it alongside NAM.
- **Mackity comparison:** executes the repository's actual `DeskDrive` algorithm with explicit 0–60 dB input drive and output pad 1.0. This is an algorithm reference for experimentation and pipeline verification, not evidence of 8•Bus fidelity. Models are fixed snapshots; changing the drive requires retraining.

Recorded pairs split chronologically into 70% training, 15% validation and 15% held-out audio. No sample appears in two splits. Short split boundaries are not independent hardware takes; the first 12,000 samples of validation and held-out scoring are excluded for recurrent warmup. For stronger acceptance, acquire separate musical takes covering clean, onset and severe overload. A single fixed capture does not identify every trim setting or route.

For held-out inference on a recorded pair, the dry validation segment is supplied as preceding recurrent context and then discarded. Its target is not used to calculate held-out error. This avoids resetting the model to silence immediately before a continuously recorded test segment.

The scripted [8Bus main-output study](spice/BUS_OUTPUT.md) uses independent three-second circuit captures. Its explicit `record_samples` configuration resets training and evaluation state at each capture boundary, excludes the first 12,000 samples of every capture from scoring, and prevents training blocks from crossing boundaries. Ordinary continuous pair imports retain the preceding-context behavior above. The exported neural network does not force periodic resets during playback.

Algorithm/model teachers use a 36-second seeded training stimulus, an independently seeded 10-second validation stimulus, and the selected audition WAV as held-out material. The default bass phrase is not used for training. Stereo audition files are explicitly averaged to mono; training pair uploads reject stereo.

## Objective

Training minimizes **MR spectral loss + LF ESR**, with weights **1:1**. The same combined validation objective chooses the best checkpoint. Full-band sample ESR is diagnostic only.

MR spectral loss averages three Hann STFT resolutions: 256, 1024 and 4096 samples, each with hop length FFT/4. Each resolution adds spectral convergence (magnitude-error norm / target-magnitude norm) and mean absolute log-magnitude difference. Spectral norm denominators have a floor corresponding to -60 dBFS RMS, scaled for the FFT size, window energy and frame count. Log magnitudes have a `1e-5` floor.

LF ESR is complex FFT error power / target power over bins from DC through 300 Hz on Hann-windowed 4096-sample blocks. Complex error retains low-frequency phase information. FFTs use window-energy normalization and one-sided Parseval weights; target band power has a `1e-6` floor, corresponding to -60 dBFS RMS. This avoids near-silence dominating relative ratios. It changes the loss weighting only. The finite bin grid ends at the last bin at or below 300 Hz. Evaluation uses the same block size, with a zero-padded final partial block. No loudness matching is performed on the audio.

This is deliberately not a raw full-band waveform-difference objective. It does not guarantee a perceptually better or hardware-faithful model: reference quality, alignment, architecture, domain coverage and listening still determine acceptance. Both component values are logged so improvement in one cannot silently hide a poor value in the other.

The UI offers an 8/16/32-unit LSTM with 4096-sample truncated backpropagation, eight sequences per batch, a 12,000-sample context prefix and 16 successive blocks before choosing new positions. Scripted studies also support 64 units, explicit learning rate and batch size, and independent-capture context. Their reports record the effective settings. Adam starts at 0.002 for a new model and decays to 0.0001; gradient norm is capped at 1. The selected checkpoint is evaluated through native C++. Export is rejected if native/PyTorch RMS disagreement exceeds `1e-4`.

For independent captures, `train_bus.py --random-record-context` samples different offsets within each training record and uses eight successive blocks. It computes the exact preceding record context with the current model weights, sharing recurrent computation when several batch entries use the same capture. It preserves record boundaries and leaves the validation objective unchanged. CPU and CUDA tests compare the resulting hidden and cell states against independent prefix evaluation.

New training runs start with zero recurrent/output biases and condition the optimizer's input by a fixed factor of 16. Export folds that factor into the LSTM input weights, so no extra input-gain stage is required and native inference receives the original dry samples. Targets, audition signals and final output gain are unchanged. The factor is recorded in the report. This helps represent steep overload curves without asking small initial input weights to grow by orders of magnitude.

The scripted 8•Bus continuation can initialize from its previous PyTorch checkpoint. That checkpoint is evaluated and saved as step zero before any optimizer update, so a worse continuation cannot replace it merely by being newer. Continuation starts at learning rate 0.0005, retaining the same objective and validation selection. The checkpoint hash is recorded in the training report. Hidden size and input conditioning must match the original training configuration.

## Listening and export

Choose Baseline, Reference or RTNeural model. Switching preserves transport position; it is not a crossfade or a sample-perfect live switch. Downloadable WAVs are 48 kHz mono float. Playback is never started automatically.

Saved runs have direct links using `?run=RUN_ID`. Opening one selects that run even if an older UI job remains in server history. The current simulated main-output candidate is available at <http://127.0.0.1:8896/?run=20260930-1429-8bus-main-gpu>; its clipping-onset and overload-recovery errors are documented in the study and are not accepted as a hardware match.

For algorithm/model teachers, compare the stage before or after the Talking Hedz filter, or disable the filter. A recorded target only supports filtering after the capture; getting the opposite order needs a new hardware recording of the filtered stimulus. The same manual OUTPUT applies after the entire chain, with no normalization or automatic compensation. Values above 0 dBFS are reported and remain in the float WAV. Use explicit OUTPUT to set playback headroom.

`model_rtneural.json` contains the selected mono 48 kHz LSTM + dense model. Its drive/route are baked into the capture; do not double-apply that gain when comparing with the workshop. `training_report.json` records the loss definition, component metrics, held-out diagnostic ESR, export parity, silence output and reference provenance. `result.json` includes audio levels, manual controls and input/runner hashes. Parity proves implementation agreement, not hardware fidelity or listening acceptance.

## Validation

```powershell
& 'C:\Users\hooki\AppData\Local\Programs\Python\Python310\python.exe' -m unittest discover -s tools/rtneural_workshop -p test_workshop.py -v
```

Checks cover finite gradients at identity/silence, bass-band selectivity, harmonic sensitivity, gain preservation, partial-block evaluation, signed alignment, disjoint chronological splitting, independent capture resets, native export parity, Proteus residual conversion, 44.1 kHz import and malformed model rejection. The nonlinear audition regression also verifies that manual OUTPUT never changes the signal driving the model.

See [SOURCE_REVIEW.md](SOURCE_REVIEW.md) for the actual 8•Bus evidence and capture path.

The [schematic-derived SPICE candidate](spice/README.md) supplies simulated pairs and audition renders while hardware recordings remain unavailable.
