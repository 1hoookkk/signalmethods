# EarBender — P2K reference 31 [WAH]

Architecture source: `P2k_031_EarBender.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**31 EarBender** [WAH] "Between wah & vowel, strong mid-boost"
- IS: M0 = MegaSweepz's air frame (shared vocabulary). M100 = the bend: a
  BEATING PAIR — 5910+5521 (~1.2 st apart) with 6272 at r=0.48 as a wide
  shoulder, plus 738/814 (1.7 st) low pair. Two clusters of adjacent poles
  fighting = the "uncomfortable."
- MORPH: air frame collapses into the beat clusters (−14..−51 st, 9
  crossings — maximum turbulence into a nasty landing).
- Q: broad lift (+0.07..+0.15) — the beat pairs tighten toward unison ring.
- RECIPE: the wah IS the beat — author pole pairs deliberately ~1 st apart
  (nothing in our taxonomy does this); Q narrows the beat. Directly
  measurable against a real parked wah (SOURCE_PLAN).

---
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 302 → 6272 | 0.857 → 0.484 | 0.997 → 0.992 | 10959 → 839 | 0.857 → 0.970 | -6.8/-4.3/-16.4/-6.0 |
| S2 | 3723 → 738 | 0.927 → 0.968 | 0.994 → 0.992 | 13401 → 1619 | 0.986 → 0.972 | -6.8/-4.3/-16.4/-6.0 |
| S3 | 10283 → 2384 | 0.888 → 0.952 | 0.880 → 0.914 | 14391 → 2374 | 0.968 → 0.968 | -6.8/-4.3/-16.4/-6.0 |
| S4 | 13571 → 5910 | 0.910 → 0.968 | 0.994 → 0.973 | 16287 → 4948 | 0.919 → 0.988 | -6.8/-4.3/-16.4/-6.0 |
| S5 | 14394 → 5521 | 0.910 → 0.972 | 0.994 → 0.990 | 16811 → 5605 | 0.638 → 0.987 | -6.8/-4.3/-16.4/-6.0 |
| S6 | 15837 → 814 | 0.848 → 0.968 | 0.994 → 0.500 | 3039 → 17961 | 1.000 → 1.000 | -6.8/-4.3/-16.4/-6.0 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 31,
 "name": "EarBender",
 "x3_type": "WAH",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_031_EarBender.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 301.75,
    "M100_Q0": 6271.61,
    "M0_Q100": 45.45,
    "M100_Q100": 5713.54
   },
   "pole_r": {
    "M0_Q0": 0.857064,
    "M100_Q0": 0.484375,
    "M0_Q100": 0.997312,
    "M100_Q100": 0.992406
   },
   "zero_hz": {
    "M0_Q0": 10958.81,
    "M100_Q0": 839.37,
    "M0_Q100": 10958.81,
    "M100_Q100": 5604.75
   },
   "zero_r": {
    "M0_Q0": 0.857064,
    "M100_Q0": 0.970273,
    "M0_Q100": 0.857064,
    "M100_Q100": 0.987229
   },
   "carve_st": {
    "M0_Q0": 62.19,
    "M100_Q0": -34.82,
    "M0_Q100": 94.96,
    "M100_Q100": -0.33
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 52.53,
   "q_lift_M0": 0.140248,
   "q_revoice_st_M0": -32.77,
   "scale_db": {
    "M0_Q0": -6.779,
    "M100_Q0": -4.339,
    "M0_Q100": -16.368,
    "M100_Q100": -6.018
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 3722.88,
    "M100_Q0": 738.19,
    "M0_Q100": 10190.38,
    "M100_Q100": 2034.81
   },
   "pole_r": {
    "M0_Q0": 0.927074,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.994126,
    "M100_Q100": 0.992406
   },
   "zero_hz": {
    "M0_Q0": 13401.26,
    "M100_Q0": 1619.34,
    "M0_Q100": 13401.26,
    "M100_Q100": 1619.34
   },
   "zero_r": {
    "M0_Q0": 0.986239,
    "M100_Q0": 0.972284,
    "M0_Q100": 0.986239,
    "M100_Q100": 0.972284
   },
   "carve_st": {
    "M0_Q0": 22.17,
    "M100_Q0": 13.6,
    "M0_Q100": 4.74,
    "M100_Q100": -3.95
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -28.01,
   "q_lift_M0": 0.067052,
   "q_revoice_st_M0": 17.43,
   "scale_db": {
    "M0_Q0": -6.779,
    "M100_Q0": -4.339,
    "M0_Q100": -16.368,
    "M100_Q100": -6.018
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 10282.75,
    "M100_Q0": 2383.56,
    "M0_Q100": 12653.42,
    "M100_Q100": 1961.59
   },
   "pole_r": {
    "M0_Q0": 0.888343,
    "M100_Q0": 0.951996,
    "M0_Q100": 0.879505,
    "M100_Q100": 0.914346
   },
   "zero_hz": {
    "M0_Q0": 14391.16,
    "M100_Q0": 2374.4,
    "M0_Q100": 14391.16,
    "M100_Q100": 2374.4
   },
   "zero_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.968258,
    "M100_Q100": 0.968258
   },
   "carve_st": {
    "M0_Q0": 5.82,
    "M100_Q0": -0.07,
    "M0_Q100": 2.23,
    "M100_Q100": 3.31
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -25.31,
   "q_lift_M0": -0.008838,
   "q_revoice_st_M0": 3.59,
   "scale_db": {
    "M0_Q0": -6.779,
    "M100_Q0": -4.339,
    "M0_Q100": -16.368,
    "M100_Q100": -6.018
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13570.98,
    "M100_Q0": 5910.28,
    "M0_Q100": 13545.53,
    "M100_Q100": 4553.07
   },
   "pole_r": {
    "M0_Q0": 0.910064,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.994126,
    "M100_Q100": 0.973287
   },
   "zero_hz": {
    "M0_Q0": 16286.92,
    "M100_Q0": 4947.63,
    "M0_Q100": 16286.92,
    "M100_Q100": 4947.63
   },
   "zero_r": {
    "M0_Q0": 0.918608,
    "M100_Q0": 0.987723,
    "M0_Q100": 0.918608,
    "M100_Q100": 0.987723
   },
   "carve_st": {
    "M0_Q0": 3.16,
    "M100_Q0": -3.08,
    "M0_Q100": 3.19,
    "M100_Q100": 1.44
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -14.39,
   "q_lift_M0": 0.084062,
   "q_revoice_st_M0": -0.03,
   "scale_db": {
    "M0_Q0": -6.779,
    "M100_Q0": -4.339,
    "M0_Q100": -16.368,
    "M100_Q100": -6.018
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 14394.08,
    "M100_Q0": 5520.7,
    "M0_Q100": 14559.87,
    "M100_Q100": 246.05
   },
   "pole_r": {
    "M0_Q0": 0.910064,
    "M100_Q0": 0.972284,
    "M0_Q100": 0.994126,
    "M100_Q100": 0.990192
   },
   "zero_hz": {
    "M0_Q0": 16811.22,
    "M100_Q0": 5604.75,
    "M0_Q100": 19531.25,
    "M100_Q100": 839.37
   },
   "zero_r": {
    "M0_Q0": 0.637569,
    "M100_Q0": 0.987229,
    "M0_Q100": 0.883116,
    "M100_Q100": 0.970273
   },
   "carve_st": {
    "M0_Q0": 2.69,
    "M100_Q0": 0.26,
    "M0_Q100": 5.09,
    "M100_Q100": 21.24
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -16.59,
   "q_lift_M0": 0.084062,
   "q_revoice_st_M0": 0.2,
   "scale_db": {
    "M0_Q0": -6.779,
    "M100_Q0": -4.339,
    "M0_Q100": -16.368,
    "M100_Q100": -6.018
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 15836.84,
    "M100_Q0": 813.6,
    "M0_Q100": 16131.11,
    "M100_Q100": 6281.07
   },
   "pole_r": {
    "M0_Q0": 0.847899,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.994126,
    "M100_Q100": 0.500244
   },
   "zero_hz": {
    "M0_Q0": 3039.38,
    "M100_Q0": 17960.83,
    "M0_Q100": 17960.83,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": -28.58,
    "M100_Q0": 53.57,
    "M0_Q100": 1.86,
    "M100_Q100": 18.19
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -51.39,
   "q_lift_M0": 0.146227,
   "q_revoice_st_M0": 0.32,
   "scale_db": {
    "M0_Q0": -6.779,
    "M100_Q0": -4.339,
    "M0_Q100": -16.368,
    "M100_Q100": -6.018
   }
  }
 ]
}
```
