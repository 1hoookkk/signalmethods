# Handoff — 2026-08-23, session close

Branch `codex/native-arx-authoring`, worktree `C:\Users\hooki\trench-native`.
Head is `ca57b79`. Tree clean, `verify.ps1 ui` green (106/106, Qt 50/50).

## Since the handoff (2026-08-23, afternoon)

| commit | what |
| --- | --- |
| `376761c` | **HP folded into EQ.** `SectionType = {Off, LowPass, Eq}`. Census pinned: 562 EQ rows = 465 old EQ + 62 conjugate-zero ≥2 oct below + 35 real-axis-zero — exactly the old HP population, nothing else moved. Fc edits hold radius words (`mag_word_for`); real-axis-zero rows take Fc/Bw on the pole, ignore Gain. |
| `ca57b79` | **Strips own their markers** — one six-hue palette (`section_color.hpp`) clear of trace/residual/refusal; Fc readout in its hue; selected strip tinted + disc halo; hover links both ways. **Manual recipes** in the FIT room chooser next to the Klatt vowels: para A/E/O/U, comb 8ve, comb 1.61, one peak, wah. Proof `dev/e2e/app_strips_own_markers.png`. |

## What shipped this session (all committed, all gated when noted)

| commit | what |
| --- | --- |
| `731b39d` | plugin: MOVEMENT **DIVISION** (1/4…1/32T); **BITE was inert** in the shipped build (gated on the dev flag) — fixed. RenderNull chew 0.5/1 vs 0 = −12.4 / −1.2 dB where it was identical. cargo 185, null gate 360/360, lifecycle PASS |
| `8c9f1f9` | dsp: pole-radius distortion is **Rossum 2019 verbatim** — `R += R(1−R)(|Vp|−Vt)/|Vp|`. The old "this step has no source" comment was wrong (it read only the 1992 paper); `bench/facts.py` now carries the patent sentence. Zero still nulls |
| `1847b04` `76fd897` | core: Klatt Table II vowels; **rows fitter** over (Fc, Bw, Gain) with zeros derived by the row law. Vowel self-recovery 1.0 dB rms, Hedz corner 0 5.8 dB. Seed is pole-first by prominence (Klatt: the valleys come free), troughs fill leftover slots as carving EQ rows (Fant free zeros) |
| `85cc844` | app: **FIT is the rows fitter**. Held = the current corner; pinned and untyped (real-axis) rows keep their bytes verbatim; all-identity corner seeds from the target |
| `e84a7ca` | app: plot markers — one filled disc per live section on the curve, zero ring only for the selected strip, no numerals, no level lane, colours 58 % → 92 % saturation ("the dots everywhere are noisy", "the colors are weak too") |
| `3dedda6` | app: **FIT room** (Bell 1961 Fig. 3/7) — points, model, difference curve, one rms score; overlay list, selected row is the target; vowel chooser writes Klatt rows as one undo. Ctrl+F |
| `bbc234d` | app: **ears** — hold Space to hear a 49 Hz saw or the loaded audio target through the view cascade; Morph/Q/transpose audible live. Core runner verified against the plotted response (±0.5 dB); `--audition <s>` sweeps Morph 0→1→0 through the real device |
| `e5fff9a` | app: **corners are files** — `.corner` = one frame's 60 bytes. Ctrl+K save, Ctrl+L load into the current slot (one undo, mirrored), `--load-corner N=path`. Proven byte-for-byte: Hedz frame 3 into Early Rizer slot 1 |
| `742ff8f` | app: **tune-in** — ±24 semitone slider, the plugin's X3 tracking law ported verbatim (angle × ratio, radius kept, sub-anchor poles < 70 Hz and zero walls exempt). View only: plot, score, ears follow; bytes and undo never move |
| `453ec33` | core/app: **the trench is the low section** — the LP row's third control is the trench (its zero at the S6 floor radius, 2–7 oct above Fc), not a gain. `param_of` reads every factory low section's trench from its zero; the fitter searches the trench and seeds it on the target's deepest valley; strip fader shows Hz. Hedz strip 6: Fc 225, trench 7221 = the −84 dB notch |

## The HP fold, unfinished (the dirty diff)

