# Next session — one edit authority on the float engine

Vocabulary: second-order section, conjugate pole pair (F, ΔF), zero pair, real-axis
pair, series cascade, log-magnitude sum, DC gain, H(z), θ = 2πF/fs,
r = exp(−πΔF/fs). Design datum 44,100 Hz. Method: Bell 1961; encoding: Rossum
US 5,170,369 / US 10,514,883; morphing evidence: Ding & Rossum 1995.

## State of the tree (2026-08-25 end of session, all UNCOMMITTED)

App suite 84/84, full CTest 104/104, headless. Landed and verified today:

- **The armadillo plane is the primary editor** (armadillo_view.{hpp,cpp}): x = log2
  frequency over fs/2048..fs/2, y = dB-from-rim (−20·log10(1−R), 96 dB span, rim on
  top). Letters p/z per section; drag writes author words via words_from_root (the
  byte lattice IS the grain — every landing quantizes to legal words);
  double-click places a pole pair at (F, depth); ghost z at the right edge for
  live-pole sections — drag in to birth the zero, drag any root off the right edge
  to park it (zeros) or clear the section (poles); pins honored; legality guarded;
  DC law on every write; one undo per gesture.
- **Curve-space editing retired** (Tyson's verdict: "editing the curve feels wrong,
  the armadillo feels right"). ResponsePlotWidget is a consequence monitor: tokens,
  hover, pin clicks for FIT survive; drags, double-click placement, snap,
  zeroWidthForDb y-solve, refusal display deleted. All gesture tests ported to the
  armadillo; the y-solve specs died with the machinery.
- **Per-corner TRANSPOSE**: the dial writes the current corner's semitones; the view
  interpolates semitones bilinearly over (morph, q) and forms the ratio after —
  corner 1 low, corner 2 high, MORPH rides the pitch trajectory. View-side only,
  never in the body. The law (transpose.cpp): radius held (bandwidth-in-Hz
  preserved), poles < 70 Hz anchored, sharp zeros (r ≥ 0.8) stop below 0.45·fs,
  travel clamps to [20, 0.49·fs].
- Corner pads 1–4 (lit = edited corner), Ctrl+click copies current corner there,
  right-click menu copy/save/load; SAVE verb; chooser lists the 9 bank postures +
  8 compiled vowel-class corners; tooltips throughout; LEVEL (grid-weighted power)
  beside ERR (weighted zero-mean residual — Bell's alignment, tested).

## THE MOVEMENT: one edit authority on native::Body (ruled, audit verified)

Sol's audit confirmed line-by-line (2026-08-25): BodyDocument still owns packed
words; every armadillo drag quantizes through words_from_root; native::Body (the
float engine, nulls the 33 imports) has no app consumer; audition sends the
44.1 k-designed viewCascade() to the device without consulting its actual rate
(main_window.cpp updateAudition) — at a 48 k device everything sounds ~1.47 st
sharp; native tests stop at 96 k.

The plan (adopted; staging per the CLAUDE.md invariant — packed stays engine of
record until the nulls prove the move):

1. BodyDocument owns native::Body (float Hz/ΔF roots, one gain per corner).
2. ONE root-edit command (pole or zero, Hz + bandwidth, undo-owning). The
   armadillo and any overlay are synchronized projections calling that command —
   one edit authority, not necessarily one visible editor.
3. Interior = the patent law on encoded roots (log F, log(1−R)) — this also kills
   the phantom-zero interior artifact (old task 6) by construction.
4. FIT commits to the same native roots; Bell traces stay read-only; delete the
   FitRoom duplicate plot (the main monitor already draws aligned target +
   residual + ERR).
5. Audition redesigns the cascade from the same roots at the audio device's
   ACTUAL rate; display renders at the requested rate. (This is the fix for the
   detune bug — do it on native roots, not as a packed-side patch.)
6. Tests at 44.1/48/96/192 k (192 k is currently untested).
7. P2K words survive only in explicit import/export adapters; design a native
   save format (floats; .body240 remains the legacy export). Format decision goes
   to Tyson before implementation.
8. Gate: the 33 imports null on the native path before any packed parity test is
   retired. Do not break packed while moving.

Interaction law for the primary surface (ruled today, keep under the migration):
double-click creates a pole at the clicked frequency and depth; horizontal drag =
frequency; vertical drag = bandwidth/resonance; the zero stays parked until
deliberately drawn (ghost-in); typed Hz/ΔF is for exact correction only.

Queued behind the migration: optional vertical formant guides on the armadillo
from Praat/the app's LPC analysis (evidence and candidates, never auto-authoring —
place poles on guides by hand; SPAN stays a visual reference).

## Open decisions for Tyson

- Transpose wall pileup: compress the ratio so no root reaches a wall, or keep
  the stack as the audible end-stop.
- Armadillo letters under transpose: author positions (current) vs riding the
  view law.
- profiles/user_postures.json "mine 1"/"mine 2": old-bug fossils, delete on word.
- Native save format naming/shape (item 7 above).

## Standing laws (unchanged)

- Series cascade of six sections; log magnitudes add; the complete cascade is the
  comparator. |H_i(1)| = 1 per section; |H(1)| = 1 for every viewed state.
- Pole pairs: operator's choice. Zero pairs: the hand or the solver. Nothing else
  writes roots. Section identity is ordered; never sort.
- Imports byte-exact. Tests headless (-platform offscreen), never on the screen.
- A claim without an address is not evidence.
