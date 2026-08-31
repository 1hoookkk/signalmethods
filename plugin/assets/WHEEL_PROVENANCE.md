# MORPH/Q wheel - provenance (trench-native)

Shipping strip: `trench_roller_strip.png` = 128 x 417x93, git blob 2060e34
(trench-native c0d04dcc, 2026-08-29), as rendered - lamp untouched.

Body: the SS3 render. Blender Cycles, 257 raw frames at 1200x360, cropped
(25, 42, 1175, 318) and Lanczos-downsampled to 417x93 cells
(`trench-x3-clean/tools/render_ss3_wheel_integrated.py`,
`assemble_integrated_ss3_strip.py`). Master copy:
`trench-x3-clean/plugin/assets/trench_roller_strip_ss3steel_master.png`
(first seen 2026-08-04 as `trench_roller_strip.png.bak_ss3steel_orig`).

Lamp: the E-mu X3 roller's light field, measured from
`do-it/emu-x3-bitmap-dump/BITMAP4331_2.bmp` (129 x 85x16) and laid on the
render (`build_measured_ss3_wheel.py`). Re-lighting is done by SOLVING that
field out and recomposing it with a two-colour lamp
(`trench-x3-clean/tools/retint_roller_glow.py` pointed at the SS3 master),
never by painting over the pixels.

Ruling (Tyson 2026-08-29): the SS3 body is the wheel. The older 139x31
cycle-10 master documented in trench-x3-clean is NOT to be restored here;
it was rendered side by side and rejected.

Ruling (Tyson 2026-08-31): every strip in git history was rendered into the live
face (72 filmstrips across df2 / trench-x3-clean / trench-native); the 417x93
lineage from 2026-07-25 onward all work, blob 2060e34 is the pick. Its diode
trail fades per tooth; it is never re-laid through a chroma mask.
