# HEADSPACE shared authoring canvas

The current conversation defines this slice. Later user corrections supersede the initial two-surface and selected-endpoint-copy rules in `HEADSPACE_SPEC.md`: AUTHOR and BODY share one canvas. All exploration is transient; only explicit 1–4 stamps write the complete sounding state into a corner. LPC and eight-corner authoring remain outside scope.

## Base and preserved variants

The established C++ HEADSPACE application target is the base. Its existing `Quad` code, inspected at HEAD because the source was already deleted in the working tree, supplies the six-section/four-corner representation and the core-backed `bodyOf`, `lerp`, `bytesOf`, and response patterns. The ImGui surface retains that packed boundary. The source files under `Source/slice` implement only this task's endpoint operations.

Discovery found these prior implementations:

| Implementation | Evidence at discovery | Fit |
| --- | --- | --- |
| C++ HEADSPACE WebView front end, `Source/app`, `Source/ui/Web`, `Source/web` | Sources/build file already deleted; old executables present; current build failed with unknown `TRENCH_Headspace_App` target | Closest existing packed body/application base |
| Painted C++ HEADSPACE, `Source/ui/Screen` | Sources already deleted; inspected through Git; shared the same application/model | Existing four-corner mechanics, historical interactions |
| Earlier C++ frame/morph workstation, `Source/model` | Sources already deleted; separate model in Git | Wider frame/stitch architecture outside this slice |
| Qt workstation, `workstation_app.py`, `native/python/workstation` | Python, Qt, core wrapper, anchor and audio imports succeeded | Has four corners, but automatic anchoring and no ImGui |
| marimo workstation, `workstation.py`, `workstation.html`; 3D HTML prototype | Live source/artifacts; not claimed built or accepted | Notebook/browser interactions outside this slice |
| SGI Studio, `tools/sgi_studio/main.cpp` | Live Dear ImGui/DX11/miniaudio analysis application | Analysis, not an endpoint authoring base |

Other implementations and their launchers were not edited. Already-deleted historical files were not restored wholesale. Before relinking, the two old workstation executables were copied to `%TEMP%/headspace-before-slice-20260908/`. No new application target was added alongside HEADSPACE.

## Files

- `Source/slice/Model.*`: exact endpoint assignment and validated `setCorner` snapshots, six-pair source admission, nearest-corner selection, selected-only pole edits, pure edge/pad resolution, canonical export.
- `Source/slice/Audio.*`: bounded single-producer/single-consumer publication of the same resolved words/cascade used by the plot; canonical runner and saw source.
- `Source/app/Main.cpp`: custom ImDrawList pad, endpoint nodes, acoustic trapezoid, compact cascade plot, mutually exclusive Perceptual/Stage decks and pole selection on the existing Dear ImGui/Win32/DX11/miniaudio host.
- `Source/slice/Audition.h`, `History.h`: live draft, stable stage identities, selected-pole operations, and bounded exact gesture/stamp history.
- `Source/slice/Response.h`: adaptive sampling of the canonical response, seeded at pole centers and bandwidth offsets.
- `Tests/SliceTests.cpp`, `Tests/RenderChecks.h`: contract, macro, independent decode, dense-curve and impulse FFT checks.
- `CMakeLists.txt`, `build_headspace.cmd`, root HEADSPACE presets: established target/build path.

The canonical core DSP files are unchanged by this slice. One line in `plugin/source/dsp/TrenchDspBridge.h` now uses `interpolate_biquads` at the native datum, replacing fractional-word decoding so the plugin audio target resolves from the same integer packed state as HEADSPACE and its own probe. The existing plugin test target includes a discriminating 16-position regression. Other sample-rate grid/rewarping and plugin dynamics are unchanged. `PackedBody`'s internal seventh identity section and mirrored second plane are only its existing legacy adapter; authored state is exactly four arrays of 30 words.

## Sources

The final requested collection is 12 Klatt anchors plus 8 compatible P2K posture tables. DVTD and the other recipe collections are excluded from the app.

`Source/slice/Klatt.h` contains the twelve already-supplied six-pair Klatt anchors read from `native/workstation/data/headspace-anchors.json` at commit `9b6dc67b3a02b2cb020a6cbf6347db3ddf78741a`. That historical file was already deleted in the working tree and was not restored or replaced. The selected table supplies every frequency and bandwidth, including F4 3300/250, F5 3750/200 and F6 4900/1000 Hz. Its stored provenance attributes those upper values to Table I and the per-vowel first three pairs to Table II of `evidence/mouths/klatt/Klatt-1980.pdf`. This slice reuses the supplied complete table; it does not infer missing poles from the three-pair core table.

