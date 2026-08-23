# Handoff — 2026-08-23, session close

Branch `codex/native-arx-authoring`, worktree `C:\Users\hooki\trench-native`.
Head is `453ec33`. **The working tree is dirty and red — read "Stop here first".**

## Stop here first

The uncommitted diff is one change: **fold HP into EQ** ("2p2z" — every section is
the same object, so a row whose zero sits below its pole is an EQ with that offset,
not a third type). It is half done and 3 core tests fail.

Two honest options:

* **Revert and re-do deliberately.** `git checkout -- native/` returns to `453ec33`,
  which was verified: core suites green, Qt 48/48. (Full `verify.ps1 ui` was last
  green at `742ff8f`; at `453ec33` I ran the core tests and the Qt slots, not the
  whole layer.)
* **Finish it.** What is left is listed under "The HP fold, unfinished" below.

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

`native/app/tests/main_window_test.cpp` was edited so `first_eq_row` prefers a row
under 4 kHz (the old "first EQ" is now the air row at 10.5 kHz).

## Open questions from Tyson, unanswered

Three arrived while I was mid-build and I never replied. They are the next session's
opening material:

1. **"What would a pre-determined recipe into a fit look like?"** — i.e. seed the six
   rows from a named recipe (vowel, comb, wah…) and let FIT only refine it, rather
   than seeding from peak-picking. The machinery exists: `rows_from_formants` is one
   such recipe, `fit_rows_watched` takes any seed. What is missing is the *set* of
   recipes and how one is chosen/typed in the FIT room.
2. **"Do the manuals give us recipes?"** — not yet checked this session. The
   Mo'Phatt / UltraProteus filter tables give a name + one-line description per
   filter (LPF/PHA/HPF/FLG/BPF/VOW/EQ+/EQ−/REZ/WAH/DST/SFX) and the Morpheus manual
   describes individual cubes in prose ("this Pole/Zero filter is a notch that
   ranges from 80 Hz…"). That is the closest thing to a recipe list in a primary
   source, and reading those descriptions **as** recipes is the obvious next move.
3. **"The colors and sections as well as the overall UX seem slightly disconnected."**
   Nothing done. Standing hypothesis, needs a verdict before code: strip colour, the
   plot disc, and the FIT room's overlay are three separate colour vocabularies, and
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
  verbatim but cannot author one. Needs a word law for real pairs. This is now
  entangled with the HP fold (see failure 2).
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
