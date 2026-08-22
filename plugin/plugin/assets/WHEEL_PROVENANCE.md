# MORPH/Q wheel — locked recipe

The neutral master is:

`trench_roller_cycle10_master_128_139x31.png`

It is the accepted DF2 wheel from
`df2-workstation/dev/tmp/wheel_grind/codex_fix/measured_glow_cycle10_candidate/`
and is retained here byte-for-byte:

```text
SHA-256  8FB47159153B93401DB84A51CADF26E05C9723E07075302F0EDBE020990A916F
size     17792 x 31 = 128 frames x 139 x 31
```

## Construction

- Body: the locked shallow saucer render using a 10-frame tooth micro-cycle.
  It does not perform a monotonic 3D spin; that was the route that ovalled,
  leaned, and mutated the wheel.
- Light: the broad transmitted field measured from E-mu `BITMAP4331_2.bmp`.
  It travels across the static/micro-cycling body and is fin-occluded. It is
  not a code-painted bead train.
- Runtime strip: `tools/retint_roller_glow.py` solves the cyan screen layer,
  recomposes only that field with the selected two-colour lamp, and appends
  frame 0 as byte-identical frame 128. The 129th cell is the E-mu wrap sentinel.

Selected lamp:

```text
deep transmission  #6F7C12
hot core           #E2EB98
```

## Non-negotiable failures

- Do not use `dev/tmp/thumbwheel_blender/wheel_129`; its source is the
  double-bulged barrel shown in the rejected contact sheet.
- Do not restore the malformed AI left cap from the SS3 strip.
- Do not synthesize a row of bulbs or move the wheel with a monotonic spin.
- Do not interpolate a 128-frame strip into 129 unique poses. Frame 128 is a
  wrap sentinel and must be byte-identical to frame 0.
