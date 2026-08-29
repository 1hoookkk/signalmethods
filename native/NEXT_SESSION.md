# NEXT SESSION — workstation (written 2026-08-29, end of the marathon)

Branch `face/ship-candidate-fx`. Everything below assumes the committed state
through `2d02097` (rendering truth). Launcher: `TRENCH Workstation.bat` at repo
root; build: `TRENCH Build App.bat` or `cmake --preset app && cmake --build
--preset app`. Never run two vcpkg-configuring builds at once — that is what
deleted Qt on 08-28.

## In flight at wrap time
An executor was finishing a slice in cascade_plot / armadillo_editor /
main_window: z-plane projection toggle for the plane (secondary view), unity
line emphasis, +24/−36 frame, LOG/LIN button. If its work sits uncommitted in
the tree: keep the z-plane toggle and unity emphasis, then apply the ruling
below (which supersedes its frame and kills the LIN button).

## RULED, not yet applied — the patent display pass (Tyson agreed 2026-08-29)
Rossum US 10,514,883's own display spec is the law for the response graph:
- Octave gridlines from 20 Hz (20 40 80 160 ... ), log frequency. No LIN mode.
- Vertical ±30 dB, gridlines every 10 dB, 0 dB the marked centre reference.
- The response graph becomes PRIMARY (takes the vertical stretch); the
  armadillo demotes to a compact fixed-height roots pane (~230 px) below it,
  still fully editable; pad stays at the response's shoulder (patent Fig. 4).
- "We render our curves too wide": all plot pens drop to 1.0 px hairlines
  (cascade, addressed stage, reference, strip minis); colour alone carries
  addressed.

## Queue after that (all ruled or agreed)
1. Scrub-ribbon import: .par / tracker trajectories in the lane seat, stamp a
   moment to a corner, stamp A+B to a morph pair (tools/formant_poles.py
   track/arma modes are the engines; ANALYZE already does held sounds).
2. espeak SPECTSQ2 decoder — 172-language mouth library for the shelf
   (format recipe in espeak-ng src/libespeak-ng/spect.c).
3. Second-wave dials: STRIKE (modal gain comb), AIR (bw-vs-f shear), FLUTTER
   (pad micro-walk), NASAL (coupled p/z insert). Selection-scoped transforms
   (gathered group as dial target).
4. Shelf audition pass — 41 postures, none ear-judged yet.
5. OneDrive: Documents/Desktop are redirected into a failing OneDrive
   (account panel says Cloud Storage unable to load). The app's real data home
   is OneDrive\Documents\TRENCH; local ~\Documents\TRENCH is an orphan
   (merged 08-29, kept as backup). Fix the account or unlink before trusting
   any Documents write.

## Known engine-proof divergences (not UI bugs)
FaceShot's LIMIT and MOVE FOLLOW checks fail against this branch's engine —
ported-harness expectations, tracked separately, untouched by UI work.
