# REDUNDANCY MAP — 2026-08-07

NEUTRAL TARGET (Tyson 2026-08-07, supersedes the first pass): "all you need to
understand is how the p2k filters work." A file survives only if it is (a) live code
the shipping plugin/pipeline runs, (b) measured data with provenance (dossiers, tf
captures, decoded corners, inspector plots), or (c) the E-mu manual in ref/.
Interpretive doctrine goes regardless of which week it was written — including this
week's frame/S6 thesis docs. Poison claims verified against decoded corner words in
`dossiers/characters/P2k_*.json`, not doc-vs-doc. Bias: delete over preserve.
Git history is the archive.

---

## 1. EXECUTED TODAY (verdicted in session)

- `plugin/presets/bodies/`: **492 files deleted**, 29 remain — exact match to PresetRoster.inc.
- Vocal Dynamic killed: body240 + roster line + HERO comment + candidates copy.
- `bodies/candidates/_superseded_single_rate/` (17 X3F copies) deleted — the half-executed X3F kill, finished.
- 68 X3F sidecar files (`.x3preset.json`, `_inspect.png`, `_sos_breakdown.csv`, `_x3f.png`) deleted from candidates.
- 26 root-level `bodies/*.body240` deleted (none matched roster stems).
- Kept in `bodies/`: `basement_deepbouche/`, `tadpole_303/` (evidence plates of today's law commit), `candidates/` (held — §5).

Verified intact after cull: RUNTIME presets load `X3F_*.json` (CMakeLists.txt:89 bakes them);
external-repo hot-scan is OFF (`kScanAuthoringBodies = false`, TrenchBodyRoster.h:155). Roster law holds.

**Late verdict — this week's doctrine goes too.** Seven docs ordered deleted; my delete was
blocked by the permission classifier, so Tyson runs it (one line, from repo root):
`rm docs/MORPHEUS_INTENT.md docs/REFERENCE_LANES.md docs/MORPHEUS_TYPE_SOURCES.md docs/COMPILE_ORACLE_MAP.md docs/EVIDENCE_2026-08-06_resonance_gap.md docs/BETWEEN_STAGE_ZEROS.md docs/SESSION_2026-08-06_PICKER_AND_BODIES.md`
Citation orphans already scrubbed ahead of it: batch_compiler.py (3 sites, incl. the
blueprint lane_law stamp), extractor.py, workstation BRIEF.md, CLAUDE.md authoring-law line.
The measured facts those docs held (120-frame census, S6 unit zero 132/132, motion stats,
per-character roles) are re-derivable by probe from the binary banks; the compiled behavior
in batch_compiler is unchanged. All seven docs deleted 2026-08-07 late. Also executed same
night: dossiers/ deleted (datum-contaminated derivatives; binary is the source),
plots/inspector regenerated from evidence/emulatorx_binary_filter_rip_20260729/p2k_main
(50 banks @ 44100 Hz — the rip proves banks are 44.1k rate-index 0 and 39062.5 has ZERO
hits in the DLL; every "datum 39062.5" label in surviving files is unverified hearsay),
filters/phrases.json restored to the curated 8-ship version, batch_compiler fixed (encoder
refusal now loud — the silent zero-repark fallback is gone; c_double(sr); Q100 mirror moved
after wire/reserve; narrow-band grid guard). REFUSED on law: per-stage gain scaling
(parallel-EQ move; SCALE is one value per corner).

CLASSIFIER-BLOCKED (needs Tyson to run, one line from repo root):
`Remove-Item 'df2-workstation','.prompts','.playwright-mcp' -Recurse -Force`

---

## 2. KILL LIST (delete on go)

### Repo root
- **`df2-workstation/`** — 198 MB embedded stale clone of the old repo, own `.git`, gitignored, zero inbound refs (every live `df2-workstation` string points at the EXTERNAL `C:\Users\hooki\df2-workstation`).
- `.prompts/` (3 files, duplicated inside df2-workstation, hardcode the external repo), `.playwright-mcp/` (6 browser-era dumps).
- Dead docs: `HANDOFF_20260720_NIGHT.md`, `HANDOFF_20260721_*.md` (×4), `WORKSTATION_HANDOFF.md`, `PRESET_HANDOFF.md`, `SURFACE_FORGE_BUILD_HANDOFF_20260720.md`, `UI_RECOVERY_HANDOFF_20260720.md`, `WHEEL_BLENDER_SESSION_PROMPT_20260721.md`, `kimi_handoff.md`, `kimi_repo_bundle.md`, `session-ses_029d.md`.
- Stale docs: `NEXT_TASK.md`, `PLUGIN_VIEW.md`, `HYPOTHESIS_BRIEF.md`, `SOURCES.md`, `UI_WHEEL_FAILURE_20260720.md` (**fold its wheel ban-list into design/SURFACE_CONTRACT.md first** — dev/HANDOFF_20260807_UI.md calls it required reading).
- Dead scripts (zero live refs, all target the old layout): `annotate_recipes.py`, `build_filter_index.py`, `build_gold_templates.py`, `build_gold_templates2.py`, `consolidate_library.py`, `decode_orphans.py`, `graft_demo.py`, `harvest_presets.py`, `harvest_recipes.py`, `inspect_actors.py`, `stage_cards.py`, `handoff.json`.
- `catalogue.json` + `tools/build_catalogue.py` + `tools/verify_pipeline.py` — stale together (paths point at external repo); delete all three as a unit. `cartridge.schema.json` — no validator reads it.
- Generated debris (gitignored, ~31 MB): `chewsweep_*.png` ×25, `gifsweep_*.png` ×29, `slamsweep_*.png` ×27, `trench_face*.png` ×19, `trench_mod*.png` ×5, `trench_movement_room.png`, `trench_rooms_closed.png`, `trench_source_step_*.png` ×3, `amber_face.png`, `audition_*.wav` ×5.
- `dev/tmp/` (250 MB render exhaust) + JUCE-era proof dirs: `recovery_faceshot_20260720*` ×5, `roller_az90_preview*`, `q0fix_20260722`, `slam_ab_20260722`, loose `wheel_*.png`/`blender_probe_*.png`, `CODEX_WHEEL_PROMPT.md`. Keep: `SCOPE_20260807_*.md`, `HANDOFF_20260807_UI.md`, `WHEEL_SEATING_HANDOFF.md`, `NEXT_AGENT_REVIEW.md`, `font/`, `notebooklm/`.
- Keep: `IP_ENCODING_RECORD.md` (legal artifact — re-date the header), `dossiers/` (decoded ROM ground truth), `analysis/`.

### docs/ (19 → 4 prose + 2 data)
DELETE — this week's doctrine (verdicted, awaiting Tyson's rm, see §1): `MORPHEUS_INTENT.md`, `REFERENCE_LANES.md`, `MORPHEUS_TYPE_SOURCES.md`, `COMPILE_ORACLE_MAP.md`, `EVIDENCE_2026-08-06_resonance_gap.md`, `BETWEEN_STAGE_ZEROS.md`, `SESSION_2026-08-06_PICKER_AND_BODIES.md`.
DELETE — older dead: `DEMO_SHOTLIST.md` (drives deleted SOURCE room), `TRENCH_AUTHORING_HYPOTHESES.md` (frozen-poles + slot roles + family statistics doctrine), `LOAD_BEARING_RAIL.md` (names its own successor).
FOLD + DELETE: `TRENCH_CORPUS_FINDINGS.md` — move §6 Q attitudes into ROM_MUSIC_THEORY (batch_compiler.py:412 cites it), update the citation, delete the rest (duplicate census).
SURGERY (Tyson-named keepers or live-code-cited — trim to measured content): `QUICKSTART.md` (cut 3-room + resample halves), `UX_ACCEPTANCE.md` (cut resample section; release_gate.ps1 reads it), `BODY_PIPELINE_INTENT.md` (§3 below), `BASS_FUNDAMENTALS.md` (cut Voice Role Allocation table), `ROM_MUSIC_THEORY.md` (§3 below).
KEEP: `THE_MOVE.md` (workflow), `P2K_CORPUS_BRIDGE.md` (measured hardware bounds), `LANES_SEQUENTIAL.md` (regenerable data appendix).

