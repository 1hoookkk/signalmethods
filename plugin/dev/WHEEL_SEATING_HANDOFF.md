# Wheel seating + shadow — start here

The wheel ART is finished and correct. **Do not touch the strip, the crown, the
frame width, or Blender.** The only job is making the drum SIT IN the well.

## The target

Tyson's screenshots, in `C:\Users\hooki\OneDrive\Pictures\Screenshots\`:

- **`Screenshot 2026-07-24 215207.png`** ← the one to match
- `Screenshot 2026-07-24 220515.png`
- `Screenshot 2026-07-21 142538.png` (labelled: top = "OLD gunmetal (crown,
  bright edges)", which is the accepted read)
- `Screenshot 2026-07-21 133553.png`

In those, a hard dark shadow runs along the **top inside edge of the well** and
the drum is nested down under it. In the current build there is no ceiling
shadow at all, so the drum reads pasted onto the plate.

## What is already correct — leave alone

| | |
|---|---|
| `plugin/assets/trench_roller_strip.png` | md5 `601ff6cd` — the ss3steel drum with the lamp retinted to mint `#66DBB8` (the face's own trace colour) |
| source strip | `trench_roller_strip.png.bak_ss3steel_orig`, md5 `e46582ef` — the strip that made `faceshot_ss3_gunmetal` |
| `WheelControl.h` | `kStripFrameWidth = 417`, crown restored verbatim from df2-workstation commit `1ae562bb` |
| verification | wheel-band diff vs `faceshot_ss3_gunmetal/trench_face_150.png` = **10.81** |

Both files are modified vs HEAD. Backups of every earlier strip are in
`plugin/assets/`, including `…bak_ss3_417_20260805_2300` (the tinted matte one
that was shipping before this session).

## The two shadow levers

**1. `drawWheelContactShadow()` in `plugin/source/ui/FaceplateView.h` (~line 96).**
Already active for both wells, but its numbers have drifted from the locked
07-17 face:

| | current | `1ae562bb` (locked) |
|---|---|---|
| `castH` | 7.0 | **9.0** |
| Y overlap | 1.5 (pulls the shadow up inside the well) | **0** (starts at `well.getBottom()`) |
| `rx` | width × 0.48 | width × **0.46** |

Gradient is the same in both: black 0.74 at the contact line → 0.50 @ 0.5 →
0.18 @ 0.82, radial, squashed into a short oval.

**2. The ceiling shadow was never applied to the plate.**
`WHEEL_SUCCESS_REPORT_2026-07-21.md` §"Still open" names the lever as the well
recess in `plugin/assets/df2_panel_beige.png`, and lists a
"darkened bottom-lip contact shadow + deeper ceiling shadow" candidate at
`scratchpad/panel_wellshadow_candidate.png`.

**Verified: it was never applied.** `df2_panel_beige.png` and
`df2_panel_beige.png.bak_pre_wellshadow` are both md5 `7605d049` — byte
identical — and the candidate file no longer exists. This is almost certainly
the missing piece for the screenshots above.

## Measured geometry — do not re-derive

Panel art `df2_panel_beige.png` is 828×1280; the face is 326×503. Layout source
space is a *different* 1010×1557 (`kPanelSourceWidth/Height` in `Theme.h`) — do
not confuse them.

Wells, measured off the panel art, converted to face px:

```
opening x   97..445  ->  38.2..175.2   centre 106.70   w 137.4
morph  y  562..627   -> 220.8..246.4   centre 233.62   h  25.6
q      y  698..764   -> 274.3..300.2   centre 287.26   h  25.9
```

Seating law (`WHEEL_SUCCESS_REPORT_2026-07-21.md` §CORRECT FRAMING): the frame
draws 1:1 centred in its component, so correct seating = frame centre ON the
opening centre. ~2.5px of **symmetric** spill tucks under the bevel and is
invisible; **asymmetric** spill is what reads as unseated. Both wheels currently
land within 0.6px of their opening centres — the framing is right, so the
problem is shadow, not position.

## Build and look — the fast loop

There is no need to run the full FaceShot suite (it takes minutes on KEY
detection). `TRENCH_MOD_ITER=1` returns early and writes three face PNGs:

```bash
cmake --build plugin/build --config Release --target TRENCH_FaceShot
TRENCH_MOD_ITER=1 "plugin/build/TRENCH_FaceShot_artefacts/Release/TRENCH_FaceShot.exe"
# -> trench_mod_off.png (326x503, 1x). Crop (30,212)-(190,310) for the wheel bay.
```

Judge at 6x NEAREST on the crop. A/B against
`C:\Users\hooki\df2-workstation\dev\tmp\wheel_grind\codex_fix\faceshot_ss3_gunmetal\trench_face_150.png`
(that shot is 1.5x — downscale it to 326×503 before diffing).

## Rules

- **No `git stash / reset / checkout / restore / clean` in this repo.** A reset
  wiped a day of uncommitted work. Read-only git only. Spare worktree at
  `C:\Users\hooki\trench-filter-list` on `feat/filter-list`.
- Another agent owns `PluginProcessor.{h,cpp}`, `dsp/TrenchDspBridge.h`. Off
  limits entirely: `PresetRoster.inc`, `TrenchRuntimePreset.h`,
  `plugin/presets/bodies/X3F_*.json`, `plugin/CMakeLists.txt`.
- Back up any asset before overwriting it (`.bak_<what>_<date>` alongside).

## Traps this session hit — don't repeat them

- **Baking the strip down to 139×31 and drawing 1:1 was rejected by eye** even
  though it measures marginally sharper. The 417 supersampled frame minified
  live is the accepted look.
- **Adding a halo/corner shadow into the strip's own alpha makes it worse** — it
  reads as a cut-out pasted on the plate. The shadow belongs on the PLATE, under
  the wheel, not wrapped around the silhouette.
- **The crown is code, not art** (`WheelControl.h`). The later "grey line in the
  middle" kill is superseded — Tyson's verdict 2026-08-05 is "it needs the
  crown".
- Don't chase the belly highlight in Blender. It is already in the shipped
  strip.
- A scratchpad file named `inspect.py` shadows Python's stdlib `inspect` and
  breaks numpy. Don't name one that.

## Loose end

The Blender session (`wheel_variant_crisp_graphite_v1_backup_20260721_175455.blend`,
open via MCP on port 9876) still holds **unsaved** edits from this session:
`X3_TiltRoot` Y scale 0.4→1.0, key light swapped SUN→AREA strip, and
`GraphiteClean` set to base 0.092 / metallic 0.615 / coat 0. The shipped strip
does **not** depend on any of it. Tyson's call whether to undo or close without
saving — do not revert it for him, that would also discard anything he had
unsaved before.
