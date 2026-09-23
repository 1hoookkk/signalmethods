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

## 23 Sep 2026: rolling strip restored

`trench_ss3_strip.png` is the 128-frame roller strip from trench-workstation commit a4f88ad2
(`plugin/assets/trench_roller_strip.png`, 53376x93, 417x93 frames), the wheel in the 3 Aug
screenshot, byte for byte. Its lamp is one chromaticity (hue 172.7 deg, saturation 0.80, peak
#2AD7C2); that value is the face accent, the curve colour and the TRENCH outline.
The wheel draws its full silhouette (frame x 7-410, y 8-87) inside the plate hole, 2 px in from
each end and 1.2 px low, with the 3 Aug contact shadow under it.
`trench_knob_black_strip.png` is the knob strip from trench-x3-clean commit 3192f66dd (12 Aug).

## 23 Sep 2026, evening: SS3 strip and crown restored

`trench_ss3_strip.png` is again the 129-frame SS3 3x strip (git blob 90791af, commit 265d6b3d, the wheel ruled "Perfect"), frame 128 blank. The crown (belly highlight, underside darkening, warm end shade 0xff17110a) and the drum-in-slot seating are that commit's code verbatim. The lamp is re-tinted at load from the face accent (`WheelControl::tintLamp`, lamp measured hue 175 deg, saturation 0.64, value 0.81; saturation held at 0.6 or more).

## 23 Sep 2026, night: gunmetal SS3 master

`trench_ss3_strip.png` is `trench-x3-clean/plugin/assets/trench_roller_strip_ss3steel_master.png` byte for byte (md5 e46582ef..., 128 x 417x93; the same bytes as trench-workstation's `.bak_ss3steel_orig`), the "OLD gunmetal (crown)" wheel of the 21 Jul comparison screenshot. No code overlays and no runtime tint: the lamp is the render's own teal.

The strip now carries 129 frames: the 128 master frames plus frame 0 appended as the wrap sentinel (frame 128 == frame 0). Ribs travel 1.5 px per frame at the drum centre (190.5 px of 417 over the range), and the drag throw equals that travel so the surface rolls with the cursor.