Done: `SectionType` is now `{kOff, kLowPass, kEq}`; `far_hz` single-branch;
`param_of` classifies LP or EQ only; strip menu, vowel recipe and rows seed no
longer emit HP; `mag_word_for(hz, rsq_word)` added to `p2k.hpp` / `container.cpp`
so an Fc edit can move frequency while holding the radius words.

Failing, all in `native/trench-core/tests/p2k_section_param_test.cpp`:

1. `AnFcEditSlidesTheWholeBandAndReturnsToItsOwnBytes` — line 159/160, counts
   487/445 against pins 415/373. These are censuses over "EQ rows", and the row
   population grew when HP folded in. **Decide whether the new counts are correct
   before re-pinning** (they probably are — same rows, new name — but that is the
   exact move the project forbids doing blind).
2. `AGainEditKeepsThePoleAndTheAuthoredZeroOffset` — line 197, `zero` is NULL.
   A folded-in row can have a **real-axis** zero, which `std::get_if<ConjugatePair>`
   returns null for. The test needs to skip real-axis zeros (or the law needs to
   say what a gain edit means there). I added a real-axis branch in
   `words_from_param_keeping_offset` for kFc/kBw; kGain returns `current`.
3. `TheFourControlsCarryAFifthOfTheBankSEqRowsWithinThreeDecibels` — line 242,
   562 vs pin 465: same census growth as (1).