The eight six-pair P2K posture tables in `native/core/src/p2k/postures.cpp` use their supplied section order. The ninth table has only five pairs and is skipped. They are named pole-only authoring templates, not byte-identical factory bodies.

Both source groups compile once through the existing core geometry encoder at 44,100 Hz, with parked numerator words and unity section scale. Bootstrap copies the resulting endpoints byte-exact, with no automatic Q, bandwidth, gain or ordering changes.

The images supplied in the conversation show an 83-group Morpheus skeleton analysis and two P2K pole-only examples. The former is not an 80-entry, compatible P2K endpoint bank: Morpheus uses a different section count and datum. It remains outside this slice. Parking P2K zeros is a valid intentional authoring operation, but it changes the factory transfer function; the supplied Lucifer's Q c0 example also has a real sixth section. Those examples do not justify adding missing pole pairs or labelling the current eight posture tables as eighty.

The right-hand source browser has KLATT and E-MU tabs with the same gestures. Hover previews a complete endpoint; click retains it as the live draft. Leaving hover restores that draft. Neither gesture writes a corner. KLATT uses a discrete F1/F2 map with IPA vowel labels tethered to the supplied anchors and live F1/F2 readout; E-MU uses a scrollable template list. The trapezoid is explicitly an anchor browser, not an unimplemented continuous acoustic interpolation engine. Both collections feed the same four corners and Morph/Q pad. The New body control is reserved for exact one/two-template bootstrap.

Every source pair is tested at 101 equal Morph intervals: each word follows the canonical integer code-space interpolation, each state remains compatible, and capture plus 240-byte export preserves it exactly. Equal code-space intervals do not claim equal perceptual distance, Hertz spacing or loudness.

## Operation and verification

At startup choose one or two templates and initialize the four endpoints. One shared canvas contains:

- A single overall cascade response plot, constrained to 3:2 and at most 440 pixels wide.
- A square Morph/Q pad with named M0/Q0, M1/Q0, M0/Q1, M1/Q1 nodes. Pad movement auditions canonical packed words without a second interpolation/rounding pass. Its edge rails and corner nodes play exact stored edges/corners.
- Perceptual mode shows the Klatt trapezoid or E-mu source browser and Tract Scale, Tension, Stress. Stage buttons are hidden.
- Stage mode hides the macros and source browser. Lo/Hi frequency and bandwidth controls sit side by side. A draft edge retains its six section identities through crossings. Horizontal pad movement sweeps it at fixed Q; touching a stored edge/corner returns to that body's audition. This never saves the draft implicitly.
- The compact cascade carries live pole overlays: F1–F6 in ascending acoustic order, S1–S6 in fixed section order in Stage mode. Sorting is only for display and never reorders packed words. These are pole locations, not a claim that every pole is a distinct spectral peak. Stage mode adds faint Lo/Hi cascade references, selected bandwidth, and a Lo-to-Hi direction marker.
- Click a curve pole/label or Stage button to select one section; Ctrl-click toggles membership. All / Ctrl+A selects six. At least one remains selected. Selection changes no sound. Macros affect only selected sections; Stage group knobs apply relative frequency/BW ratios to selected sections on the chosen end. Selection rebases macro values to the current sound without changing it.
- One warm-white highlight on flat charcoal; no cyan or glows. Hover, selected controls and live nodes use this same language. A detached live draft does not falsely light a stored corner.
- Keys 1–4 and Capture buttons stamp the complete live state, including a hover preview, into the requested corner. Numeric entry and open popups consume these keys. No other edit writes corners.
- Undo / Ctrl+Z and Redo / Ctrl+Shift+Z / Ctrl+Y restore exact draft and corner words, names, selection and pad position. One drag is one history entry; hover alone adds none. The bounded in-memory history holds 128 entries.
- Playback, listening level, exact one/two-template bootstrap and canonical 240-byte export remain available.

One `Resolved` value contains the 30 packed words and their core-decoded cascade. The response uses this value and the audio queue receives the same value. Audio retains delay state and uses the canonical 256-sample approach. During that short transition the runner is approaching the displayed target; settled responses are checked against the plot. Ring levelling and pole distortion are disabled for this linear audition. Packed gains are never normalized or altered for listening. The LISTEN control sets a monitor RMS reference for the fixed 110 Hz saw, default -24 dB, with a separate peak readout. A harmonic response estimate supplies the monitor scalar; gain and transport gates approach over 256 samples, and a final monitor gain guard caps extreme transition peaks at 0.5. This changes monitoring only, not the transfer-function plot or body bytes. At the former fixed -140 dB monitor setting, the source bank measured -143.965 to 0 dBFS RMS, with a pre-clamp transition peak of 112656. With the new -24 dB reference, all 20 sources measure -23.9997 to -23.9612 dBFS after settling. Source switching is checked for finite, bounded samples. These measurements are not perceptual loudness or listening acceptance. Playback starts stopped; miniaudio handles device-rate conversion outside the 44,100 Hz filter datum.

