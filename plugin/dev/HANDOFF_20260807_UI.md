# UI session handoff — 2026-08-07

Ship day. Built and installed to `C:\Program Files\Common Files\VST3\TRENCH.vst3`.
**FL must be restarted** (DLL cache). Old builds parked alongside as
`TRENCH.vst3.inuse-old-<timestamp>` — those are the rollbacks.

Runtime proofs all pass: `SOURCE STEP`, `RIGHT-CLICK LAW`, `KEY MODEL`.

---

## READ THIS BEFORE TOUCHING THE WHEEL

`UI_WHEEL_FAILURE_20260720.md` is not optional reading. It rejects, in writing:
native 85x16 frames, `lowResamplingQuality`, removing the software crown, and
**"a procedural tooth field"**. I rebuilt a procedural tooth field today without
reading it first and it was killed on sight. Do not repeat that.

**THE GLB IS SACRED** (Tyson, today). The wheel art descends from it. Do not
regenerate the strip.

**The wheel is at its session-start state.** Verify before assuming otherwise:

```
plugin/assets/trench_roller_strip.png == .bak_pre_x3language_20260807   (md5 1453dd59)
plugin/assets/df2_panel_beige.png     == .bak_pre_wheelshadow_20260807  (md5 5eb1b92e)
```

---

## What shipped

- Wheel material darkened at source, crown re-lit, lamp retinted to `curveColour`
  (`tools/darken_wheel_material.py`, `tools/retint_roller_glow.py`). Approved.
- Knob strip de-browned at source, load-time gamma deleted
  (`tools/neutralise_knob_material.py`). R-B `+7.0 -> -5.4`.
- Bay captions 12pt uppercase; knobs 34->36 in 42px rows (they overlapped by the
  width of their own baked shadow); bay moved up 12 into dead plate.
- Room selector: plain dropdown, THREE positions — OFF / GAIN / MOVEMENT. The
  carve only draws when a room is open.
- PRESET (was SOURCE): full-width header, prev/next steppers, mouse wheel,
  name auto-sized to fit ("1 Oct Random - Chromatic" is 24 chars).
- Right-click = host automation on BODY / KEY / PRESET. It used to fall through
  and open the body browser.
- Glass: trace supersampled 3x into a cached buffer, grid snapped to pixel
  centres at 1.0px (was 0.55/0.7px — a sub-pixel stroke can never be crisp).

## Deleted

`ModulationChip.h`, `ReentryChip.h`, `MotionBox` (in `SectionRail.h`), the whole
dead canvas-drag subsystem in `GraphDisplay`, the SLAM cursor cue, the MIX
readout. All were unreachable, invisible, or duplicated something else.

Tyson: **"don't be afraid to DELETE. anything gone off the plate is good for
the product."**

## Reverted on verdict — do not silently restore

- Wheel geometry (144 wide, seat drop/shift, widened component rects) — "the
  wheels are off".
- Baked plate shadows under the wheels and MIX — "the shadows too".
- Trace width 1.25 -> **1.1**.
- The readout contact shadow. See below, it matters.

---

## Open, and Tyson's to call

1. **326 or 352.** `Theme.h` says `kEditorWidth = 326`; Tyson said 352. Every
   type size today was judged at 326. Unresolved.
2. **FOLLOW.** Recommended for deletion, never confirmed, so it shipped.
3. **KEY** — pick a worm (below).
4. The OFF state is very bare. Nobody has eyeballed it properly.

## Findings worth acting on — traced, not guessed

**FOLLOW moves the wheels and the wheels do not move.** `engine.rs:999` adds
`env.offset()` to morph and `env.q_offset()` to q every control block. The core
exposes `env_puck()` — its comment says *"what the UI would draw"* — and the
plugin **never reads it** (zero hits in `plugin/source`). DEPTH moves MORPH and
the wheel travels, because that path reports via `effectiveMorphForUi`. Two
controls, one wheel, one of them invisible. This is why the wheels feel wrong.

**FOLLOW is not gated by `modOn`.** It runs with the preset OFF, yet sits under
the PRESET header. It is an auto-wah, and it has nothing to do with the
logarithmic morph — that warp lives in the encoding.

**Modulation is transport-locked.** `MorphMod.h:153`:
`hostLocked = synced && playing && ppq >= 0.0`. Stepping a PRESET arms SYNC, and
SYNC only advances off host `ppq`. Transport stopped = nothing moves, with
nothing on the face saying so. Free-run was buried 2026-07-27.

**BITE is sectional.** It sets pole-radius distortion depth and **MORPH selects
which of the six sections it hits**, crossfading as it walks. Inert on the
identity body. Nothing communicates this.

**KEY, four problems.** (1) Snap moves a pole by at most 1 semitone / 5.9% in Hz,
so it is musically meaningful only at high Q, and nothing couples them.
(2) `kFftOrder = 17` -> 5.94s per window, x3 windows = ~18s before it suggests.
(3) Detection only runs while the editor is open. (4) `KeySnapBox::hitTest`
returns false without a suggestion, so it is often not clickable.

**Shadows must be a mask on the plate, not paint in the control.** E-MU's own
grammar file: *"the feathered seat/shadow is an alpha-mask effect, not a hard
rectangle or baked plate recession."* Measured on `BITMAP4615`, their plate grain
survives inside the shadow (sd 26-29 in, 9 out) — it darkens the plate, it does
not cover it. And a control cannot draw its own: JUCE clips painting to component
bounds and `ValueReadout` never opts out, so anything cast from inside is sliced
off at the edge. That is why my attempt read hard and opaque.

## The E-MU bitmap dump — 288 files, worth knowing

`C:\Users\hooki\do-it\emu-x3-bitmap-dump\`

- `BITMAP4330/4331/4332_1` — **three** rollers, all 129 frames of 85px. 4330 is
  the dark cut, 4331 the lit one, 4332 the **thin** one at 85x11. One wheel,
  three cuts. The thin cut keeps its rails and gives up the channel.
- `BITMAP4602/4209/4470/4400/4800/4000_1` — the 690x570 plates. One continuous
  brushed sheet; every panel is carved into it, never a separate tile.
- `BITMAP4615_1` — the wheel sitting on the real plate. Best shadow reference.
- `x3_control_grammar.json` in `df2-workstation/tmp/x3_wireframe/` — measured
  row pitches and ratios, plus the alpha-mask statement above.
- Our MIX rail is shaped like `BITMAP4100` — E-MU's **level meter**. There is no
  vertical roller anywhere in the dump. A control shaped like a readout.

## Method notes

- Prove with a render or a measurement, never by eye alone. I shipped a
  supersample that was **pixel-identical** to the original because
  `drawImageTransformed(..., fillAlphaChannelWithCurrentBrush=true)` bypasses the
  resampler. Only measuring caught it.
- Fix the ASSET, not the code painting over it. Every material win today came
  from that; every code-paint attempt was rejected.
- No `git stash/reset/checkout/restore/clean` in this repo. Read-only git.
- Off limits (another agent): `PluginProcessor.{h,cpp}`, `dsp/TrenchDspBridge.h`,
  `PresetRoster.inc`, `TrenchRuntimePreset.h`, `plugin/presets/bodies/X3F_*.json`,
  `plugin/CMakeLists.txt`.
