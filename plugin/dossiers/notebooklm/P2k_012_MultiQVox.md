# MultiQVox — P2K reference 12 [VOW]

Architecture source: `P2k_012_MultiQVox.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**12 MultiQVox** [VOW] "Multi-formant, map Q to velocity"
- IS: textbook vowel: formants at 1787/2898/4853/8204 with the mouth-zero
  carve; S1 wide-open r=0.35 (a broadband mouth floor).
- MORPH: formants nearly static (±3 st) except S2 dives −32 st and S6 rises
  +27 — ONE articulator moves inside a held face.
- Q: S2 −0.16, rest ≈0 — Q RELAXES the moving formant: velocity mapping
  makes loud notes rounder, not sharper. Inverted expressiveness.
- RECIPE: hold the face, move one articulator on M, put a NEGATIVE Q on that
  same articulator. The playable-vowel spec (velocity → Q) is the point.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 6288 → 7001 | 0.354 → 0.927 | 0.415 → 0.662 | 702 → 1236 | 0.991 → 0.935 | -4.9/-4.0/-5.0/-2.1 |
| S2 | 1787 → 274 | 0.999 → 0.983 | 0.839 → 0.866 | 1713 → 781 | 0.976 → 0.910 | -4.9/-4.0/-5.0/-2.1 |
| S3 | 2898 → 3533 | 0.987 → 0.993 | 0.999 → 0.999 | 3754 → 3652 | 0.897 → 0.976 | -4.9/-4.0/-5.0/-2.1 |
| S4 | 4853 → 5083 | 0.969 → 0.857 | 0.978 → 0.906 | 7903 → 5594 | 0.531 → 0.946 | -4.9/-4.0/-5.0/-2.1 |
| S5 | 8204 → 9081 | 0.998 → 0.946 | 0.999 → 0.999 | 8327 → 9209 | 0.972 → 0.923 | -4.9/-4.0/-5.0/-2.1 |
| S6 | 225 → 1103 | 0.994 → 0.994 | 0.999 → 0.999 | 6282 → 6282 | 1.000 → 1.000 | -4.9/-4.0/-5.0/-2.1 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 12,
 "name": "MultiQVox",
 "x3_type": "VOW",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_012_MultiQVox.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 6287.86,
    "M100_Q0": 7000.68,
    "M0_Q100": 6975.67,
    "M100_Q100": 10349.18
   },
   "pole_r": {
    "M0_Q0": 0.353898,
    "M100_Q0": 0.927074,
    "M0_Q100": 0.414872,
    "M100_Q100": 0.661622
   },
   "zero_hz": {
    "M0_Q0": 701.65,
    "M100_Q0": 1235.54,
    "M0_Q100": 6090.39,
    "M100_Q100": 6513.04
   },
   "zero_r": {
    "M0_Q0": 0.991178,
    "M100_Q0": 0.935439,
    "M0_Q100": 0.987723,
    "M100_Q100": 0.998901
   },
   "carve_st": {
    "M0_Q0": -37.96,
    "M100_Q0": -30.03,
    "M0_Q100": -2.35,
    "M100_Q100": -8.02
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 1.86,
   "q_lift_M0": 0.060974,
   "q_revoice_st_M0": 1.8,
   "scale_db": {
    "M0_Q0": -4.861,
    "M100_Q0": -4.006,
    "M0_Q100": -5.028,
    "M100_Q100": -2.106
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 1787.39,
    "M100_Q0": 274.45,
    "M0_Q100": 980.43,
    "M100_Q100": 4752.23
   },
   "pole_r": {
    "M0_Q0": 0.998718,
    "M100_Q0": 0.98327,
    "M0_Q100": 0.838635,
    "M100_Q100": 0.866078
   },
   "zero_hz": {
    "M0_Q0": 1712.52,
    "M100_Q0": 781.49,
    "M0_Q100": 3100.98,
    "M100_Q100": 3008.78
   },
   "zero_r": {
    "M0_Q0": 0.976293,
    "M100_Q0": 0.910064,
    "M0_Q100": 0.951996,
    "M100_Q100": 0.838635
   },
   "carve_st": {
    "M0_Q0": -0.74,
    "M100_Q0": 18.12,
    "M0_Q100": 19.93,
    "M100_Q100": -7.91
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -32.44,
   "q_lift_M0": -0.160083,
   "q_revoice_st_M0": -10.4,
   "scale_db": {
    "M0_Q0": -4.861,
    "M100_Q0": -4.006,
    "M0_Q100": -5.028,
    "M100_Q100": -2.106
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 2898.18,
    "M100_Q0": 3533.03,
    "M0_Q100": 3337.52,
    "M100_Q100": 3040.89
   },
   "pole_r": {
    "M0_Q0": 0.987229,
    "M100_Q0": 0.993389,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.999023
   },
   "zero_hz": {
    "M0_Q0": 3754.5,
    "M100_Q0": 3652.41,
    "M0_Q100": 1438.4,
    "M100_Q100": 2103.9
   },
   "zero_r": {
    "M0_Q0": 0.897095,
    "M100_Q0": 0.976293,
    "M0_Q100": 0.984753,
    "M100_Q100": 0.888343
   },
   "carve_st": {
    "M0_Q0": 4.48,
    "M100_Q0": 0.58,
    "M0_Q100": -14.57,
    "M100_Q100": -6.38
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 3.43,
   "q_lift_M0": 0.011794,
   "q_revoice_st_M0": 2.44,
   "scale_db": {
    "M0_Q0": -4.861,
    "M100_Q0": -4.006,
    "M0_Q100": -5.028,
    "M100_Q100": -2.106
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 4852.84,
    "M100_Q0": 5082.98,
    "M0_Q100": 4542.01,
    "M100_Q100": 6224.12
   },
   "pole_r": {
    "M0_Q0": 0.969266,
    "M100_Q0": 0.857064,
    "M0_Q100": 0.978291,
    "M100_Q100": 0.905762
   },
   "zero_hz": {
    "M0_Q0": 7903.42,
    "M100_Q0": 5593.91,
    "M0_Q100": 2211.13,
    "M100_Q100": 1122.85
   },
   "zero_r": {
    "M0_Q0": 0.53056,
    "M100_Q0": 0.945821,
    "M0_Q100": 0.99829,
    "M100_Q100": 0.901439
   },
   "carve_st": {
    "M0_Q0": 8.44,
    "M100_Q0": 1.66,
    "M0_Q100": -12.46,
    "M100_Q100": -29.65
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 0.8,
   "q_lift_M0": 0.009025,
   "q_revoice_st_M0": -1.15,
   "scale_db": {
    "M0_Q0": -4.861,
    "M100_Q0": -4.006,
    "M0_Q100": -5.028,
    "M100_Q100": -2.106
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 8203.54,
    "M100_Q0": 9081.27,
    "M0_Q100": 1829.51,
    "M100_Q100": 1787.12
   },
   "pole_r": {
    "M0_Q0": 0.997679,
    "M100_Q0": 0.945821,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.999023
   },
   "zero_hz": {
    "M0_Q0": 8326.69,
    "M100_Q0": 9209.49,
    "M0_Q100": 1559.71,
    "M100_Q100": 509.29
   },
   "zero_r": {
    "M0_Q0": 0.972284,
    "M100_Q0": 0.922851,
    "M0_Q100": 0.939605,
    "M100_Q100": 0.760466
   },
   "carve_st": {
    "M0_Q0": 0.26,
    "M100_Q0": 0.24,
    "M0_Q100": -2.76,
    "M100_Q100": -21.73
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 1.76,
   "q_lift_M0": 0.001344,
   "q_revoice_st_M0": -25.98,
   "scale_db": {
    "M0_Q0": -4.861,
    "M100_Q0": -4.006,
    "M0_Q100": -5.028,
    "M100_Q100": -2.106
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 225.0,
    "M100_Q0": 1102.81,
    "M0_Q100": 932.81,
    "M100_Q100": 412.23
   },
   "pole_r": {
    "M0_Q0": 0.993635,
    "M100_Q0": 0.994372,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.999023
   },
   "zero_hz": {
    "M0_Q0": 6282.31,
    "M100_Q0": 6282.31,
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
    "M0_Q0": 57.64,
    "M100_Q0": 30.12,
    "M0_Q100": 51.21,
    "M100_Q100": 65.34
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": 27.52,
   "q_lift_M0": 0.005388,
   "q_revoice_st_M0": 24.62,
   "scale_db": {
    "M0_Q0": -4.861,
    "M100_Q0": -4.006,
    "M0_Q100": -5.028,
    "M100_Q100": -2.106
   }
  }
 ]
}
```
