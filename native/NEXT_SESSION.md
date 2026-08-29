# NEXT SESSION — workstation (written 2026-08-29, end of the recovery session)

Branch `face/ship-candidate-fx`. Everything below assumes the committed state
through `764c006` (workstation) and `5762791` (tools). Launcher:
`TRENCH Workstation.bat` at repo root (points at
`out\build\app\native\app\trench_native.exe`); build: `TRENCH Build App.bat`
or `cmake --preset app && cmake --build --preset app`; tests:
`ctest --preset app -R native` (headless, offscreen). Never run two
vcpkg-configuring builds at once; `out/build/app` has
`VCPKG_MANIFEST_INSTALL=OFF` pinned so ninja's cmake re-runs stay out of vcpkg.

The checkout is shared live with other chats (one stages `plugin/` work in the
same index). Commit native work with explicit pathspecs only:
`git commit -F msg -- <paths>`.

## What this session closed (all native, all tested)
- One cascade. `EditorState` renders the document to a `PackedBody`
  (`export_p2k_body`); graph, ears and `.body240` export all consume
  `design_audition(view, rate)` from that packed body at every Morph x Q
  position and device rate. The editor-local log2 lerp is gone. OFF sections
  and absent zeros are the core's identity roots (`RealRoots{inf, inf}`) and
  export as the identity words; body_io's hand codec is gone.
- SAVE writes a lossless, versioned `.trenchbody` JSON document (four
  corners, six ordered sections, pole/zero roots, enabled and zero-present
  flags, corner gain, editing corner, Morph/Q) atomically; OPEN restores it
  as the editable document. `.body240` is an explicitly labelled EXPORT.
- Imports route by extension only (`.fbw` poles, sound, `.csv/.txt` response
  tables with a validated schema and REW `*` headers, `.Table` Praat formant
  tracks as median poles of the voiced frames, `.trenchbody`, `.body240/.bin`
  reference). An all-positive response table can never become pole material.
- AUDITION is a checkable button; device-start failures land in the status
  line; ears redesign the same packed view at the actual device rate; the
  device is released on stop and on close.
- Display pass finished: expanding response plot, 1 px hairline curves, the
  Ctrl-wheel diagnostic frame removed, z-plane edits confined to the upper
  unit semicircle (double-click outside the disc or in the lower half is
  refused; drags mirror the lower half and stop at the domain boundary), the
  compact pane fits its circle and labels, the top row no longer forces the
  window wider than its 1180 default (true minimum 1177x732).
- `tools/extract_poles.py`: one-button (f, B) caricature of any WAV. Auto
  mode picks formant bands (one peak per acoustic band: sub / warmth /
  vowel-horn / presence / sizzle / air) when the dominant peaks are harmonics
  of one fundamental, otherwise the six most dominant distinct tonal peaks
  sorted into S1..S6; `--mode peaks|bands|lpc` forces; writes `.fbw` that
  OPEN takes into a corner, plus .json/.csv/C++ initializer.

- ANALYZE is now a pole-zero envelope fit in the Atal-Schroeder sense: f0
  from the core comb (octave errors resolved by harmonic share over the first
  twelve harmonics, divisors 2-8), the envelope sampled at the harmonics
  (cubic between them) when voiced or peak-hold smoothed otherwise, a
  minimum-phase impulse response through the cepstrum, Prony-Shanks 12/12 on
  it, then pruning by detail signature (0.5 dB, half-octave baseline) and the
  M0-corpus gate (a seat below 700 Hz needs Q >= 1.5, a formant above needs
  Q >= 5; roots above 4.4 kHz are band-edge artefacts). Proof: a synthetic
  nasal (280/60, 1250/120, 2300/160, 3300/250 + antiformant 900/150) returns
  292/99, 1252/155, 2305/166, 3306/253 and 901/106; the recorded /ah/ returns
  644/138, 1079/119, 2649/196. `TRENCH_ANALYZE_DEBUG=1` prints the f0 search,
  the unpruned fit and every pruning step.
- Queued from this pass: the "Configurable_PEQ" analyzer text (cuts -> poles,
  boosts -> zeros; Frequency / Bandwidth(Hz) columns) as an OPEN route; the
  scrub ribbon so ANALYZE fits one chosen moment per corner (VV2 recipe:
  ~30 ms steady state, onset and offset of a diphthong = corners A and B).

## Measured contract facts (report, not fixed — Tyson's call)
- Lattice quantisation of the core exporter: an authored 250 Hz / 250 Hz pole
  exports as 233.68 Hz / 251.03 Hz (`export_p2k_corner`, native_body.cpp).
  The graph now shows the exported truth, so authored marks and the curve
  can visibly disagree by up to ~6% in frequency.
- S6 zero rule (`native_body.cpp:324-326`, `kS6ZeroRsqWord`): the boot state's
  S6 zero (12 kHz / 12 kHz bw, paired with its pole) exports as an
  8559 Hz / 0.026 Hz-bw needle; corner 0 S6 authored-vs-packed diverges by
  31.6 dB (test `s6_zero_export_law_is_the_existing_one` prints it). The
  "boot is a flat EQ" verdict and the packed S6 contract contradict each
  other whenever S6 carries a zero. Removing the S6 zero exports identity.
- The audio boundary's old corner special case (continuous native design at
  exact corners, packed elsewhere) was dropped in favour of the packed law
  everywhere; corners are now the corner's packed words, so there is no jump
  between corner and interior.

## Evidence classes produced
- Build: fresh `cmake --preset app` configure + build, then incremental.
- Tests: 22 CTest cases under `native.*`, all passing headless;
  the two audible cases pass live with `TRENCH_AUDIBLE=1` and skip otherwise.
- Packed parity: graph vs export re-import 0 dB delta over 3 rates x 8
  positions x 240 grid points; exported words legal (`p2k::is_legal`,
  `pole_is_legal`) and stable at 44.1/48/96 kHz; DC gain within 0.005 dB.
- Audio: device "Primary Sound Driver" at 44100 Hz opened, streamed 1.5 s
  of the built-in saw through the boot cascade, closed and reopened
  (`audition_device_opens_streams_and_closes`); the AUDITION button reported
  `AUDITION OPEN · PRIMARY SOUND DRIVER · 44100 Hz` then `AUDITION CLOSED`.
  This is signal-presence evidence, not listening acceptance.
- Visual: offscreen screenshots at the minimum and normal sizes in
  `out/build/app/smoke/` (offscreen renders glyphs as boxes; geometry only).
- NOT produced: an on-screen launch through `TRENCH Workstation.bat`, a human
  listening pass, a human look at the face. Session rule kept everything
  headless.

## Queue after this (unchanged rulings)
1. Scrub-ribbon import: `.par` / tracker trajectories (Praat `.Table` such as
   Downloads/c1r1.Table is exactly this material) in the lane seat, stamp a
   moment to a corner, stamp A+B to a morph pair.
2. espeak SPECTSQ2 decoder — 172-language mouth library for the shelf.
3. Second-wave dials: STRIKE, AIR, FLUTTER, NASAL; selection-scoped
   transforms.
4. Shelf audition pass — 41 postures, none ear-judged yet.
5. OneDrive: Documents/Desktop are redirected into a failing OneDrive; the
   app's data home is OneDrive\Documents\TRENCH. Fix or unlink before
   trusting any Documents write.

## Known engine-proof divergences (not UI bugs)
FaceShot's LIMIT and MOVE FOLLOW checks fail against this branch's engine —
ported-harness expectations, tracked separately, untouched by UI work.