### trench-core/
- Dead bins (16 of 22): `tf_harness.rs` (192 KB, not even compiled), `tf_harness_routed.rs` (250 KB), `compose_body.rs` (uncalled rival of make_body), `hd_ab.rs`, `pole_proof.rs`, `bloom_probe.rs`, `env_probe.rs`, `listener_probe.rs`, `slam_alias_probe.rs`, `slam_ab_render.rs`, `armadillo_check.rs`, `corner_contrast.rs`, `hf_flatness.rs`, `morph_path.rs`, `null_render.rs`, `rom_null.rs`, `rate_ab.rs`, `motion_gap_proof.rs`, `motion_take_proof.rs` — plus their `[[bin]]` entries.
- Dead modules: `x3_tables.rs` (never declared in lib.rs — doesn't compile into the crate), `oversample.rs` (only dead bins import it).
- Manifest: `libc` dep (zero uses), `cbindgen.toml` (no build.rs), `src.zip` (tracked stale snapshot).
- LIVE and undocumented: `bin/body_from_geometry.rs` is the compiler boundary for the Python tooling (justfile, filter_cli, author_peaks, translate_sources, test_contract…) — add to CLAUDE.md.
- `batch_compiler.py` verified clean: family-median kill is complete; only the fenced carve-law median remains.

### tools/ (~100 of 137 scripts)
LIVE LOOP (keep): `workstation/`, `batch_compiler.py`, `extractor.py`, `batch_ingest.py`, `armadillo_plot.py`, `q_attitudes.py`, `tf_ingest.py`, `taste_linter.py`, `inspect_body.py`, `build_install_vst3.ps1`, the untracked Blender/asset bakers edited today (`bake_*.py`, `punch_wheel_wells.py`, `rehue_wheel_lamp.py`, `retint_roller_glow.py`, `neutralise_knob_material.py`, `matte_knob_strip.py`, `make_glass.py`, `thinwheel_assemble.py`, `apply_theme.py`, `darken_wheel_material.py`), `phrases.py` (see §4.1).
DELETE as units:
- joint_fit parallel authoring stack (~23): `joint_fit.py`, `make_body.py`, `author_body.py`, `md_author.py`, `path_body.py`, `dual_axis_body.py`, `body_wizard.py`, `foundry.py`, `author_peaks.py`, `corners_from_wavs.py`, `corners_from_notes.py`, `fit_field.py`, `fit_tone.py`, `make_vowel.py`, `vowel_morph.py`, `voice_lead.py`, `recipe_pipeline.py`, `clamp_body_gain.py`, `repack_bodies.py`, `repack_xml_bodies.py`, `x3_morph_compiler.py`, `vocal_dynamic.py`, `ship_v1_trap.py`.
- probe/plot dups of inspect_body: `sos_breakdown.py`, `cascade_scope.py`, `cascade_ladder.py`, `plane_check.py`, `x3f_plot.py`, `prove_master_body.py`, `compare_master_bodies.py`.
- slot-role/lane-law stack: `register_lanes.py`, `test_register_lanes.py`, `route_correspondence.py`, `lane_sheet.py`, `lane_reconcile.py`, `roster_sheet.py`, `stage_decompose.py`, `translate_law.py`, `compile_frame_voice.py`, `gold_frames.py`.
- key-detection ML island (9): `train_key_model.py`, `prepare_key_dataset.py`, `key_benchmark.py`, `test_key_benchmark.py`, `test_key_model_pipeline.py`, `convert_giantsteps.py`, `build_chroma_vectors.py`, `build_fft_chroma_vectors.py`, `augment_chroma.py`.
- Surface-Elites genetic island (7): `evolution.py`, `mutation.py`, `surface_archive.py`, `surface_evaluator.py`, `trenchsrc.py`, `guide_projection.py`, `trench_profile.py`.
- X3 capture island (9): `capture_x3.py`, `run_x3_batch.py`, `x3_dialog_watchdog.py`, `x3_typewalker.py`, `x3_runtime_loader.py`, `decode_templates.py`, `audition_raw_vs_packed.py`, `mine_regions.py`, `clean_room_fixed.py`, plus `x3_fundamentals_to_bodies.py`, `x3_fundamentals_to_cartridges.py`.
- roma_* one-offs (4), literature harvest (6: `harvest_bandwidth_literature.py`, `ingest_academia.py`, `pair_bandwidth_priors.py`, `inspect_references.py`*, `source_catalog.py`, `translate_sources.py`) — *`inspect_references.py` regenerates LANES_SEQUENTIAL: keep it, kill the rest.
- misc zero-ref dead (~20): `ascii_wizard.py`, `filter_cli.py`, `build_filter_roster.py`, `build_shipping_catalogue.py`, `catalogue.py`, `character_dossier.py`, `distance_gate.py`, `recipe_families.py`, `ebl.py`, `engine_null.py`, `nam_render.py`, `table_stitch_gui.py`, `mint_vowel_presets.py`, `make_303_table.py`, `fit_sibilance_gate.py`, `add_detune_variants.py`, `distill_recipe_index.py`, `MAKE BODY.bat`, `make_body_picker.ps1`, `build_workstation_exe.ps1`.
- pairwise dups (keep one or zero): `assemble_glb_wheel_257.py`/`render_glb_wheel_257.py`, `roller_assemble_accent.py`/`roller_assemble_compact.py`, `display_grid_dark.py`/`display_grid_hifi.py`, `bake_funcgen_patterns.py`+`measure_funcgen_phrases.py` (phrases.py subcommand does the job).
- residue: `__pycache__/` (untrack), `praat/`, `iconic_recipes/`, sidecar-era `check_faceplate.py`, `measure_blue.py`, `arrange_ship_rail.py`, `make_sidecar_plate.py`, `fit_sidecar_art.py`.

### filters/ (whole previous generation — two exceptions)
Everything frozen at 2026-07-29 "workstation goes lean", zero live refs. DELETE: `README.md`, `archetypes/`, `docs/` (METHOD, REGISTERED_LANES, SOURCE_CATALOG, PRIOR_ART_LEDGER, REFERENCE — all superseded slot-role/8-lane law), `generator/` (documents a script that doesn't exist), `rails/`, `fundamentals/`, `gates/`, `stage-plans/`, `tables/` (12 of 16 byte-identical to recipes/tables, 5 orphans), `*.schema.json`.
KEEP: **`filters/bodies/`** — hidden hard dependency: trench-core tests read it (`minifloat.rs:341` sweeps the dir; `engine.rs`, `keyframe.rs` read `CAVL_mason_jar_to_stone_pipe.body240`). Move to a fixtures dir or leave. **`filters/phrases.json`** — see §4.1.

### plugin/
- `assets/`: 29 backup files (65 MB), 133 orphaned `frame_*_measured_*.png`, `trench_panel.png` (dev-scratch output), `tone_match_rtneural.json`, `display_log_grid*.png` ×2, `thin_wheel_panel_shadow.png`, `thumbwheel_runtime_strip_1_*.png`, `measured_glow_checkpoints_8x.png`, `Videos - Shortcut.lnk`. Only 7 of 177 assets are referenced.
- `plugin/` root: 73 tracked FaceShot PNGs (16 MB) — gitignore patterns are root-anchored so these got committed; untrack + widen patterns.
- Code: `source/dsp/TrenchCleanBody.h` (zero call sites; drop from PluginProcessor.h:10 + CMake), `source/PluginProcessor.cpp.bak`.
- Presets dir residue: `PresetRoster.inc.bak_pre_xml_trim`, `PresetRosterSignature.inc.bak_pre_xml_trim`, `approved_bodies.txt`, `gain-fix-v2.json`, `gain-fix-v2-verification.json`, `oldschool_hybrids.provenance.md`.
- CMake hygiene: sources list missing six live headers; `project(TRENCH_WorkstationProject)` name stale.

---

## 3. POISON IN THE LOAD-BEARING DOCS (verified vs decoded corners)

The keepers Tyson named — INTENT, MUSIC THEORY, STAGE_LAW, profiles — each carry
claims that contradict measurement. Cut the passage, keep the doc.

**BODY_PIPELINE_INTENT.md**
- CUT "Verified ground truth" median block: "Use per-family medians, never one fixed pair" + the S1 7374/S6 371 + per-family number table. This is the mechanism commit 7183e793 killed for inventing +18.9 dB of low end. Highest-risk text in the repo.
- CUT the epistemics inversion: "the description is the intent… the dossier row is evidence, not an override." Measurement outranks prose (inspector plots are ground truth — Tyson 2026-08-07).
- CUT "never fit at all" (contradicts the proven joint-fit method and INTENT's own Drive Thru recipe).
- CUT the embedded to-do list (§Priorities/Next session — cites the removed `--anchors` flag).
- Fix: three contradictory Q100 defaults in one file; Klatt bandwidth numbers that don't match the repo's own Klatt table.
- KEEP: S6 unit zero 132/132; SCALE closed-form law.

**ROM_MUSIC_THEORY.md**
- CUT §4 + composer rule 3: "when in doubt, author contrary motion" — INVERTED. Re-measured across all 33 bodies, n=451 lane pairs: **63% parallel / 29% contrary / 8% oblique**.
- CUT §1/§2 interval theory + composer rules 1 and 2 ("tune the intervals", "mouth cluster spaced 1–5 st"): adjacent-voice spacing is a smooth random log decay, median 7.0 st, no musical clustering. Matches STAGE_LAW's own "do not snap to notes."
- Reframe §3 anchor/mouth/air as description of some bodies, not architecture law.
- KEEP: the register census, pose-reuse §7 (ancestor of the frame thesis), motion taxonomy definitions (with corrected stats), Q attitudes §6 once folded in.

**design/STAGE_LAW.md**
- CUT "All 360 complete section rows across the corpus are unique" — refuted twelve lines earlier in the same file (Millennium/MeatyGizmo share 12 exact rows) and by the frame library.
- FIX every Hz: all are unlabeled 44.1k readings colliding with the 39062.5 Hz datum. The "20,277.1 Hz air cap" is 17,960.8 Hz at the datum (confirmed most common S6 zero, 48/132). Convert to datum or label the rate at every number.
- CUT the frequency-sorted DeepBouche ladder row (the file commits the exact sort error its own motion grammar forbids; INTENT says descending is the signature).
- Un-conflate air-cap (optional, 26/50) from S6 terminal unit zero (132/132, the one law).
- Drop "permanent" from the header — it blocked this correction.
- KEEP: motion grammar bullets (judge only the whole; correspondence not frequency rank), FAMILY LAW.

**recipes/INTENT.md**
- CUT "reverse the order and it stops being DeepBouche" — serial responses multiply; multiplication commutes; slot order is inaudible. What matters is the pairing. (The repo says so itself: exact_skeletons "products commute".)
- CUT per-stage SCALE authoring instructions (−18 dB sub cut etc.) — SCALE is a per-corner trim, never per-stage.
- CUT 303 "tuned to the note" / "space poles harmonically" (no chromatic tuning exists in the corpus) and the Opium beat-frequency claim (physically wrong).
- FIX Opium "remaining four are identity" — S6 always carries the terminal unit zero.
- KEEP: the header thesis (shared pose vocabulary = frame library, stated correctly) and the stated 39062.5 datum. Repeat "ears are the gate" in each AUTHORING block.

**profiles (`profiles/` + `recipes/profiles/` — byte-identical copies)**
Poison confirmed but CONTAINED: taste_linter.py imports only stdlib+numpy; the only
consumers (author_body → trench_profile → guide_projection) are the dead Surface-Elites island.
The constraints are measured-wrong anyway: global anchor/mouth/air role bands that no measured
family obeys; behaviour whitelists banning the exact Q moves their own reference presets measure
(acid-resonant bans defuse; MegaSweepz S2 −0.66, Sinkhole requires a defuser; eq-shaper bans arm;
BassTracer S6 +0.36); a 3-octave travel gate the ROM's median (1.16–1.21 oct) fails; five unsourced
fitter regularization weights. Do not revive as-is — rebuild from measured constraints or retire.

**recipes/tables/**
- `family_intents.json`: 4% invented frequency jitter; frequency-sort slot alignment; mis-cites Klatt for Peterson-Barney values.
- `vowel_formants.json`: bandwidths attributed to Peterson-Barney, who published none — invented numbers under a measured citation.
- `metallic/membrane/tube` q_guidance bandwidths unsourced (the ratios are properly cited); `radius_hint` hardcodes the ROM datum while the pipeline encodes at real rate.
- `klatt_1980_*.json`, `p2k_filter_q_behavior.json`: provenance is NotebookLM transcription, not the source; `p2k_filter_q_behavior` contains NO deltas despite MORPHEUS_TYPE_SOURCES calling it "measured Q100−Q0 deltas" (one-line fix there).
- Dangling: `klatt_1980_formants.json` warns about `forge/src/main.rs` (doesn't exist); `physical_models.py` docstring cites `build_physical_intents.py` (doesn't exist).
- Models of good practice to keep as-is: `recipe_index_v1.json`, `q_radius_table.json`, `hrtf_pinna_P0001.json`, `fundamentals_manifest.json`, `exact_skeletons.json` (label its note grid as deliberate daylight).

**CLAUDE.md**
- Authoring-law line cites "ROM_MUSIC_THEORY.md: intervallic voicing, contrary/oblique/parallel motion" — both halves now measured-false as stated. Reword to: pose reuse + motion taxonomy with measured priors (63/29/8).
- Add `bin/body_from_geometry.rs` to Where-things-live; note workstation drives the engine via `trench_core.dll` FFI.

---

## 4. REGRESSIONS AND TRAPS (found, not caused by cleanup)

1. **`filters/phrases.json` was clobbered.** Working copy = raw factory import (all 56 `ship:true`, factory names), reverting the 2026-08-04 curation (8 shipped, curated names). `FuncGenPatterns.h` calls this file source of truth — a bake now ships all 56. Fix: `git checkout -- filters/phrases.json`. **Needs verdict — it's a working-tree change I didn't make.**
2. `.vscode/settings.json` points CMake at the external df2-workstation repo.
3. Root-anchored `.gitignore` patterns let `plugin/`-relative FaceShot output get tracked (16 MB) — widen patterns when untracking.
4. `trench_profile.py` docstring claims it "drives the linter" — it never did. `PROFILES_DIR` reads root `profiles/`, not the documented `recipes/profiles/`.
5. **The df2-series hook carries doctrine.** `.claude/hooks/df2_series_law.py` injects "Lane roles (S1 = air, S6 = chest anchor) are locked A PRIORI" — a fixed slot-role claim the dossier measurements contradict (roles are per-character; only S6 unit zero is universal) — and its Reference line cites the deleted COMPILE_ORACLE_MAP. The serial-cascade half of the hook is correct and measured; the slot-role line and the reference need Tyson's verdict to fix, since the hook is enforcement machinery.

---

## 5. HELD FOR VERDICT

- **`bodies/candidates/` (~90 files after today's cull)** — strict reading of "only the plugin bodies are correct, delete the rest" kills these too. Held because it includes TODAY'S uncommitted work: `MD_bass_shaper.body240`, `MOTH.body240` (re-authored, modified in working tree) and workstation scratch written tonight (`picked_qt`, `oui_relay`, `qmove_proof`, `picked`). Say the word and it all goes.
- `filters/bodies/` (132 bodies) — trench-core test fixture. Move into trench-core or accept `filters/` surviving as a fixtures shell.
- `evidence/` (76 MB) — prune to the runs cited by SLOT_GRAMMAR/design docs?
- `release_gate.ps1` + `run_pluginval.ps1` + `bin/pluginval.exe` — the old release ritual; UX_ACCEPTANCE trim decides its fate.
- Untracked in-flight Rust: `carve_ab.rs`+`tpt.rs` (today's TPT experiment), `gate.rs` (declared, zero consumers). Yours from today — keep or kill.
- `verdict.py` — possibly the machine gate that killed the X3F bodies; unclear if still wanted.
- Root `profiles/` + `recipes/profiles/` — you named profiles load-bearing; both copies carry the measured-wrong constraints (§3). Rebuild or retire.

## 6. DOCTRINE → WORKFLOW (law becomes executable)

| Law | Enforcement point |
|---|---|
| S6 terminal unit zero (132/132) | batch_compiler already terminates; add hard-fail axiom in taste_linter (S6 zero r = 1.0) |
| Motion priors 63/29/8 parallel/contrary/oblique | correct taste_linter motion axiom priors; delete "author contrary" prose |
| SCALE = per-corner trim, one value across stages | taste_linter advisory check (26/33 measured) |
| Serial cascade / judge only the whole | inspect_body.py plots ARE the ground truth (Tyson law); linter certifies whole cascade only |
| Rate-datum discipline | every Hz in docs/tools output carries its rate; STAGE_LAW constants converted to 39062.5 datum |
| Frames + pairing, no slot roles, no medians | batch_compiler enforces (verified clean); ground truth = dossiers + inspector plots, no doctrine doc |
| No note-snapping / no interval theory | prose deleted; nothing to enforce |
| Ears are the shipping gate | stays human; stated once at the top of each surviving law doc |

End state: a doc survives only as (a) the citation behind an executable check, or (b) measured data.
Everything else is deleted, and this map file dies once executed.
