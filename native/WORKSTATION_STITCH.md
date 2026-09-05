# THE STITCH

The FRAMES room becomes one object: the library drawn as a single stitched surface.
Spec, 2026-09-05. Replaces the Martens plane as the hero. Anchors, PAIR, the timeline,
EDIT and SOUND stay as they are.

## What it is

A corner is a skeleton: six poles, twelve numbers, level removed. A body is a square in
that space, four corners joined by two MORPH edges and two Q edges. A Morpheus cube is
a box, eight corners, three edge kinds. The census shows E-mu copied corners verbatim
between bodies (ten exact groups in the 33, eighty-three shared postures across the 289
cubes). Where two bodies share a corner, their squares touch. The library is therefore
not a pile of squares. It is one connected surface, and that surface is the thing drawn.

Walking an edge plays that body's morph. Passing through a shared corner walks into the
next body without a jump, because the corner is the same bytes. The whole library is
one filter you move through.

## The graph

- Node: a distinct skeleton. Identity is the pole bytes of the six rows with the gain
  word masked. Exact match first. Then near match: every slot's pole within 3 percent in
  frequency and 0.01 in radius, slot to slot. Near matches fuse into one node and the
  fused node carries the mean; the count of members is kept.
- Edge: one axis of one body between two of its corners. Kinds: MORPH, Q, Z. An edge
  carries its body, its axis, and its two corners' full words, so the sound anywhere on
  it is the hardware lerp of those two corners.
- Face: a body (four nodes) or a cube (eight nodes). The sound inside a face is the
  bilinear or trilinear blend of its corners, which is what the plugin does.
- Stub: a captured or read frame attached to its nearest node by slot-paired pole
  distance. Drawn as a short edge leaving the surface. Its sound is the frame itself.
- Groups are floors: P2K, MORPHEUS, X3, VOWELS, HEADS, XL-1, captures. A floor can be
  folded from the tray. Edges never cross floors; a stub can.

Expected size: 132 P2K corners fold to about 110 nodes; 2312 cube corners fold far
further because of the posture groups. Hubs are the nodes with many faces. The 2089 /
4170 / 6270 Hz posture is a hub of twenty cubes.

## The geometry

No springs, no random seeds. The layout is spectral: the graph Laplacian's second, third
and fourth eigenvectors give x, y, z. Every run gives the same picture. Hubs sit at the
centre, long chains reach out, isolated bodies float as separate squares near the edge.
Edge length then means something: a long edge is a big morph, a short one is a small
re-voicing, because the Laplacian keeps neighbours close.

Floors are stacked on z with a fixed gap. Inside a floor the z eigenvector gives relief
so the surface is not flat. Orthographic camera, three-quarter view, drag to orbit,
wheel to zoom, double click to centre on a node. No perspective, no fog.

## The drawing

- One wireframe. Edges are 1 px lines. MORPH edges white, Q edges grey, Z edges the
  darker grey. Brightness falls linearly with depth so the far side reads as behind.
- Nodes have no marker. A node is where edges meet. A hub shows a small square only when
  it has four or more faces.
- Stubs are cyan lines. Anchors are pins: a small cyan square on the surface.
- Live position is a single yellow mark. It sits on an edge or inside a face. Its
  cascade is on the response panel, its poles on the ARMAdillo, as now.
- Hover names the edge: body and axis, in the status line. Nothing is written on the
  surface itself. No labels on nodes, no titles.
- Colour rule as elsewhere: yellow live, cyan chosen, white data, grey structure.
- The floor of each group is one horizontal line at its z, nothing more.

## Moving through it

- Press on an edge: the live mark snaps to it and the body's morph plays at that point.
  Drag along the edge is MORPH or Q, whichever the edge is.
- Drag across a face: MORPH and Q together, the plugin's wheel, solved from the pointer.
- Reach a node and keep dragging: the mark continues into the face on the other side
  chosen by drag direction. This is the walk. The status line names the new body.
- Press on a node: that corner alone.
- Drag from a node or a point on the surface to a corner key in the body block: assigns
  the corner (a node) or captures the moment (a new frame, stub on the surface).
- Drag to the timeline: a key at that surface position. Play walks the path.
- PAIR stays: pick two nodes anywhere and morph straight between them, off the surface.
  A pair edge is drawn cyan while it is live.
- EXPORT of a body whose four corners are nodes adds a new face to the surface. The
  library grows in front of you, and the new face is yours.

## What it replaces

- The Martens plane and the eight sort measures leave the field. The PCA stays in the
  code as a measure for the tray sort only.
- Delaunay and the triangle blend leave. Blending is by face, the same maths as the
  plugin. Anchors that are not on a face keep the PAIR morph.
- The FIELD shading, gone already, stays gone.

## Data

`tools/stitch_graph.py` builds `native/python/workstation/stitch.json` from the 33
bodies, the 289 cubes, the 18 xml bodies and every frame already in frames_3d.json.
Nodes with members, edges with body and axis, faces, stubs, and the spectral layout.
The workstation loads it; the C++ side does no graph building. Layout is recomputed in
Python only when the library changes.

## Acceptance

- Two corners with identical pole bytes are one node. Count of exact groups in the 33
  equals the census, ten.
- The sound at t on a MORPH edge equals the plugin's interpolate_words of the two
  corners at t, word for word.
- The sound inside a face equals the plugin's wheel at that MORPH and Q.
- Spectral layout is identical across two runs.
- The head, anti-vowel, zeros-only, chimera and fifth bodies appear as faces after
  export, attached where their corners are nodes.

## Build order

1. Graph tool and JSON, with the census numbers printed.
2. Flat drawing, x and y only, in the current field. Press and drag on edges and faces.
3. The walk through nodes.
4. Depth, floors and orbit.
5. Stubs from SOUND and captures; EXPORT adds a face.

Nothing on the ship face changes. This is the authoring tool only.
