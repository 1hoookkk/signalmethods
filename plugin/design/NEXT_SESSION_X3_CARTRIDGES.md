# Next session: ship the X3 designer cartridges as-is

2026-08-04. The focused work: ship the **real designed presets** — the ones that
came from the ROM recipe, never from fit-from-capture — in their **own cartridge
forms**, not forced into body240.

## The two designed formats (both recipe, both proven)

| Format | Count | Source | Proven path |
|---|---|---|---|
| **Runtime presets** | 17 | `ref/x3_menu/runtime_blocks/` (68 blocks: 17 × 4 rate banks) | `runtime_preset.rs` → `Cartridge` (datum 0, verbatim), FFI `trench_engine_load_runtime_preset` |
| **XML Morph Designer** | 71 | `ref/x3_morph_designer/templates/*.xml` | `tools/decode_templates.py` (DLL compiler oracle) → decoded dossiers `dossiers/templates/decoded/` |

**Key law: the four corners of a runtime block do NOT map to body240's MORPH×Q
corners.** The runtime preset is a **separate cartridge** — the X3's own control
states (Freq/Res per the X3 UI). Never convert runtime presets into body240;
ship them in their own cartridge form.

## What's verified

- `RuntimePresetManifest` loads (17 presets), each with `active_stages` (1-3),
  `read_words()` per rate, control labels.
- `load_runtime_preset` FFI returns rc=0 (engine accepts a runtime cartridge).
- 71 XMLs decoded into dossiers (the DLL-compiler path, proven).
- Morph Designer has **NO Q axis**: Q100 corners are copies of M0/M100 (the DLL
  writes both banks from the same endpoints). Runtime presets' Freq/Res map to
  the body's two axes per the X3 UI.

## The ship work (in order)

1. **Certify all 17 runtime presets** — for each: load at 44.1k, run the 33×33
   cert grid, record max pole radius + stability. Any that FAIL = the "not
   properly implemented" ones — the focus. Report pass/fail (names only).
2. **Certify all 71 XML dossiers** — compile each to a cartridge, cert grid,
   pass/fail.
3. **Produce inspect plates + SOS breakdowns** for the survivors (the eye-gate
   surface: signal-so-far build-up + pole/zero lanes + corner overlaid curves
   + numeric per-section cascade).
4. **Ship as-is** — runtime presets via their cartridge path, XMLs via theirs.
   Never body240. Bodies stay verbatim.

## The law (from the session)

- **Read the recipe, don't fit the ride** — decoded bytes beat fitted
  impressions (proven: TB nulls against hardware).
- Fit-from-capture is DEAD — it already failed ears once; the 14 CR_* fits are
  dead on the method, one strike.
- The four corners of a runtime block ≠ body240 corners. Separate cartridge.
- Morph Designer XMLs have no Q axis — Q100 = copies.
- Eyes gate it: signal-so-far + cascade breakdown, not ears.
- No invented data. Verbatim ROM words only.

## Handoff note

This work is the reason to use `claude-native` for the engineering thread (the
plugin/launcher work) while this cartridge work stays focused here. Both
designed formats ship as-is; nothing gets forced into body240.
