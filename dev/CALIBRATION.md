# The calibration ledger

Every face control returns through one of two doors: **born calibrated** (its unit is
already perceptual) or **measured** (a bisection session writes dev/curves/<axis>_*.curve.json,
gen_curves.py bakes it under the parameter, the quarter-turn test passes). The knob-space
value is the unit of record; param IDs never change.

Sessions: workstation axes in the app's eyes-closed room (`trench.cmd --bisect <axis> --body <p>`);
plugin axes in TRENCH_Bisect against the shipping engine. Programme material: the saw riff
and the canonical drum loop (TBD by Tyson — checked in once, kept forever). A curve measured
on one programme is verified on the other by matching, not re-bisection.

Acceptance: the quarter-turn test — 25% jumps at random dial positions sound like the same
amount of change, eyes closed.

| control | door | status |
| --- | --- | --- |
| KEY, TRACK display, OUT, RATE | born (semitones / dB / divisions) | done |
| MIX | born while plain dry/wet | done |
| Morph | session | awaiting (start: 303 body, both rooms, compare) |
| Q | session | awaiting |
| xForm (drive + z bundle) | session, as the bundle | blocked on the bundle existing |
| FOLLOW | session (drum loop programme) | awaiting |
| PATTERN | tournament + grouped browser | awaiting ears |
| INPUT knob, GAIN tab | evicted from the face | closed |

Rulings behind this: every TRENCH parameter calibrated by Martens bisection; measure and
invert, never model; axes are authored by Tyson, measurement only makes them honest.
