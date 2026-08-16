# Design ideas for a Z-plane filter — prompt

> PARKED. Not the workflow. The schema is not locked until lane assignment
> is proven by hand in the app; authoring is done entirely by Tyson on the
> surface. This file waits until that day.

You are designing morphing filter objects for a Z-plane cascade engine. Your
job: propose designs as JSON files in the exact format below. The format
compiles deterministically to hardware-identical filter bodies, so precision
matters more than prose.

## The object, in five facts (all verified against factory hardware — do not
contradict them)

1. The filter is a serial cascade of up to 7 second-order sections ("lanes").
   Responses multiply, so dB curves add. There are NO filter types — no
   "lowpass section", no "shelf mode". A lane is only a pole pair, a zero
   pair, and a scale. Character comes entirely from placement.
2. Loudness comes from geometry, never from gain: a tight pole (r close to 1)
   against a distant zero is how the factory makes level. Keep `scale_db`
   near 0 and let placement do the work.
3. Roots are either a conjugate pair `{ "hz": <Hz>, "r": <0..1> }` (a
   resonance or notch at a frequency) or a real pair
   `{ "pair": [a, b] }` with a, b in (−1..1) (smooth asymmetric
   shoulders and rolloffs with no resonant ring — the factory uses these for
   bass shelves and rolloff shapes).
4. The object has up to three morph axes. M is the performance axis, ridden
   continuously — the journey the player hears. Q is a character axis
   (typically radius lift: resonances tighten). T is a third pose axis; if no
   lane moves on T, the object is a flat ".4" (four corners).
5. Corners are interpolated in log space by the engine and stability is
   guaranteed by construction, but wildness is gated: keep the summed peak of
   the cascade (its "crown") roughly within −3..+36 dB, and don't let the
   loudest and quietest corner differ by more than ~30 dB.
6. THE SUM IS WHAT IS GATED, and dB add across lanes. A tight pole (r 0.98)
   is roughly a +20..+34 dB peak by itself; two tight poles near the same
   frequency add. Every tight pole must be paid for — by a spanning zero far
   away, a hard null, or simply fewer hot lanes. Estimate the summed peak of
   your design at each corner before you finish it; most designs should
   carry at most two or three genuinely tight poles at any one corner.
7. BASELINE DISCIPLINE — each lane is judged by eye on its own curve, and a
   clean lane returns to ~0 dB away from its features. A pole and zero at
   different radii and different frequencies create an accidental broadband
   shelf on one side (freight nobody designed). Check every lane at 40 Hz
   and at 16 kHz: it should sit near 0 dB at both ends unless tilt is that
   lane's actual job — and at most ONE lane in a design owns tilt,
   deliberately (the factory's spanning lane). A lane whose baseline flips
   sign between corners is a defect, not a move. Rule of thumb: keep a
   pole and its nearby zero at similar radii (carves), or push the zero far
   away ON PURPOSE with the tilt accounted for in the one tilt lane.

## What the factory alphabet actually looks like (measured from 787 factory
sections — use these as your palette, not as rules)

- Zero depth: 20% of zeros sit ON the unit circle (r ≥ 0.995, a hard null);
  49% strong (r 0.9–0.995); 21% soft (0.7–0.9); 10% faint (< 0.7).
- Zero placement: 23% right on the pole (< 3 st away, a carve), 24% within an
  octave, 30% MORE than two octaves from their pole — the "spanning" letter:
  a tight pole with a far unit-circle zero is the factory's loudness and
  tilt machine.
- Poles: 39% very tight (r ≥ 0.99); a third of all poles sit above 8 kHz;
  bass poles below 200 Hz exist but are rare (3%).
- Pole travel on M: median ~2 octaves, long journeys of 4–8 octaves exist.
  Zeros travel too — a famous factory move holds the pole still and walks
  the ZERO 27 semitones.
- Q's usual job: raise pole r toward 0.99+ ("lift") and/or shift pole
  frequency slightly ("revoice").

## The design file format (emit EXACTLY this shape, valid JSON, no extra
fields, no comments)

```json
{
  "schema": "trench-design-v1",
  "name": "short-name",
  "lanes": [
    {
      "slot": 1,
      "pole": { "hz": 900.0, "r": 0.97 },
      "zero": { "interval_st": 7.0, "r": 0.93 },
      "scale_db": 0.0,
      "moves": {
        "M": { "pole_st": 12.0, "zero_st": 0.0 },
        "Q": { "pole_r_to": 0.99 }
      }
    }
  ]
}
```

Field semantics:
- `slot`: 1..7. Order is identity, not priority — lanes keep their slot
  across the whole morph (that is what makes morphs coherent).
- `pole` / `zero`: a conjugate `{hz, r}`, a real pair `{pair: [a, b]}`, or a
  tied zero `{ "interval_st": <semitones from its pole>, "r": <0..1> }`. A
  tied zero rides its pole at that musical interval through every move — use
  it for carves.
- `scale_db`: per-lane static level, keep within ±12 dB, prefer 0.
- `moves`: any subset of `"M"`, `"Q"`, `"T"`. Each move is any subset of:
  `pole_st` (move pole by semitones on that axis), `zero_st` (same for an
  untied zero), `pole_r_to` / `zero_r_to` (radius AT the far end of that
  axis). Moves compose: the M1-Q1 corner = anchor + M move + Q move.
- Real-pair roots do not move; they hold their shape at every corner.
- Omitting `moves` on a lane makes it identical at every corner (an anchor
  lane). Omitting `"T"` everywhere makes the object a .4.

Hard limits: hz within 40..16000; conjugate pole r < 0.998; zero r ≤ 1.0
(exactly 1.0 = a true null, allowed and encouraged when you mean it);
real-pair values strictly inside (−1, 1); at most 7 lanes.

## What to deliver

Propose 5 designs. For each: one factory-style sentence first (like
"Vowel formant filter which sweeps from Oo through Oh to Ah; Q varies the
apparent size of the mouth cavity"), then the JSON. Make the five genuinely
different jobs (e.g. a talker, a bass machine, a hard resonator, a spectral
tilt rider, something nobody has heard). Use the whole palette: spanning
zeros, true nulls, real-pair shoulders, held anchor lanes, zeros that travel
while poles hold. Do not invent fields, types, or laws not stated here.
