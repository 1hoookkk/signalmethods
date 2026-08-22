# SLOT GRAMMAR — the reusable six-slot order (measured from the E-mu bank)

Companion to `STAGE_LAW.md` (motion grammar). This is the *ordering* grammar:
what each of the six serial slots does in the factory bodies, measured over
P2k_000–032 on the packed path (evidence:
`evidence/session_20260804/corpus/CORPUS_REPORT.md`,
`analysis/factory_topology_20260804/`, `analysis/family_map_20260804/`).
The cascade is serial — these are correspondence roles, not frequency ranks,
and never per-formant volume slots.

## The three hard laws

1. **SCALE law** — 26/33 bodies carry the *same* SCALE word in all six slots
   of a corner (differing between corners). SCALE is corner-level voicing
   gain distributed across the cascade: author ONE level per corner, never
   six gain knobs.
2. **S6 unit-zero law** — 127/132 factory corner-rows put slot 6's zero on
   the unit circle (r ≥ 0.9995); slots 1–5 never do. S6 is the terminal
   frame stage: hard notch / spectral boundary (often the 20,277 Hz air-cap
   zero — common, not universal).
3. **Lane identity** — stage number is correspondence (STAGE_LAW). S6 stays
   S6 across all four corners; crossings are authored inside a lane, never
   created by re-sorting.

## The slot roles (measured medians, 33 bodies)

| Slot | Role |
|---|---|
| S1 | HF foundation / frame — median pole ~7 kHz; carves the LF/MF floor; widest Morph travel in sweeper families |
| S2 | Low-mid main voice (adds LF body) |
| S3 | Mid inner voice |
| S4 | Upper voice |
| S5 | Colour / detail |
| S6 | LF-lift terminal frame (+15 dB LF contribution) carrying the unit zero |

Per-family weighting of Morph travel across the slots is tabulated in
`analysis/family_map_20260804/FAMILY_MAP.md` (e.g. sweepers ride S1/S6
hardest ~3.5 oct; vowels keep every slot under ~1.5 oct; high-rings freeze
S3–S5 as the pinned ring block).

## Compliance status of our tooling (2026-08-04)

- `tools/make_body.py` / `trim_gain_budget` satisfy the SCALE law by
  construction (verified on VOWEL_UW_AE/UW_IY joint bodies).
- **Lane templates are live (2026-08-04).** `scratchpad/joint_fit.py` holds
  `LANE_TEMPLATES` — per-archetype slot-role priors (`vowel` measured-backed;
  `phaser`/`sweep`/`wah` provisional from STAGE_LAW species + Tyson's lane
  rules). They are used only for correspondence, never as fit targets:
  a template seed joins the joint multistart, and the final solution is
  relabeled into grammar order by one uniform slot permutation across all
  four corners (response-invariant — serial cascade order never changes the
  product; verified bit-equal scores on the UW→AE/UW→IY joint bodies).
- `tools/make_body.py` now runs the joint stage for 2-source bodies too
  (Q axis duplicated) and takes `TRENCH_TEMPLATE=vowel|phaser|sweep|wah`.
  Proven end-to-end: UW→IY 18.2 → 4.8 dB interior, certify PASS, S1 = HF
  frame / S6 = LF terminal in every corner.
- Remaining gap: relabeling files an existing near-unit zero into S6 but
  cannot *create* one — enforcing the S6 unit-zero law inside the
  optimization (soft prior on slot 6's zero radius) is the one open item.
