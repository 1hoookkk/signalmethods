# NEXT — one edit authority on `native::Body`

Plan of record, updated 2026-08-25. The packed runtime and its parity tests remain
intact until the imported-body null gate passes. The native app is Tyson's dev
authoring workbench; the commercial TRENCH plug-in is outside this movement.

## Green movement gates

- [x] Baseline committed before migration edits: `8a14486`. Full headless CTest
  104/104.
- [x] 192 kHz native root coverage: `876c15a`. Full headless CTest 104/104.
- [x] `BodyDocument` owns `native::Body`; P2K is explicit import/export only:
  `0eaa463`. Full headless CTest 106/106.
- [x] One root-edit command owns pole/zero Hz + BW, DC law, gesture undo, and all
  editor projections: `ccb790f`. Full headless CTest 106/106.
- [x] Interior interpolates each root in log-frequency and log(1-R), then
  decodes. The morph 0.849 + transpose phantom-zero case and the interior/
  modulation envelopes are covered: `3600e0e`. Full serial headless CTest
  108/108.

## Green gate — native fitting and target workflow

- [x] Step 5 native fitting and explicit target workflow. Focused operator tests
  10/10; full serial headless CTest 117/117 (116 passed in the gate run, then
  the repaired Qt aggregate passed on `--rerun-failed`).

The target is an observed response, never a disguised root list:

```cpp
struct FitTarget {
  std::vector<double> frequency_hz;
  std::vector<double> magnitude_db;
  std::vector<double> phase_rad;  // optional
  std::vector<double> weight;
  TargetKind kind;                // envelope or transfer function
  bool absolute_level;
};
```

Primary interchange is CSV/TXT with required `frequency_hz,magnitude_db` and
optional `phase_deg,weight`. Naked 512-value magnitude files are rejected.
The same target types may be dropped directly on the main window.
Audio input creates a smoothed envelope plus LPC pole suggestions. Stereo
measurement audio also creates a complex left-input/right-output transfer target
with coherence weights. Existing body files contribute their complete native
cascade response. Targets never contain pole or zero positions.

The native fitter's 25 continuous variables are exactly six pole log-Hz/log-BW
pairs, six zero log-Hz/log-BW pairs, and one corner gain. Each trial is decoded
through the native runtime law, constrained stable, DC-normalised, assembled as
the ordered six-stage cascade, and accepted only when the weighted magnitude-dB
loss improves. Adam makes the broad moves; block-coordinate L-BFGS finishes.
One accepted step touches one section. Live root pins are read between steps;
gain is solved separately. No raw coefficient variables or coefficient-space
error are permitted.

The retained factory-forensics path continues to search exact minifloat words
through the packed decoder and scores the complete cascade. Packed words never
become variables in the native fitter.

Operator workflow:

1. Smooth/average recorded audio; ignore individual harmonic spikes.
2. Use LPC suggestions or operator placement for broad pole resonances.
3. Display the complete six-stage pole-only cascade.
4. Freeze those poles.
5. Fit native zero roots in log-Hz/log-BW against the complete-cascade residual.
6. Adjust corner gain separately.
7. Optionally release selected pole tokens for final joint refinement.

The UI exposes this directly: `FIT · AUDIO ENVELOPE` / `FIT · TRANSFER` /
`FIT · RESPONSE CURVE` names the selected target type; `1 LPC → POLES` commits
ordered LPC suggestions as an all-pole corner and holds them; `2 FIT ZEROS`
preserves zero pins and runs the ruled zero-only phase; `3 REFINE FREE` touches
only roots released on the response. The chassis FIT verb is the same zero-only
action, not an unqualified all-variable search.

The compact vowel journey is `FROM [AH]` to `TO [EH]` with one high-Q bandwidth
control. F1/F2/F3 keep sections 1/2/3 across the journey. It writes M0/Q0 = FROM,
M100/Q0 = TO, and their high-Q counterparts, starts pole-only, freezes poles,
and leaves zeros + gain free for FIT. Compiled factory surface corners do not appear
as vowel endpoints.

UI laws for this gate:

- The main response is the only response plot. `TARGETS` is a compact source list;
  the duplicate FitRoom plot is deleted.
- Response tokens are literal `p1…p6` / `z1…z6` fit pins, shown only on hover or
  for the active/fitting section. They never edit roots.
- The root surface remains the editor: double-click pole placement; horizontal
  drag frequency; vertical drag BW; Shift-drag BW only; zeros parked until drawn.
- The overlay names its real source and corner. Grey items are reference poles;
  LPC formants are a separate F1/F2/F3 layer. Authored and transposed locations
  are both visible and share one frequency mapping with the response plot.
- Minimal literal DSP vocabulary only: poles, zeros, Hz, BW, response, target,
  residual, fit.

Current implementation addresses: `trench-core/{fit_target,native_fit}.{hpp,cpp}`,
`trench-core/src/measure.cpp`, `audio/audio_boundary.cpp`,
`app/{fit_controller,fit_room,vowel_journey}.{hpp,cpp}`,
`app/main_window.cpp`, and `app/response_plot.cpp`. User-visible proof belongs
under `dev/e2e/` and is produced only by `-platform offscreen` tests.

## Remaining gates, in order

- [x] 6. Audition owns a native body/view snapshot and redesigns it when the
  audio device reports its ACTUAL rate; display remains at the requested rate.
  `AuditionRate.SameNativeBodyKeepsItsPitchAtTwoDeviceRates` proves the corrected
  44.1/48 kHz responses keep the same pitch and the stale 44.1 kHz cascade shifts
  by the rate ratio. Full serial headless CTest 118/118.
- [x] 7. All 33 imported bodies, four corners each, null against the packed
  decoder through `MainWindow → BodyDocument → native::Body`: 132/132 below
  −120 dB, worst −230.968 dB. Addressed report:
  `dev/e2e/imported_body_nulls.csv`. Packed forensic parity remains intact.
  Full serial headless CTest 118/118.
- [ ] 8. Bring Tyson the proposed native save shape before writing it.
  `.body240` remains explicit legacy export.
- [ ] Final address-backed change report after Tyson decides the native save
  shape. Current suite 118/118; screenshots, detune proof, and null report are
  complete.

## Open Tyson decisions — surface, never guess

- Native save format name and shape.
- Transpose wall pileup: compress or audible end-stop.
- Whether editor letters stay at authored positions or ride transpose.
- Whether `mine 1` / `mine 2` fossils are deleted.

## Standing laws

- Six ordered sections; section identity is never sorted.
- One undo per gesture. DC law on every root write.
- Bell/reference traces are read-only.
- All tests are headless with `-platform offscreen`; never open test UI on screen.
- A claim without a file/test/artifact address is not evidence.