`n## Evening: the direction is ruled, the envelope exists

Ruled: drop byte preservation, keep the architecture (`CLAUDE.md` at `5297990`;
reasoning in `dev/SECOND_OPINION.md` and the chat-brief exchange it came from).

Built on the *current* engine, before anything changes, so the replacement can be
proven not to move them:

| file | what it is |
| --- | --- |
| `dev/interior_envelope.txt` | 33 bodies × (step, excursion up/down vs corners, bilinear deviation, detour, pink loudness swing/beyond, prefix headroom/floor) on a 33×33 grid. `trench_interior_envelope.exe` regenerates it. |
| `dev/modulation_envelope.txt` | plugin engine, cascade linear: moving peak vs loudest frozen point, tails, non-finite and muted-stage counts. `cargo test --test modulation_stress -- --nocapture`. |

Three facts from them, each now a pinned test:

1. **No factory body stays between its corners.** Median excursion below the corner
   floor 81 dB; median bilinear deviation 17 dB. Acceptance is the factory distribution,
   never "stay between the corners".
2. **Motion is not covered by the frozen-stability proof.** Linear, 22/33 bodies pump
   >6 dB above any frozen point (ace_of_bass +76); nothing diverges. The section
   nonlinearity bounds it to +15 dB — it is load-bearing and the float engine keeps it
   or an equivalent.
3. **The word lerp breaks unity DC in the interior** by up to 4.6 dB (corners hold to
   0.02 dB). Blending real coefficients with unity at the corners fixes this exactly;
   it is the one place imported bodies will differ from the P2K runtime mid-morph.

## The float engine (evening)

`native_body.hpp`: a body is 4 corners x 6 sections, each pole and zero stored as
`Resonant{hz, bw_hz}` (or `RealRoots` as signed decay Hz for imported real-axis pairs),
plus one `gain_db` per corner. `design(corner, sr)` makes coefficients at the host rate;
`blend(body, m, q, sr)` is US 10,514,883's law — bilinear interpolation of log frequency
and log bandwidth per root, decoded after; `cascade(design, gain_db)` applies unity DC
per section and the corner gain once. `import_p2k` decodes the 240-byte bodies.

Proven (`native_body_test.cpp`, all 33 bodies): every corner nulls against
`corner_response_db` to < 1e-9 dB in shape and level; the interior is stable and
unity-DC on a 17x17 grid at 44.1 k and 48 k; motion through the blend is finite.
`trench_interior_envelope.exe float` writes `dev/interior_envelope_float.txt`. Hedz under
the patent law sits on the hardware interior (max step 5.28 vs 5.20, excursion 69/93 vs
70/97, loudness swing 39.5 vs 38.9). A coefficient-blend interior was tried first and
drifted (7.44 / 76 / 44); the patent names that approach as the thing it replaces, and
the ruling text that asked for it was wrong. CLAUDE.md now cites the patent.

Two findings: the factory corners are not unity-DC (worst 42 dB; the 0.02 dB figure was
drift relative to corners), hence one gain per corner; and the bank holds 54 real-axis
pairs in 35 mixed lanes, which the patent's representation does not have — the rule for
them is in CLAUDE.md. The plugin now runs the same law: `minifloat.rs::interpolate_biquad` interpolates each
root's log angle and log(-ln r) and the stage scale in log, decoded after; corners exact;
the engine's two rebuild paths and the null gate's Python oracle follow. 185 tests, gate
PASS, lifecycle PASS; `dev/modulation_envelope_2019.txt` is the re-measured motion table
(worst +65.7 dB over frozen, was +70.1; nothing diverges). Installed to Program Files
17:57. Not done: the app and fitter still author on the packed words.

## The three questions, answered

1. **Recipe into a fit** — no new machinery. A recipe writes six rows as one undo;
   `startFit` already seeds from the corner's typed rows, so recipe → FIT = FIT
   refines the recipe. Only the recipe *set* was missing.
2. **Do the manuals give recipes?** Only the Morpheus manual, and only ~10 with
   numbers (paravowel peak tables, the two comb laws, One Peak, PZ Notch, BassEQ,
   Wah, VowelSpace). The Mo'Phatt table is names + one line — a type list, not
   seeds. The numeric ones are now in `formants.cpp::kManualRecipes`; widths and
   gains are ours (manual states none).
3. **Colours/sections disconnected** — diagnosis: rainbow `index/7` hues collided
   with the trace (section 4) and the residual (section 6); strip order is section
   order while the plot is frequency order, so colour has to carry the link and it
   was a 2 px band and a small dot; selection was a 1 px edge; no hover. Fixed as
   above. Strip order stays section order (identity is ordered, never re-sorted).

our vocabularies, and
   the strips do not visually own their marker (no hover link, no shared highlight on
   selection). Ask before rebuilding.

## Priorities as ruled today (memory: `trench-priorities-2026-08-23`)

Ears → corners as reusable objects → tune-in. All three built. The deliverable
behind them is unchanged and still not started: **the bank**. After the TB303 pull
the roster carries no body authored by us. Twenty corners, made by ear in this app,
assembled into bodies, nulled into the plugin.

Also still open (from `METHODS.md` / `current.md`, read this session):

* **Correction spectrum** — Bell co-fits a smooth source/radiation curve as a search
  parameter; our vowel recipe fakes it with fixed tilts and a loaded recording still
  contains its source. Cheap, decisive, would explain why our fits are flatter than
  the bank.
* **Comparator symmetry** — Bell smears target *and* candidate through the same
  filter bank before scoring; our objective ERB-weights the grid but does not blur
  the candidate identically.
* **Real-axis rows (LP1/HP1)** — 46 of 132 factory corners carry them; we hold them
  verbatim but cannot author one. Needs a word law for real pairs. After the fold
  they read as EQ rows whose Gain edit is a no-op.
* Gain cuts (S3/S6): rule never recovered, we write none, 105/132 factory corners
  write none either. Leave it.

## Verification

`pwsh verify.ps1 dsp | wrapper | roster | ui | all` — run the layer that changed.
Traps that cost time today, in case they recur:

* A long `PATH` makes the VS dev-cmd fail with *"The input line is too long"* — the
  short PATH is in `verify.ps1`.
* The Bash tool unescapes `\n` inside heredocs, so writing C/C++ string literals
  through `py - <<EOF` silently produces `error C2001: newline in constant`. Use the
  Write/Edit tools, or a script file in the scratchpad.
* The Qt test binary prints nothing under the Bash tool; run it from PowerShell.
* Two builds sharing `out/build/...` clobber each other's links (a 2 KB stub exe).

## Not done, needs you

The VST3 in `C:\Program Files\Common Files\VST3\` is still the 22:26 build from
yesterday. The current one (DIVISION, live BITE, Rossum law) is at
`plugin/build-juce9/TRENCH_artefacts/Release/VST3/`. Close FL Studio, copy it in,
restart FL.
