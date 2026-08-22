# Lane gaps — adjacent-pole spacing in the P2K references

Measured 2026-08-08 by `tools/measure_lane_gaps.py` over
`dossiers/characters/P2k_*.json` (33 references, both authored morph
corners M0_Q0 / M100_Q0, live conjugate poles hz > 20 with r >= 0.5,
sorted by Hz per corner). 318 adjacent gaps, mean 5.8 voices per corner.

| spacing | count | share |
|---|---|---|
| touching, <= 2 st | 44 | 14% |
| cluster, 2-5 st | 79 | 25% |
| spread, 5-12 st | 108 | 34% |
| wide, 1-2 octaves | 41 | 13% |
| frame gap, > 2 octaves | 46 | 14% |

Close (<= 5 st): 39%. Far (> 1 octave): 27%. Median gap 7.1 st.

The typical six-voice corner: two close pairs (near enough that one
lane's zero lands on its neighbour's ring and the multiplied response
pinches), one or two frame-scale loners (anchor or air, 2+ octaves out),
the rest in the 5-12 st middle.

The distribution falls off smoothly from touching to frame gap — no
two-camp split, no quantising to musical intervals. Design consequence:
no snap-to-cluster or snap-to-interval behaviour anywhere; the
references themselves do not snap.
