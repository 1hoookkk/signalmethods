# Control artwork

The UI was restored on 27 September 2026 to the version installed immediately before the rejected readout, typography and wheel-shading passes. The previous installed module is retained in `out/install-backups/20260927-223440-593`; its SHA256 is `0B280FAD4F15EA6C2E5F062B6F7E965F2F6DAD30532F8F1FED39B30D6539571F`.

`trench_control_art.png` derives from `C:/Users/hooki/do-it/emu-x3-bitmap-dump/BITMAP4615_2.bmp`, SHA256 `02b4378bca57d16eaba6e22ba7d3b61d7e1f59e8768c912214f6d92fcd4b29e1`. The Morph/Q panel is frame 9 at `(1251, 0, 139, 183)`; frame 0 supplies the identical unpainted background.

All four numeric readouts use the same blue-grey face as the BODY and modulation bars. The proposed neutral recolour was rejected and is not used. Control shadows follow an inset rounded outline at 16 percent opacity, radius 2 and offset `(0, 1)`. The outline is inset 1.4 logical pixels and trimmed another pixel at the top to concentrate the shadow beneath the control. The clipped and stretched bitmap shadow was rejected because it produced straight underlines; it is no longer rendered.

The sage plate and SS3 strip are unchanged. Wheels sit 2 logical pixels lower and extend 3 logical pixels beyond each side of the opening. Their height follows the respective opening rather than being inferred from width. They paint over the frame using their bitmap alpha, with a narrow dark axle behind the transparent central end notches. Each active frame is copied into a separate image before alpha clipping: Direct2D's image-mask path selects the first texture page, so a clipped view into the wide filmstrip can produce opaque end blocks on later frames. Hover lighting uses the same frame mask. The component bounds contain the whole painted wheel.

The wheel cast shadow comes from atlas `(0, 22, 96, 7)`, corresponding to source frame coordinates `(0, 30, 96, 7)`. Light bevel pixels are excluded, leaving the original dark shadow matte. Each shadow starts one logical pixel inside the nominal lower silhouette and is scaled uniformly with wheel width, preserving the source shadow's proportions. No additional procedural ellipse is drawn beneath it. Numeric weight and spacing retain the preceding installed treatment.

The logical plugin face is now 290 by 480 pixels. The same plate coordinates continue to place the controls, with the existing UI scale options applied to that smaller face.

Reproduction: `output/x3-ui-integration-20260927/extract_control_art.py`. Rejected source and artwork were saved under `output/ui-revert-20260927/rejected-source` before the rollback. The reference comparison remains available under `output/x3-reference-confirmation-20260927`; it does not establish the exact X3 font family.
