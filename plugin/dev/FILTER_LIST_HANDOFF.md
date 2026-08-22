# FILTER LIST — handoff for the agent

2026-08-05. What's decided, what's done, what's left. Read this first.

## The decision (Tyson, 2026-08-05)

The plugin preset menu ships EXACTLY this set, nothing else:

1. **WORKHORSE (20)** — the XML Morph Designer set under its shipped names:
   Cross Band, Peak Rise, Twin Peak, **Opium**, Low Shape, Notch Walk, Peak Walk,
   Shift, Shine, Blade, Cliff, High Rise, Low Rider, Soft Touch, Crackle, Crisp,
   Drift 2, Drift, Scream, Throat.
   Files: `plugin/presets/bodies/xml_*.body240` — 6-stage body240, Hz-anchored.
2. **RUNTIME (9)** — the X3 runtime workhorse fundamentals, **verbatim ROM
   cartridges, 1–3 stages, rate-locked** (datum_rate 0, nearest-bank selection):
   2-Pole Lowpass, 4-Pole Lowpass, 6-Pole Lowpass, 2-Pole Highpass, 4-Pole
   Highpass, 4-Pole Bandpass, Contrary Bandpass, Swept EQ 1-Oct, Phaser 2.
   Files: `bodies/candidates/X3F_*.x3preset.json` — 4 banks (44.1/48/96/192k),
   active stages 1–3, words = 4 × stages × 5 per bank.

**Never pack the runtime filters into 6-stage body240.** They're 1–3 stage
verbatim ROM. Forcing them through the 6-stage/Hz-anchored body240 path is
forbidden (violates the xStream law). They load via the cartridge path.

## The three formats (understand before touching)

| Format | File | Axiom |
|---|---|---|
| `.body240` | 240 B = 4 corners × 6 stages × 5 words | 6-stage, Hz-anchored, recompiles to host rate |
| `.x3preset.json` | `X3F_*.x3preset.json`, 4 banks, words = 4×stages×5 | verbatim ROM, 1–3 stages, rate-locked, nearest bank |
| `.raw` (ROM block) | `ref/x3_menu/runtime_blocks/<stem>_<rate>.raw` | the words the cartridge is built from |

Verified: all 17 cartridges' 68 banks byte-identical to their ROM blocks
(0 mismatches). bat_phaser 44.1k bank first corner `[8438,496,9461,33020,57452]`.

## Current roster state (`plugin/presets/PresetRoster.inc`)

**WRONG — the 9 RUNTIME stems say `cleanroom_*` (e.g. `cleanroom_2_pole_lowpass`).
They must say `X3F_*` (e.g. `X3F_2_pole_lowpass`) so the loader resolves them to
the cartridge path.** The display names are correct. The 20 WORKHORSE xml_* are
correct. Fix the 9 RUNTIME stems (only the stems, not the names), then the roster
is final.

## Loader — already wired, do not redo

- `plugin/source/dsp/TrenchRuntimePreset.h` — `parseRuntimePreset`,
  `isRuntimePresetJson`, `isRuntimePresetFile`, `bankForRate`. Complete.
- `PluginProcessor.cpp` — `loadRuntimePresetForCurrentRate()` (~line 145),
  `prepareToPlay` re-selects the bank on rate change (~line 167), guard routes
  runtime JSON to the cartridge loader (~line 818). Complete.
- `TrenchDspBridge::loadRuntimePresetBank` — FFI `trench_engine_load_runtime_preset`
  (authoredRate must be 44100/48000/96000/192000; size must equal 4×stages×5).

## Cartridge metadata (bat_phaser = Opium)

`X3F_bat_phaser.x3preset.json`: name "Bat Phaser", stem bat_phaser,
active_stages 2, datum_rate 0, controls Freq/Res, provenance = verbatim
EmulatorX.dll bytes, codec null < 0.01 dB per bank, 33×33 certify per bank.

## Provenance facts

- The 17 X3F runtime cartridges = verbatim ROM (`ref/x3_menu/runtime_blocks/`),
  68 banks byte-identical, 0 mismatches.
- The `cleanroom_*.body240` = textbook analytic curves, single-rate, sharp at
  48k / octave out at 96k. Retired 2026-08-05. Kept on disk as clean-room
  fallback, NOT in the roster.
- The `xml_*.body240` = decoded ROM XML templates (Morph Designer, 71 decoded
  dossiers in `dossiers/templates/decoded/`), 20 shipped under renamed names.
- The 14 `x3_shape_*` + 31 CUT per `docs/CANONICAL_SHIP_LIST.md` — out.

## Not to touch

- The 8 character bodies (`bodies/candidates/*.trenchsrc`, the hero rail:
  Sinkhole, Tadpole, Drive Thru, Speaker Knockerz, Fire Escape, Nosebleed,
  Basement, + `bodies/candidates/*.body240`) — these are in trench-filter-list,
  a separate worktree. Their roster work is separate.
- The loader code — it's correct and complete.
- Names are approved; do not rename.

## Build / verify

- `python tools/x3_fundamentals_to_cartridges.py` regenerates the 17 cartridges.
- Build: `just build` (or the ship-vst3 skill / `tools/build_workstation_exe.ps1`).
- After the roster fix, verify: 29 entries, all 20 xml_*.body240 resolve,
  all 9 X3F_*.x3preset.json exist.
