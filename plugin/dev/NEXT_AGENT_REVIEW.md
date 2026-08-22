# Review + work list for the next agent

Commit under review: `817d2fbb` — "X3 4-bank cartridges, the AGC wake-up, and
the GRIT remap". Branch `feat/x3-runtime-pipeline`, repo `trench-x3-clean`.

Read the commit message first; it carries the reasoning and the measurements.
This file is what to *check* and what to *do next*.

## Ground rules

- **No `git stash / reset / checkout / restore / clean` in this repo.** A reset
  earlier that day wiped a day of uncommitted work from two sessions. Read-only
  git only. A spare worktree exists at `C:\Users\hooki\trench-filter-list` on
  `feat/filter-list`.
- Another agent owns the UI and is actively editing: `plugin/source/ui/**`,
  `PluginEditor.{h,cpp}`, `UiLayout.h`, `plugin/assets/**`,
  `tools/apply_theme.py`, `tools/make_glass.py`, `design/themes.json`,
  `design/PALETTE.md`. Stay off those.
- Shared, edit with care: `PluginProcessor.{h,cpp}` and
  `plugin/source/dsp/TrenchDspBridge.h` — both sessions have code there.
- The VST3 install script targets `C:\Program Files\Common Files\VST3\`, which
  Tyson says is the **wrong folder** for his setup. Confirm the right path
  before building/installing anything.

---

## Part 1 — review this commit

Claims worth verifying independently, in rough order of consequence.

### 1.1 The cap was inverting the Q wheel

Claimed: with `response_peak_cap_db = 24.0`, turning Q up made Scream 6.6 dB
*quieter* and Drift 11.8 dB quieter, because the cap ducked by `peak − 24` and
Q is what raises the peak.

Check: `trench_engine_set_response_peak_cap(e, 24.0)` vs `INFINITY`, measure
output RMS at Q0 vs Q100 on `xml_scream` / `xml_drift`. Expect roughly
−6.6 / −11.8 dB with the cap on, +9.6 / +4.0 with it off.

If this holds, `presets_ship_v1/PLUGIN_PRESET_VERDICTS.md` (14 ship / 31 die)
was judged through it and needs re-running. That is the single biggest
downstream consequence in the commit.

### 1.2 AGC_DRIVE 1.8 is not uniform

Claimed: at −12 dBFS in, AGC pull is Shift 4.5 dB, Twin Peak 0.1, Opium 0.0,
Drift 18.1. The value suits the middle of the set, not the ends.

Check the spread yourself before accepting 1.8 as final. The bodies are not
level-matched, so no single constant is right for all of them — see 2.4.

### 1.3 GRIT drive span 7.0/6.0 → 1.1/0.9

Claimed: five interstage saturators compound, so the whole audible range is
`L ∈ 1.2..3.0`; the old mapping crossed 3.0 at GRIT 0.154.

Check: THD vs L for 1 / 2 / 3 / 5 cascaded saturators. Then confirm the new
mapping sweeps rather than snapping. **Ears outrank this measurement** — 42% THD
at full GRIT is a big character change from the old behaviour and Tyson has not
signed off on it by ear yet.

### 1.4 GRIT makeup exponent 0.55

Claimed: dividing by the full `local_drive` capped each stage at `1/L` and cost
13 dB across the knob.

Check `saturate(v*L)/L^k` for k = 1.0 vs 0.55 at −12 dBFS, r=1, five stages.
0.55 was chosen for level-neutrality; it is a taste parameter, not a law.

### 1.5 The teardown fix

Claimed: the old `Timer::callAfterDelay(2000, ...)` hung the host if it quit
inside those 2 s, and `engineRetired` was set but never read.

Check `retire()` in `TrenchDspBridge.h`. It now frees only when
`audioStopped` is true and **deliberately leaks otherwise**. That leak is
intentional and documented — do not "fix" it into a synchronous free.

Not yet verified: whether FL still freezes. Tyson reported it still froze after
an earlier build; this fix landed after that report and has not been retested.

### 1.6 Cartridges and roster

- 17 `.x3preset.json`, 68 banks, all codec-null 0.0000 dB. Re-run
  `python tools/x3_fundamentals_to_cartridges.py --dry-run` to confirm.
- Roster is 29 entries: 20 WORKHORSE `xml_*.body240` + 9 RUNTIME `X3F_*.json`.
- The baked filename is `X3F_<stem>.json`, **not** `.x3preset.json`, because
  JUCE mangles the filename into the BinaryData symbol and the roster looks up
  `base + "_json"`. There is a comment in `plugin/CMakeLists.txt`. Do not
  "correct" the extension.

---

## Part 2 — the work list

Ordered by risk. 2.1 is a live bug a user can hit today.

### 2.1 PREAMP latch (live bug)

Mid-PREAMP mutes hot bodies. `agc_gain` collapses to ~1e-7 within the first
block and takes ~3.4 s at 1.0001/sample to recover. Measured on Low Rider Q100
at PREAMP 0.5: output −141 dBFS, AGC reporting 131 dB of "reduction". At
PREAMP 1.0 the desk saturation bounds the signal below the table and it is
fine again — hence the non-monotonic feel.

Cause: `idx = floor(gain * mag) & 0xF`. With a 40 dB body and ~50× preamp gain
the magnitude is far outside the table's 0..16 domain, so the index is
effectively random, lands in the 0.12 region repeatedly, and gain collapses
geometrically.

Two options, and it is a judgement call, not a bug fix:

1. **Clamp the index** — `min(idx, 15)`. Emulates a hardware bus saturating.
   Diverges from the DLL, which genuinely does `& 0x0F`
   (Ghidra: "Index wraps with & 0x0f (no clamp)").
2. **Floor `agc_gain`** at something recoverable (~0.001). Keeps the wrap
   authentic, stops the latch.

Leaning (1): the wrap is authentic to a code path E-mu never fed magnitudes
this large, so reproducing it reproduces a bug they never shipped. Tyson's call.

### 2.2 Re-verdict the presets

If 1.1 holds, the whole sweep was run through a body-dependent duck that
penalised the hottest presets most. Bodies may have died for being quiet.
`presets_ship_v1/PLUGIN_PRESET_VERDICTS.md` is the file.

Ears only. Do not re-verdict by measurement.

### 2.3 The 20 `xml_*` bodies

Untracked working-tree files. 69 templates compiled through
`x3_morph_compiler.py` and matched against them on pole/zero words with SCALE
excluded: **0 matched**. So they were not produced by this compiler in its
current form. Their source is unaccounted for — DeepSeek generated them in
another branch and they arrived as loose files.

Tyson's instruction was "must be re-packed". Before doing that, note that
re-packing through the compiler would produce *different bodies*, not
reproduce these. Establish provenance first, or get a decision that
replacing them is acceptable.

Six of them (`crackle`, `low_rider`, `scream`, `peak_rise`, `cross_band`,
`cliff`) now carry a post-hoc SCALE clamp from `tools/clamp_body_gain.py`.

### 2.4 Level-match the bodies

At the same input, Drift and Scream pull ~18 dB of AGC while Opium and Crackle
pull 0. The 40 dB clamp equalised *peaks*, not loudness. This is why no single
`AGC_DRIVE` suits the whole set.

Note E-mu's own filters were not level-matched either — different filters had
different loudness, and that is part of the character. Worth asking whether
uniformity is actually wanted before implementing it.

### 2.5 Pole distortion is dormant

`R_new = R + R(1−R)(|Vg| − Vt)/|Vp|`. The `R(1−R)` stability brake peaks at
R = 0.5 and is ~1000× weaker at R = 0.999, so the mechanism does nothing on
exactly the resonant bodies where the bloom would matter. Measured `polepush`
was 0.000 on Opium at every GRIT setting and peaked at 0.086 on Shift.

Structural, not tuning. Options: scale the push by something other than
`R(1−R)`, or let it push down as well as up. It is a TRENCH invention (patent
US 10,514,883 inspired the mechanism, the implementation is ours), so there is
no authenticity constraint — ears are the only gate.

### 2.6 Rate-bank smoke test

Never run. Load in a 48 kHz and a 96 kHz session, confirm the loader selects
the matching bank and that formant/notch pitch does not drift. The FFI path is
proven (`rc=0` at all four rates) but nothing has been heard in a host.

### 2.7 The 8 hero bodies

In `trench-filter-list`, not here: Sinkhole, Tadpole, Drive Thru, Speaker
Knockerz, Fire Escape, Nosebleed, Opium, Basement. They exist as
`author_body.py` output, but **every guide target failed pre-flight lint** —
they are auto-fitter seeds, not authored bodies. Recipes are in
`dossiers/characters/INTENT.md`.

---

## Things not to undo

- `response_peak_cap_db = INFINITY`. Deliberate. Re-arming it re-introduces
  the Q inversion.
- The intentional engine leak in `retire()`.
- `X3F_<stem>.json` as the baked cartridge filename.
- Per-corner (never global) SCALE trimming in the packer.
- Formant synthesis in `catalogue.py` is a cascade (dB add). Klatt's vowels are
  all-pole in series and so is TRENCH; a parallel bank is the wrong model and
  was the first thing written here by mistake.
