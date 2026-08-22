# X3F shipping presets — handoff

2026-08-04. The focused work: **ship the 17 X3F runtime presets + 71 Morph
Designer XML presets** as certified bodies in the plugin. The X3F bodies are
the real X3 designer presets — verbatim ROM bytes, not fits. The XML bodies
are the Morph Designer user-preset corpus, decoded via the type 1–3 compiler
grammar. Both are the designed identity. The workstation cannot produce these
complex shapes; shipping them is the only path.

## What's built and verified

### 17 X3F runtime-preset bodies

`bodies/candidates/X3F_*.body240` — all 17 packed, certified (33×33 grid),
codec-null proven (0.0000 dB), and probed through the engine's real
interpolation path.

| Preset | Stages | Type |
|---|---|---|
| X3F_2_pole_lowpass | 1 | Replicated pole-ladder |
| X3F_4_pole_lowpass | 2 | Replicated pole-ladder |
| X3F_6_pole_lowpass | 3 | Replicated pole-ladder |
| X3F_2_pole_highpass | 1 | Replicated pole-ladder |
| X3F_4_pole_highpass | 2 | Replicated pole-ladder |
| X3F_2_pole_bandpass | 1 | Replicated pole-ladder |
| X3F_4_pole_bandpass | 2 | Replicated pole-ladder |
| X3F_contrary_bandpass | 1 | Non-conjugate (verbatim) |
| X3F_swept_eq_1_octave | 1 | Swept EQ prototype |
| X3F_swept_eq_2_1_octave | 1 | Swept EQ prototype |
| X3F_swept_eq_3_1_octave | 1 | Swept EQ prototype |
| X3F_phaser_1 | 2 | Pole-riding-zero (moving notch) |
| X3F_phaser_2 | 2 | Pole-riding-zero |
| X3F_bat_phaser | 2 | Dual pole/zero pairs |
| X3F_flanger_lite | 3 | Three distinct designed stages |
| X3F_vocal_ah_ay_ee | 3 | Formant stack (2 formants + carrier) |
| X3F_vocal_oo_ah | 3 | Formant stack |

### Exact pole/zero per stage per corner

Decoded from the verbatim runtime blocks via `trench_stage_roots_from_words_at`.
See `analysis/x3_runtime_pipeline/X3_DATAFLOW.md` §2.1 for the full writer-
function map and `OPEN_QUESTIONS.md` for the gaps.

### The eye-gate surface

- `bodies/candidates/X3F_*_x3f.png` — 17 plates, the simplified active-stage
  layout: stages, signal-so-far build-up, four corners overlaid, pole/zero
  lanes
- `bodies/candidates/X3F_*_inspect.png` — 17 full inspect plates (6-stage view)
- `bodies/candidates/X3F_*_sos_breakdown.csv` — 17 numeric per-corner
  per-section cascade breakdowns

### The plugin

The runtime preset cartridge path exists (`runtime_preset.rs` →
`trench_engine_load_runtime_preset` FFI), but the X3F bodies use the standard
Cartridge path. Plugin build: `plugin/build/TRENCH_artefacts/Release/VST3/`.
Installed to `C:\Program Files\Common Files\VST3\TRENCH.vst3`.

### Tools

- `tools/x3_fundamentals_to_bodies.py` — produces X3F_* bodies from the
  verbatim runtime blocks. Produces 17/17 with codec-null 0.0000 dB and
  certify PASS.
- `tools/x3f_plot.py` — simplified active-stage eye-gate plate for X3F bodies.
- `tools/sos_breakdown.py` — numeric per-corner per-section cascade breakdown.
- `tools/inspect_body.py` — full 6-stage inspect plate (tuned for ROM bodies).
- `tools/ascii_wizard.py` — original hardware front-panel visual for content.
- `tools/x3_runtime_loader.py` — loads runtime presets into the engine via FFI.
- `tools/decode_templates.py` — decodes 71 Morph Designer XMLs via the DLL
  compiler grammar.
- `tools/run_x3_batch.py` — batch pipeline (not used for X3F; the CR_* fit
  path is dead). Use `x3_fundamentals_to_bodies.py` for X3F.

## Architecture: the building blocks

The X3's writer functions compose filters from prototypes:

- **LP/HP/BP** — replicate one prototype stage N times (proven by byte
  comparison — identical S1=S2=S3 words per corner).
- **Phaser/Bat** — pole-riding-zero pairs placed per-stage, each a moving
  notch (pole/zero at near-unity radius).
- **Flanger/Vocals** — genuinely distinct designed stages per slot (no
  replication).
- **Swept EQ** — single-stage prototypes with a second lookup dimension.
- **Contrary Bandpass** — non-conjugate stage geometry, preserved verbatim
  (cannot decompose to simple pole/zero pairs).

## The open gaps (not blocking shipping, but real)

1. **Extraction script missing** — `tools/extract_x3_menu_filters.py` is
   referenced but doesn't exist. The runtime blocks were produced by it in
   df2-workstation. Without it, can't regenerate from a different DLL or
   verify the extraction.
2. **No hardware null** — codec equivalence proven (df2==TRENCH, 0.0000 dB)
   but never nulled against real X3 audio. TB or Not TB null protocol exists.
3. **Generated classes** — 4 filters (Dual EQ Morph, etc.) have NO runtime
   blocks. Writer functions must be reimplemented from Ghidra.
4. **Only 44.1 kHz** — the 48/96/192k rate-family blocks exist but aren't
   processed.
5. **Interpolation axis order** — morph-first vs q-first unproven (affects
   bit-exact mid-interpolation nulls; inaudible for practical use).

## Next session — ship checklist

- [ ] Confirm all 17 X3F_* + 71 XML bodies load in the plugin's cartridge path
- [ ] Clean up `bodies/candidates/` — remove the 14 CR_* fit bodies (dead
  method), keep only X3F_* + XML_* + dvtdfit_*
- [ ] Commit everything cleanly in x3-clean
- [ ] Install fresh VST3 build, test in FL
- [ ] Tyson verdicts on the 17 X3F plates (eyes, not ears — signal-so-far +
  cascade breakdown)

## Repo layout (clean)

```
trench-x3-clean/
├── trench-core/          # The compiler (Rust)
├── plugin/               # The product (JUCE VST3)
├── bodies/candidates/    # X3F_* + XML_* bodies (shipping slate)
├── tools/                # The pipeline (not the product)
├── ref/                  # ROM data, DLL extracts, templates
├── analysis/             # X3 dataflow docs, manifest
├── design/               # Authority + handoff docs
└── scratchpad/           # One-shot probes, not shipped
```
