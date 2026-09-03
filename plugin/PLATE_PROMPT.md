# TRENCH plate render prompt (geometry locked to df2_panel_beige.png, 828 x 1280)

Render a flat front-on product photograph of a rectangular instrument faceplate, portrait,
exactly 828 x 1280 pixels (or 1656 x 2560 at the same proportions). Camera perfectly
orthographic, no perspective, no tilt, plate fills the frame edge to edge, no background
visible, no shadow outside the plate, no text, no logos, no knobs, no wheels.

Geometry, in pixels on the 828 x 1280 frame, must be kept exactly:
- Outer stamped groove: a single rounded-rectangle channel whose inner edge runs at
  x = 38 to 790 and y = 31 to 1245, corner radius about 30.
- Notch: at the lower right the groove leaves the right side at y = 987, runs left to
  x = 620, turns down to y = 1245, then continues left along the bottom. The plate
  itself is not cut; only the groove steps around the notch.
- Display aperture: one recessed rounded rectangle, x = 97 to 738, y = 191 to 498,
  corner radius about 18, filled flat near-black (matte, no reflections, no content).
- Two wheel slots: recessed rounded rectangles, x = 97 to 445, y = 561 to 626 and
  y = 697 to 765, corner radius about 20, filled flat pure black.
- Nothing else on the plate. The lower third below the slots is plain surface.

Material and light:
Keep the warm golden-grey base tone and the soft, organic light gradient falling from
top-left to bottom-right. Replace flat procedural grain with an ultra-fine, microscopic
horizontal hairline brush, so fine it only shows where a soft specular highlight glints
off the chamfered channel. A semi-gloss, baked industrial enamel coating over a solid
cast chassis (warm olive-drab, slate, or warm putty). Zero digital noise or sandpaper
texture. The surface is silky-smooth to the eye, relying entirely on realistic studio
rim-lighting and ambient occlusion to show weight. The stamped groove retains soft,
liquid-like paint buildup in the recesses, giving the impression of physical tooling.
The aperture and slot edges carry a small chamfer with a thin catch-light on the upper
edge and a soft shadow on the lower inner edge.

Deliver as PNG, no alpha, no vignette, no border.
