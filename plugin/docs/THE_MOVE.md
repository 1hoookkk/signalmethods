# THE MOVE — the repeatable body-authoring workflow

One page. Every step leaves evidence. No step requires typing numbers.

## The grammar: what the four corners ARE

**A body is two gestures** (Tyson's law, 2026-08-01).

- Gesture one: **MORPH travel at Q0** — two photographed moments of a motion.
- Gesture two: **MORPH travel at Q100** — the same motion performed another
  way, or a different motion entirely.
- MORPH plays the gesture; **Q crossfades between the two performances**.
  Or vice versa — which axis carries which pair is the author's choice;
  the engine treats both axes the same.
- The wheel's in-between travel is the engine's own (encoded-word
  interpolation), never the source's. That is the originality defense.

The 33 ROM tables agree (fit 2026-08-01, `scratchpad/rom_stage_law.tsv`):
only ~38% of stages keep their tuning across Q. The second gesture is
authored three ways, all one press in the workstation — PUSH (the special
case: same notes, resonance pushed 40% toward a ~66 dB R' ceiling),
RETUNE (whole cascade shifted, FuzziFace = +2 oct), or a hand re-voice.

## LANE A — sound in hand (measured)

1. **Record the gesture.** One WAV, one motion, a few seconds. Phone is fine.
2. **Pick the moments.** Workstation → `P` → drop the WAV → ANALYZE →
   formant tracks appear. Select the first steady moment → **AY [1]**.
   Second moment → **EE [2]**. (LISTEN plays the selection raw — the filter
   never hears the evidence.)
   - Praat-first variant: analyze in Praat itself, save the FormantPath
     (either text format), drop that file on the desk instead.
   - No-moments variant: Ctrl+drop the WAV on the workstation field →
     MAKE BODY fits the whole spectrum in one shot.
3. **BUILD → OPEN P/Z.** Certified Q0 pair lands in the editor.
4. **Author the Q attitude.** PUSH Q100 (the fitted ROM law), then
   RETUNE / SPREAD / DEEPEN / BLOOM / SPIT to taste. One press each, undoable.
5. **Gate and judge.** Distance gate against the loved references
   (`tools/distance_gate.py`); then ears. One strike — a failed body is
   killed, not dialed.

## LANE B — idea in hand (designed)

Describe the curve to Claude (vibes, references, dashed-target plates).
Claude designs the target, the fitter places the poles (house law:
curves, not poles). Enter LANE A at step 3.

## Division of labor

- **Tyson**: records, drops files, rides the wheel, issues verdicts.
- **Claude**: runs fits, plots on the fixed −60..+30 grid, distance gates,
  batch sweeps, dossier lookups — and converts any Praat object headlessly.
- **The workstation**: the only place poles are placed by hand.

## The right way / the wrong way (settled 2026-08-02)

RIGHT — every TRENCH filter is designed exactly like this:
1. **Every number has a source**: a measurement, a published equation, a
   derivation run in code, or Tyson's explicit verdict. Nothing typed from
   taste or memory. If the source can't be named, the number is a bug.
2. **Design the curve. The fitter places the poles. The certifier gates.**
   Hands never touch pole coordinates outside the workstation surgery room.
3. **Two designed poses in a musical relation** — notes, octaves, chords,
   voice leading. No random corners. The travel between them is the engine's.
4. **Q is authored over a held scaffold** (the byte law: zeros are the hall,
   poles are the performance) — or by a documented, named alternative.
5. **Posture is the loved posture**: cascade p95 at the measured +6.9 dB,
   peaks commit, valleys carve but never fall out, the open corner is OPEN.
6. **Gate before ears**: the loved-ROM minima (travel/aliveness/contrast),
   daylight >= 3 dB from every ROM corner, certification on the shipped
   bytes, plates rendered from the shipped interpolator.
7. **Ears crown. One strike kills.** No dialing a corpse.

WRONG — any of: invented constants, hand-placed poles, corner slop,
resonance bolted onto an untouched passband, peak-normalized gain (the
quiet-body bug), unmeasured "vibes" targets, clones inside the daylight
gate, claims without a run on the real path.

Timing note: TRENCH sections are minimum-phase, so time behavior is
inherited from the magnitude curve. Design the curve honestly and the
timing follows — there is no separate time-domain design step.

## Honesty rules

- Every fit reports its residual in dB; every body is certified before
  it is ever audible.
- Measured sources are studied, then re-voiced; convergence-to-reference
  is a failure, distance is the feature.
- No claim without a run. "Done" means the evidence is in the chat.
