# Skeletons: shared pole halves across the factory data

Verified 2026-09-07 against plugin/presets/p2k/*.body240 (33 bodies, 132 corners).

- Comparing each corner's six pole pairs (words 2 and 3 of every row) as a SET, 10 groups of corners in different bodies carry identical pole halves:
  acid_ravage c0 = klub_klassik c0 = tooth_comb c0; acid_ravage c1 = klub_klassik c1; bass_tracer c1 = boland_bass c1;
  boland_bass c0 = lucifer_s_q c0; cruz_pusher c0 = fuzzi_face c0; dead_ringer c1 = eeh_to_aah c1 = ooh_to_eee c0;
  eeh_to_aah c0 = ooh_to_eee c1; meaty_gizmo c0 = millennium c0; meaty_gizmo c1 = millennium c1; talking_hedz c0 = ubu_orator c1.
- Compared row by row IN ORDER only 5 of those groups survive. The copies carry the same six pole pairs in a different row order.
  Row identity is therefore not something E-mu preserved when reusing a skeleton; the set of poles is. Any lane rule in the tool is ours.
- No M corner shares its pole half with its Q partner (0 of 66): Q always re-voices the poles.
- The three plots here are the corner-by-corner drawings of these groups, the bare cascades of ear_bender and lucifer_s_q with zeros removed,
  and the Morpheus skeleton library (83 shared pole postures across the 289 cubes, top 16 shown). The Morpheus count is not re-verified here.
