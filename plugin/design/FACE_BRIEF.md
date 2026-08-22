# TRENCH face — design brief

*2026-07-30. This page states the DSP as shipped and what the face must communicate about it. It never says where anything goes.*

---

## The signal chain (as shipped, in processing order)

```
DAW input
  │  (when a generation is committed, this is REPLACED by the R1 loop)
  ▼
PREAMP   Mackie desk model BEFORE the cascade. param `preamp` 0..1.
         Exact unity at 0 (desk wet-fades in over the first 5%).
         Adds harmonics the filter then carves. Works with NO FILTER.
  ▼
CASCADE  6 second-order IIR sections. BODY = 240 bytes of coefficient
         keyframes at 4 corners of MORPH×Q. The wheel position is lagged
         (CORDS law) and the cascade rebuilds every 32 samples from real
         points on the encoded path.
         GRIT (param `chew` 0..1) lowers the threshold at which a section's
         own output pushes its pole radius toward the unit circle — the
         patent's dynamic pole distortion. MORPH selects which section.
         GRIT is mathematically inert on a body with no poles (NO FILTER).
  ▼
AGC      per-sample leveller (E-MU table, verbatim from the DLL). Invisible law.
RESONANCE BUDGET  broadband gain cap: response peak ≤ +24 dB. Engages on
         ~10-20% of the grid of hot bodies only. Invisible law.
  ▼
SLAM     desk drive AFTER the filter. param `slamDrive` 0..1. Off = exact
         unity. Resonant peaks driven into it = the tearing-paper sound.
         Works with NO FILTER.
  ▼
SAFETY CEILING  −0.1 dBFS soft bound, wet only. Invisible law.
  ▼
MIX      param `amount` 0..1. True linear crossfade with the untouched dry
         input: out = dry + (wet − dry) × mix. Zero added latency.
  ▼
DAW output ──────────────────────► also recorded into a 30-second ring
```

## The state machine (re-entry, as shipped)

- `sourceGeneration = 0` (**LIVE**): the DAW input feeds the chain.
- **COMMIT**: on the audio thread, the last 4 bars at host tempo (4 s
  without transport) are copied out of the output ring. `sourceGeneration`
  becomes N+1. The copied audio now loops in place of the DAW input.
  The DAW input keeps flowing (into the ring only) — it is never lost.
- **RETURN TO LIVE**: one act, `sourceGeneration = 0`, instant, lossless.
- Re-commit stacks (R1 → R2) with exactly one undo level.
- Not yet built: R1 persistence in the project; bar-aligned loop start.

## What the face must communicate (each item is a DSP fact)

1. MORPH and Q are positions in the body's coefficient space — continuous,
   performable. (Already solved: wheels + readouts + curve. The standard.)
2. The curve is the filter's transfer function at the current position.
   The ghost line is the actual output spectrum. Law vs reality. (Solved.)
3. PREAMP, GRIT, SLAM are three different nonlinearities at three different
   points of the chain above. Their ORDER is a fact. PREAMP and SLAM
   function with no filter loaded; GRIT does not — the face must not
   pretend otherwise (DRIVE ONLY contract).
4. GRIT's actual work varies with signal and Q: the engine reports the real
   pole-push amount (`trench_engine_grit_activity`). Show measurement,
   never restate the knob.
5. MIX is wet/dry of the whole chain. (Solved: the thin wheel.)
6. The source state: whether the chain is eating the DAW input or a
   committed generation. This changes what every other control acts on.
   Must be readable at a glance, plus one act in, one act out.
7. BODY selection loads new coefficients with a 100 ms composed crossfade —
   switching is safe and musical; the face can invite browsing (hover
   audition already works on the same path).

## Standing laws (short form)

- Coral = signal, teal = hands (GIF law). Curve stays pristine — no control
  ever deforms it. AGC / budget / ceiling never appear on the face.
- Names are verbatim: PREAMP, GRIT, SLAM, MORPH, Q, MIX, BODY, KEY, LIVE, COMMIT.
- No dumped E-MU pixels. 2009 vibe, baked bevels, never modernize.
- One gesture set: vertical drag / wheel, Shift fine, double-click reset;
  state changes by a press.
- Existing control types: rollers (MORPH/Q/MIX), the glass, white readout
  boxes (numbers only), the BODY bar, the plate. A new control type must be
  drawn as well as the rollers were, or not exist. The three-knob strip
  failed because the knobs were PRIMITIVE — a gradient circle and a dot,
  one step away from default — next to rollers that took real drawing.
  Whatever the chain's controls are, they must be finished to the same
  standard as the wheels: depth, material, illumination, wear.

## References (study, don't copy)

- 13-state X3 FILTER panel grid: wheel labels change per filter type;
  controls appear with the choice that needs them; blank when empty.
- EmulatorX FILTER panel screenshots 2026-07-25: dark controls + white
  readout boxes; small dark knobs with teal glints.

## The two open questions

1. What do PREAMP / GRIT / SLAM look like, such that their order in the
   chain and their different jobs are visible without text, and they read
   as part of the same instrument as the wheels?
2. What does COMMIT / current-source look like? Facts it must carry: which
   source is feeding the cascade right now; that live input is set aside in
   R1 (not lost); one press in, one press out. No E-MU visual precedent
   exists — the hardware's version was the resampling ritual itself.