Zero editing is deferred. The slice does not invent a packed zero/gain normalization operation.

Run `native/workstation/build_headspace.cmd`, or build with `cmake --build --preset headspace` in the MSVC environment and test with `ctest --preset headspace`.

`HEADSPACE.exe --smoke` exercises real ImGui input silently: pad and vowel hover, exact stamps, draft-only macro and Stage gestures, whole-gesture undo/redo, exact stamp undo/redo, E-mu hover/pick/stamp and hover exit. It captures normal/compact layouts and exits. Headless tests additionally cover all source-pair sweeps, section crossings, selection masks, relative group editing, and exact preservation of unselected words.

Launch with the existing `Launch_HEADSPACE.bat`. Listening, monitor level and musical feel require the user's judgement.


## Macro boundary

Macros are explicit edits, never bootstrap transformations. The live draft keeps an unmodified anchor for the current shaping gesture family; source replacement or a changed pole selection starts a new anchor. Reset shape restores all 30 anchor words exactly. Changes are computed and validated in a temporary endpoint before assignment; failure leaves the draft and controls intact. Corners are never changed by a macro. Section order, zero words and gain words are retained.

- Tract scale: -12 to +12 semitones, ratio `2^(st/12)`, applied to both frequency and bandwidth. Source pitch stays at 110 Hz. Invalid out-of-range results are rejected, not silently clamped or fitted.
- Tension: bandwidth multiplier `8^(1-2t)` from 8x broad to 1/8x sharp. The midpoint preserves anchor bandwidths. It is independent of the pad's authored Q axis.
- Stress: frequency moves linearly between an explicit ideal-tube reference (500, 1500, 2500, 3500, 4500, 5500 Hz) and the anchor, then tract scale is applied. The neutral frequency is assigned by fixed section identity, never current frequency rank; natural crossings are preserved. This is a synthetic authoring reference, not a measured neutral voice or inferred source dataset. Odd-spaced tube resonances are described in [Praat's source-filter documentation](https://fon.hum.uva.nl/praat/manual/Source-filter_synthesis_2__Filtering_a_source.html).
- Vowel position chooses a complete supplied six-pole endpoint. No anatomical constants, missing formants, LPC ingestion or new corpus conversion are inferred.

These are useful physical mappings, not experimentally calibrated equal-perception controls. Carve/Tilt remains deferred pending a defined zero-shaping law: section gains in a serial cascade multiply overall level; they cannot independently tilt its spectrum. No zeros, source spectrum redesign, MIDI mapping or PCA/MDS pipeline was added.

The selected ideas from the supplied literature note are acoustic targets, coordinated controls, direct trajectory audition and independent source pitch. Historical parameter counts, fixed keyboard modes and claims that one update interval guarantees smoothness were not adopted as requirements.

## Rendering evidence (2026-09-08)

The plot uses canonical `cascade_response_db` on the same `Resolved` packed state submitted to the audio queue. It retains its dB scale during ordinary pad movement; the range only expands if a new response exceeds it. It never renormalizes a curve to hide gain changes. Identical corners are identified explicitly.

The independent arithmetic decoder check covers all 65,536 codes, including zero and full-scale sentinels, with rational golden cases and exact coupled biquad fixtures. It verifies the repository's specified packed format; it is not a measurement of original hardware arithmetic. Rossum's [US5170369A](https://patents.google.com/patent/US5170369A/en) supports the modified recursive coefficients and approximately logarithmic interpolation concept, but does not by itself certify every P2K storage convention or hardware truncation detail.

A 262,144-sample impulse is measured through the actual canonical runner after a 256-sample target transition, with levelling/distortion disabled. Across 17 corner/interior/narrow-resonance states, FFT bins within 80 dB of each response peak must agree within 0.05 dB. The current measured maximum and bin count are printed by the test in `out/build/vst3/Testing/Temporary/LastTest.log`. The plot is the linear target response, not the short coefficient-transition response or the clipped monitor output.

A 100,001-frequency dense oracle per state checks adaptive-curve line error against a 0.1 dB limit, including six 2-Hz bandwidth resonances. The measured error and maximum vertex count are printed by the test. Every plotted ordinate still comes from the core evaluator.

HEADSPACE build and `ctest --preset headspace` pass. Silent ImGui smoke checks pass at normal and compact window sizes. The plugin target builds and its new integer-word parity checks pass, but the full `trench_plugin` suite reports 15 failures in preamp, processor levels, desk compensation/position and leveller expectations. Those paths were not altered to make the suite green. This slice does not claim full plugin acceptance or hardware/listening equivalence.
