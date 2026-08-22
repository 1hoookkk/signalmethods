# SPECTRAL SCORE — the voice-leading score format

The prompt layer (an LLM or a hand) emits this JSON; `tools/spectral_score.py`
compiles it deterministically through trench-core into a certified `.body240`.
The LLM never writes coefficients and never labels its own output "good" —
it proposes a score, the compiler makes it exact, ears judge it.

Grounded in the measured corpus grammar
(`evidence/session_20260804/corpus/CORPUS_REPORT.md`):

- **Six persistent voices; the cascade is the chord.** Slot identity never
  swaps during compilation or polish.
- **SCALE law** (26/33 ROMs): level is per-corner (`corner_level_db`),
  distributed equally over the six stages. No per-stage gain knobs.
- **S6 law** (127/132 corner-rows): slot 6 is the terminal voice; its zero is
  forced to the unit circle unless the score says `"force_terminal": false`.
- **Voice-leading verbs, per voice, per axis.** Contrary and oblique motion
  emerge from per-voice signs — the ROMs measure 65% parallel, 31% contrary,
  5% oblique, median travel 1.1–1.6 octaves, common tones rare.

## Shape

```json
{
  "name": "glass_choir",
  "rate_hz": 48000.0,
  "corner_level_db": {"M0_Q0": -12, "M100_Q0": -12, "M0_Q100": -16, "M100_Q100": -16},
  "voices": [
    {
      "slot": 1,
      "role": "bass anchor",
      "m0q0": {"pole_hz": 110, "pole_rp_db": 30, "zero_hz": 300, "zero_rp_db": 18},
      "morph": {"motion": "parked | rising | falling", "interval_st": 7, "rp_delta_db": 0},
      "q":     {"motion": "parked | tighten | widen", "rp_delta_db": 12, "interval_st": 0},
      "m100q0": {"pole_hz": "...optional explicit corner override..."}
    }
  ]
}
```

- `pole_rp_db` / `zero_rp_db` are R' dB (Rossum): `R' = −20·log10(1−r)`.
- The zero rides with its voice under `morph`/`q` motion (the ROM depth-pairing
  grammar); override a corner explicitly to break the ride.
- `morph` derives the M100 corners from `m0q0`; `q` derives the Q100 corners;
  M100Q100 applies both. Explicit corner objects (`m100q0`, `m0q100`,
  `m100q100`) always win.
- Compile = roots → `trench_stage_words_from_roots_at` → pack → **certify
  (33×33)** → install only on pass. A refused or unstable score fails loudly.

## What a prompt layer should do

Translate intent into the score's vocabulary — "bass stays parked, middle
voices move contrary, top voice rises an octave; Q tightens the inner voices" —
then let the compiler and the ears do the rest. Proposals are auditioned
against BEFORE/AFTER in the Workstation's BUILD face; nothing is auto-judged.
