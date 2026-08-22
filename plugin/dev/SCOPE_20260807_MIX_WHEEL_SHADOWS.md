# Scope — MIX wheel vs BITMAP4386_2, and E-MU shadow grammar

Measured 2026-08-07. Every number below is from pixel measurement, not eye.
Assets verified at session-start state first (md5 1453dd59 / 5eb1b92e — match).

## 1. The target: BITMAP4386_2 vertical rollers

78x61 crop, two vertical toothed rollers at right, each ~12px wide, 46px tall.
(Note: the 2026-08-07 handoff says "no vertical roller anywhere in the dump" —
this bitmap is two of them. That claim is dead.)

Measured profile (col x=54 and x=68, both rollers agree):

| property | target |
|---|---|
| tooth pitch | 2px hard alternation |
| gap rows | 0–24 — essentially BLACK |
| tooth rows | ramp 72 → peak 128 → fade → 0 |
| specular band | ~35% from top (y20–26 of 46) |
| bottom quarter | rolls to true black (teeth 24 → 0) |
| tooth:gap contrast at peak | 128:8 ≈ 16:1 (mid-roller still 11:1) |

Around the roller: **no cast shadow at all.** Plate beside it is full
brightness (115 vs 110 baseline); below it, 1–2 rows dip ~10%. The darkness
that makes it read deep is inside the sprite — black gaps and the bottom
rolling off to nothing — not painted on the plate.

## 2. Ours: thin_wheel_strip.png (frame 0, 12x94)

| property | ours | target |
|---|---|---|
| tooth pitch | 2px ✓ | 2px |
| gap rows | grey 23–34 | black 0–24 |
| specular peak | top row (y4–9) | ~1/3 down |
| bottom | plateaus at grey 27–52 | rolls to 0 |
| contrast at peak | 107:23 ≈ 4.6:1 | 16:1 |
| contrast mid | 2:1 | 11:1 |

Why it looks worse, in order: **gaps aren't black** (mushy), **teeth never
reach black at the bottom** (flat/plastic, no roundness), **specular in the
wrong place** (top edge, not the tangent band). Geometry is fine.

Fix is the ASSET, not paint — same move as `darken_wheel_material.py` on the
big wheel: a source-level regrade of the existing strip (crush gaps to black,
move the specular envelope down to ~1/3, roll the bottom quarter to black).
No regeneration, no procedural teeth.

## 3. E-MU shadow grammar — measured

Sources: BITMAP4386_2 pills, `x3_wheel_seat_effect_mask.png` +
`x3_control_grammar.json` (df2-workstation/tmp/x3_wireframe/), BITMAP4615.

- **Alpha mask on the plate, never paint in the control.** Grammar json,
  verbatim: "The feathered seat/shadow is an alpha-mask effect, not a hard
  rectangle or baked plate recession." Plate grain survives inside it
  (BITMAP4615, sd 26–29 in vs 9 out).
- **Direction: strictly below.** Light from top. No sideways or diagonal
  offset anywhere in the dump.
- **Tight.** 4386 pills: contact row ~40–50% darker, back to plate baseline
  within 2–3px. E-MU's own seat mask for the 85x16 wheel: ~2px lip above,
  ~8–9px monotonic feather below, zero lateral spread.
- **Small controls cast almost nothing.** The vertical rollers get ≤2px
  below and that's it.

## 4. Where ours is wrong

1. **`ThinWheel.h:96-108`** — the only live code-drawn shadow on the face.
   Four stacked black rounded rects offset RIGHT (+2.0 → +6.8px). Wrong on
   every measured axis: sideways (E-MU: below), ~7px lateral spread (E-MU: 0),
   opaque paint inside the control (kills grain, clipped at bounds — the
   exact failure Theme.h:218 already documents). And the target says a roller
   this size should cast ~nothing. Delete candidate.
2. **Plate has no seat under the wheels/MIX** — the baked ones were reverted
   on verdict ("the shadows too"), which is consistent with E-MU only if the
   sprites themselves carry the darkness. Ours currently don't (see §2) —
   that's why the face reads shadow-wrong: grey floating sprites, no black.
3. **`FaceplateView.h:62-67` comment is stale** — it claims wheel/MIX shadows
   are baked into `df2_panel_beige.png`; the shipping plate is the
   pre-wheelshadow backup (md5 verified). The comment describes a reverted
   state.

## Proposed order (one verdict each)

1. Regrade `thin_wheel_strip.png` at source to the §2 target envelope, and
   delete ThinWheel's code shadow in the same render — they're one look.
2. If a seat is still wanted after that: alpha-mask multiply drawn by
   FaceplateView (it owns the plate, so grain survives, no clipping), E-MU
   proportions — 2px lip above, short feather below, nothing sideways.
