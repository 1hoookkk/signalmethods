# Corner Workstation — evidence audit (2026-08-10)

Boundary-reset audit for the four-corner registered-lane pivot. Nothing here
was moved, fixed, or deleted; this file records what exists, what its
provenance status is, and where the route is broken. Every dataset is
classified against the canonical chain:

```
READ-ONLY SOURCES -> SOURCE OBSERVATIONS -> FOUR REGISTERED CORNERS
  -> RESOLVED GEOMETRY -> TEMPORARY PACKED PROOF -> OPERATOR KEEP/KILL
```

## 1. Datasets

### ETL vowels (Mokhtari & Tanaka 2000)
- `recipes/tables/academia/etl_mokhtari_tanaka_2000.json` — 2750 objects,
  verified programmatically: 5 speakers (S0001, S0003, S0010, S0015, S0041)
  x 5 vowels (a, e, i, o, uu) x 22 words x 5 consecutive steady-state
  frames = 550 five-frame utterances. Hierarchy speaker > vowel > word >
  frame is encoded in the object_id (`ETL_S0001_i_w01_f3`).
- Every object carries exactly F1–F4, each with frequency_hz AND
  bandwidth_hz. **Both are measured** (the table's measurement_method says
  so; the note calls it "the only vowel set here carrying measured
  bandwidths").
- Gap: `f0_hz` is null for all 2750 objects.
- 2750 pre-compiled atoms exist at `bodies/atoms/etl_mokhtari_tanaka_2000/`
  (1:1 with objects). These are body artifacts, NOT source evidence.

### Bandwidth provenance — the priors run the other way
- `hillenbrand_1995.json` (1668 objects) and `peterson_barney_1952.json`
  (1520 objects): F1–F3, `bandwidth_hz: null` throughout. Frequencies
  measured; bandwidths absent.
- `h95_paired.json` / `pb52_paired.json` (8 objects each): bandwidths are
  **AUTHORED PRIORS** = median ETL measured bandwidth, labelled as such
  in-file ("never Klatt, never implied measured"). These must badge as
  `authored_prior` and may never display as measured bandwidth.

### TB303 (`recipes/tables/tb303_acid_sweep.json`)
- Two endpoint objects (`tb303_m0_squelch`, `tb303_m100_open`), 4 lanes
  each, with per-row `provenance` already explicit:
  - **Measured (1 row per endpoint):** L4 — the −3 dB width of the harmonic
    envelope peak (m0: 709.1 Hz / bw 621.6; m100: 3808.4 Hz / bw 1565.4).
  - **Derived (3 rows per endpoint):** L1–L3 — harmonic N of the measured
    root; bandwidth = f/Q with a single measured Q scalar per endpoint
    (1.141 at m0, 2.433 at m100). Badge: `derived_from_q`.
- **Raw window WAVs are MISSING.** `source_window` names
  `m0_303_squelch.wav` / `m100_303_open.wav`; neither exists anywhere in
  the repo, nor does the cited musicradar acid-samples zip
  (was `C:\Users\hooki\Downloads\`, per docs/BODY_PIPELINE_INTENT.md).
  `wav-source-library/` contains no audio at all (README + a PS1 script).
  The table is honest but **unreproducible from in-repo material**.

### WAH
- **There is no P2K WAH donor.** None of the 33 P2K characters is a wah;
  nothing matching "wah" exists in dossiers/, recipes/architectures/, or
  plots/inspector/.
- WAH material is exclusively the Emulator X heritage line: three vendor
  XML endpoint templates (`ref/heritage/Wah Wah {1,2,3}.xml`, mirrored in
  `ref/x3_morph_designer/templates/`, with parity fixtures under
  `trench-core/tests/heritage/`).
- No measured pedal source exists. WAH authoring is therefore
  **donor-only** until controlled pedal captures land.

### HRTF (`recipes/hrtf/`)
All SOFA sources present: aalto_laser_spark, hannover_kemar_nearfield,
oldenburg_mmhr, sadie_ii (doubly nested zip layout), sonicom P0001
(quadruply nested, raw + free-field-comp + synthetic + head meshes).

### DVTD (`recipes/vocal/dvtd/`)
44 measured **complex** transfer functions: 2 subjects x 22 articulations,
one `*-vvtf-measured.txt` each (freq / magnitude / phase_rad, 20806 rows,
~0.96 Hz bins). Duplicate zip under `recipes/holy_sources/vocal/`. The
"calculated" DVTD variants mentioned by translate_sources' docstring are
not vendored (measured only). Badge: `measured_complex_tf`.

## 2. Frequency / bandwidth provenance summary

| dataset | frequency | bandwidth | badge |
|---|---|---|---|
| ETL 2750 | measured | measured | measured_same_source |
| H95 / PB52 base | measured | absent (null) | missing (bw) |
| h95_paired / pb52_paired | measured | authored prior (ETL median) | authored_prior |
| TB303 L4 | measured | measured | measured_same_source |
| TB303 L1–L3 | derived (harmonic ladder) | derived (f/Q) | derived_from_q |
| DVTD 44 | measured complex TF | implicit in complex fit | measured_complex_tf |
| HRTF SOFA | measured HRIR | implicit in complex fit | measured_complex_tf |
| Heritage / P2K | reference only | reference only | reference_donor |

## 3. Tooling route breaks

- `tools/translate_sources.py` broken defaults:
  - `DEFAULT_OBJECTS` -> `wav-source-library/measured_objects/ir_library`
    — **does not exist**; the real IR library is at
    `recipes/measured_objects/ir_library/`.
  - `FITTER` -> `target/release/fit-complex-candidates.exe` — **not
    built**; every fit path fails until it is.
  - All other defaults point into the sibling repo
    `C:\Users\hooki\trench-filters\data\` which duplicates data now
    vendored under `recipes/` — **dual-authority hazard**. Resolution: the
    workstation passes explicit `recipes/` roots as arguments; the catalog
    is the path authority, not module constants.
- Stale paths in documents (flagged, not fixed):
  - `design/STAGE_LAW.md` names `df2-workstation/ref/presets/` and
    `ref/p2k_variants` — neither resolves from this repo root (the variant
    bins live at `evidence/emulatorx_binary_filter_rip_20260729/`).
  - `catalogue.json` ROM_GOLD entries point at
    `surface-forge/out/foundry/` — no such tree in this repo.
  - `ref/heritage/heritage_designer_sections.json` has a mojibake byte in
    its `source` string.

## 4. The two-route conflict (why this pivot exists)

- **Bench route (legacy):** workstation `Session.replan` ->
  `batch_compiler.plan_body` -> seat/wire grammar `read_back` ->
  `pack_body`. Its blueprint marks `q_corners.derived: true`.
- **Disciplined route (canonical):** translate_sources evidence -> source
  catalog -> `register_lanes` validate/emit -> `filter_cli pack` ->
  trench-core. Its schema **rejects** derived Q100
  (`secondary_axis.authored` must be true) and refuses `study_reference`
  catalog records as numeric evidence.
- The two share **no bridge** and disjoint vocabularies (seats/wires/Q
  attitudes vs lane_id/law/topology/provenance). The pivot makes the
  registered-lanes route the session model and fences the bench route
  behind `tools/legacy`.

### P2K bit-exact reuse vs reference-only boundary
The reference pose library (dossiers, P2K bins, heritage XML, inspector
artifacts) demonstrably supports bit-exact reuse — the ROM itself shipped
pairings of a small pose library (MORPHEUS_INTENT, ROM_MUSIC_THEORY §7).
The clean-source rules forbid copying those bytes. **Resolution:** P2K /
heritage material lives on a DONOR shelf as `study_reference` records;
exactly four named transfer operations may cross the boundary —
normalized motion, lane permutation, zero topology/relative placement,
Q posture — each stamping field-level provenance. Absolute roots, packed
words, and coefficients never cross.

### Radius ceilings are layered, not inconsistent
analysis priors (pole 0.9985 / zero 0.995) < bench runtime clamp (0.9995)
< authoring contract POLE_R_MAX (0.9999) < geometry zero_r (1.0, unit
zeros are legal). Each layer is a tighter gate closer to the evidence; do
not unify them.

## 5. Document status (descriptive vs binding)

| document | status |
|---|---|
| filters/registered-lanes.schema.json + tools/register_lanes.py | **binding** — the authoring contract |
| filters/docs/SOURCE_CATALOG.md | **binding** — evidence boundary + record-type gates |
| design/STAGE_LAW.md | binding for lane-correspondence + Q-posture law; its corpus statistics are descriptive; its source paths are stale |
| docs/MORPHEUS_INTENT.md | descriptive study of the ROM; ground truth for the OTE/ETA pairing evidence (L50, 63–70) |
| docs/ROM_MUSIC_THEORY.md | descriptive study; composer rules are guidance, not gates |
| recipes/INTENT.md | authored recipe intent (creative brief, not a validator) |
| tools/workstation/BRIEF.md pre-2026-08-10 body | superseded where it names batch_compiler.plan_body as "the one planning path" — see CANONICAL ROUTE section |

## 6. Guardrail provenance (operator directive 2026-08-10: no invented guardrails)

Every restriction on the canonical path, sorted into: (a) runtime/encoding
constraint, (b) verdicted product policy, (c) invented — removed or flagged.

| restriction | where | verdict |
|---|---|---|
| conjugate-only at emit (real_pair refused) | register_lanes emit_geometry | (a) encoding — .body240 words encode conjugate hz/r pairs; a real pair is unrepresentable, projection would silently lie |
| pole_r <= 0.9999, hz <= 19531.25, scale (0,4] | schema + validate | (a) runtime contract (stage_law.rs minifloat range, Nyquist at 39062.5) |
| derived Q100 rejected; link is authored copy | schema + corner_session | (b) verdicted — Q100 is a second authored pose (STAGE_LAW: Q is a posture) |
| measured-bandwidth-only admission | load_object_rows | (b) verdicted (Tyson 2026-08-07 admission rule) |
| medoid displayed, never auto-selected | sources.medoid | (b) briefed — operator picks endpoints |
| donor material refused as observation | sources._refuse_donor + catalog gate | (b) briefed boundary — reference_donor shelf only |
| max 6 lane pairings | plan_from_two_sources | (a) six-section machine |
| pairing row reuse forbidden | plan_from_two_sources | **(c) INVENTED — removed 2026-08-10.** ROM reuses poses freely; paravowels run 5 resonances over 6 sections |
| S6 terminal lock | legacy session.py only | (c)-adjacent: 132/132 unit zero is descriptive corpus evidence, not law. NOT carried into the canonical path |
| "no crossings" | nowhere | confirmed absent; STAGE_LAW says crossings are intentional. Must never be added |
| profiler bands 55–10500, radii 0.9985/0.995 | schema analysis block | optional — enforced only when the analysis block is authored; conservative, does not block the path |
| continuity DEFAULT_LIMITS (3 oct / 0.5 r / 24 dB) | new_document defaults | unproven defaults, nullable per lane — flagged, not law; corpus shows 2-oct throws are real |

## 7. Open evidence gaps

1. TB303: the raw sweep (AM_OneNoteSweepDry_120A.wav) was recovered from
   Downloads on 2026-08-10 and vendored at wav-source-library/tb303/ with
   sha256 (catalog record tb303_sweep_raw). Remaining gap: the two window
   cut points (m0_303_squelch / m100_303_open) are unrecorded, so the
   table is source-complete but not yet cut-point-reproducible.
2. `fit-complex-candidates.exe` not built — DVTD/HRTF fits blocked.
3. Dual-authority data roots (trench-filters vs recipes/) — catalog must
   become the single path authority.
4. ETL f0 null throughout (frames usable for F/B; no pitch evidence).
5. DVTD calculated tables not vendored.
6. No measured WAH pedal source (donor-only family until captured).
