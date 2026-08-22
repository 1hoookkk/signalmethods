# Scope — MORPH / Q thumbwheels vs E-MU reference

Measured 2026-08-07. Reference = Tyson's pasted MORPH/Q crop + Screenshot
2026-08-03 / 2026-07-25 + BITMAP4331. The wheel is a flat-laying spinny wheel
(pitchwheel seen edge-on), not a drum scrolling vertically — all reads below
follow that.

## 1. "Still sits too far left" — it is CENTRED; the light lies

Measured at the well's centre row (plate y594 → editor):

| | editor x |
|---|---|
| well opening | 38.2 .. 175.2 (w 137.4) |
| visible drum (134.3 of the 139 aperture) | 39.35 .. 173.65 |
| clearance left / right | **1.15px / 1.55px** |

Dead-centred within 0.4px. What reads "left": the plate's bevel LIGHTING is
asymmetric — right bevel steps to bright (L≈190–197) immediately at the edge,
left side sits dimmer (L≈166) behind a ~2.4px half-tone ramp. The lit right
band reads as clearance the left side doesn't have. `WheelControl.h:279`
already documents this exact finding; the 2026-08-07 nudge attempt was
reverted on verdict ("the wheels are off"). Moving the wheel is the wrong
method — it's the well art.

## 2. "Cut the wells out" — the reference agrees

In the pasted E-MU crop, the wheel fills its opening **wall-to-wall**:
wheel span = opening span (x19..156), zero side clearance, no lit side bevel.
Vertically: ~4px feathered shadow above (the alpha-mask seat), ~1px below.
Our well leaves 1.15/1.55px gaps plus a lit right bevel — that's the whole
difference.

Fix is the plate asset: recut both wells in `df2_panel_beige.png` to E-MU
grammar — walls flush to the drum caps, symmetric (or no) side bevels,
feathered top shadow, done as a mask so the grain survives. Both wells are
identical openings (plate x97..623-ish bands at y562–627 and y699–764), so one
cut serves both.

## 3. Strip anatomy — measured against the "static body, glow travels" spec

`trench_roller_strip.png` = 128 frames of 417x93, minified to the 139x31
aperture, drawn centred 1:1 logic (`WheelControl.h`).

- **The body SPINS monotonically.** Mean |body diff| vs frame 0 (glow pixels
  excluded): 1.7 at f1 growing to ~8.5 by f32 and never returning — no cycle
  of any length ≤15 frames. This is exactly the "monotonic spin" the spec
  rejects (ovals/leans the wheel). A static-body / micro-cycle strip does not
  exist yet.
- **Glow travel works** (frames 8→120 sweep x91→307 monotonically) but frame 0
  is odd: lamp nearly out (474 sat px vs ~3000 typical) with residual sheen
  centred mid-wheel (x207), and frames 8→24 brighten in place before moving.
- **`build_measured_glow_128.py` does not exist** in tools/ (quoted from a
  plan, not the repo). What exists: `render_glb_wheel_257.py`,
  `assemble_glb_wheel_257.py`, `bake_wheel_lamp.py`, `rehue_wheel_lamp.py`,
  `retint_roller_glow.py`. A static-body rebuild = reassemble from the
  existing GLB renders (one body pose + composited travelling lamp), never a
  regeneration — THE GLB IS SACRED.

## Glow colour — mint confirmed, with numbers

| source | hue |
|---|---|
| Tyson's pasted reference glow | **162.3°** |
| our accent `66DBB8` (curveColour) | **162.1°** |
| raw BITMAP4331_2 lit tooth | 180.0° (pure cyan) |
| our baked strip packet today | ~171° |

The raw bitmap tooth is cyan, but on E-MU's face it reads mint — and the
reference measures EXACTLY our accent. "Mint green" is right and the target
already lives in the theme. The baked packet sits ~9° cyan of it; a
`rehue_wheel_lamp.py` pass to 66DBB8 closes that.

## Proposed order (one verdict each)

1. Recut the wells in the plate (kills the "too far left" read + seats the
   wheel like the reference).
2. Rehue the baked lamp packet 171° → 162° (66DBB8).
3. Static-body strip rebuild from GLB renders (the big one — kills the
   oval/lean). Frame 0 lamp behaviour gets fixed by the same assembly.
