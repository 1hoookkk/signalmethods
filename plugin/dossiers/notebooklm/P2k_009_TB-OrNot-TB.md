# TB-OrNot-TB — P2K reference 9 [EQ+]

Architecture source: `P2k_009_TB-OrNot-TB.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**09 TB-OrNot-TB** [EQ+] "Great bassline processor"
- IS: bassline cluster 357–1934 Hz (five sections stacked in two octaves) +
  one air pole. The processor: zeros interleaved INSIDE the cluster carving
  pockets between peaks.
- MORPH: cluster stays (±8 st) while S1 leaps +49 st to become presence —
  the "talk" is one section jumping registers over a stable band.
- Q: NEGATIVE on the two low sections (−0.09, −0.15) — Q cleans the bass
  while (slightly) arming the top. Anti-scream.
- RECIPE: dense low-mid cluster + interleaved zeros + one register-jumper.
  Author Q to trade mud for presence (the mix-engineer axis).
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 578 → 9826 | 0.740 → 0.958 | 0.648 → 0.977 | 357 → 10046 | 0.901 → 0.857 | -4.8/-4.5/-5.8/-1.9 |
| S2 | 1934 → 1394 | 0.950 → 0.880 | 0.801 → 0.974 | 15254 → 0 | 0.839 → 0.977 | -4.8/-4.5/-5.8/-1.9 |
| S3 | 1290 → 2053 | 0.956 → 0.962 | 1.000 → 0.996 | 1932 → 1236 | 0.940 → 0.973 | -4.8/-4.5/-5.8/-1.9 |
| S4 | 15033 → 0 | 0.927 → 0.895 | 0.952 → 0.964 | 1182 → 2007 | 0.897 → 0.978 | -4.8/-4.5/-5.8/-1.9 |
| S5 | 753 → 1156 | 0.969 → 0.952 | 0.993 → 0.973 | 1039 → 563 | 0.871 → 0.971 | -4.8/-4.5/-5.8/-1.9 |
| S6 | 357 → 445 | 0.976 → 0.970 | 0.974 → 0.573 | 3091 → 17961 | 1.000 → 1.000 | -4.8/-4.5/-5.8/-1.9 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 9,
 "name": "TB-OrNot-TB",
 "x3_type": "EQ+",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_009_TB-OrNot-TB.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 577.72,
    "M100_Q0": 9826.47,
    "M0_Q100": 0.0,
    "M100_Q100": 13506.54
   },
   "pole_r": {
    "M0_Q0": 0.739634,
    "M100_Q0": 0.958131,
    "M0_Q100": 0.647805,
    "M100_Q100": 0.977293
   },
   "zero_hz": {
    "M0_Q0": 357.22,
    "M100_Q0": 10045.75,
    "M0_Q100": 357.22,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.901439,
    "M100_Q0": 0.857064,
    "M0_Q100": 0.901439,
    "M100_Q100": 0.978733
   },
   "carve_st": {
    "M0_Q0": -8.32,
    "M100_Q0": 0.38,
    "M0_Q100": null,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 49.06,
   "q_lift_M0": -0.091829,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -4.794,
    "M100_Q0": -4.505,
    "M0_Q100": -5.782,
    "M100_Q100": -1.864
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 1934.1,
    "M100_Q0": 1394.29,
    "M0_Q100": 1134.05,
    "M100_Q100": 10324.02
   },
   "pole_r": {
    "M0_Q0": 0.949942,
    "M100_Q0": 0.879505,
    "M0_Q100": 0.800505,
    "M100_Q100": 0.97429
   },
   "zero_hz": {
    "M0_Q0": 15253.93,
    "M100_Q0": 0.0,
    "M0_Q100": 1038.65,
    "M100_Q100": 13884.75
   },
   "zero_r": {
    "M0_Q0": 0.838635,
    "M100_Q0": 0.976568,
    "M0_Q100": 0.870577,
    "M100_Q100": 0.684831
   },
   "carve_st": {
    "M0_Q0": 35.75,
    "M100_Q0": null,
    "M0_Q100": -1.52,
    "M100_Q100": 5.13
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -5.67,
   "q_lift_M0": -0.149437,
   "q_revoice_st_M0": -9.24,
   "scale_db": {
    "M0_Q0": -4.794,
    "M100_Q0": -4.505,
    "M0_Q100": -5.782,
    "M100_Q100": -1.864
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 1290.02,
    "M100_Q0": 2053.18,
    "M0_Q100": 356.96,
    "M100_Q100": 376.18
   },
   "pole_r": {
    "M0_Q0": 0.95609,
    "M100_Q0": 0.962199,
    "M0_Q100": 0.999527,
    "M100_Q100": 0.99621
   },
   "zero_hz": {
    "M0_Q0": 1932.48,
    "M100_Q0": 1235.72,
    "M0_Q100": 1182.49,
    "M100_Q100": 14135.78
   },
   "zero_r": {
    "M0_Q0": 0.939605,
    "M100_Q0": 0.973287,
    "M0_Q100": 0.897095,
    "M100_Q100": 0.947884
   },
   "carve_st": {
    "M0_Q0": 7.0,
    "M100_Q0": -8.79,
    "M0_Q100": 20.74,
    "M100_Q100": 62.78
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 8.05,
   "q_lift_M0": 0.043437,
   "q_revoice_st_M0": -22.24,
   "scale_db": {
    "M0_Q0": -4.794,
    "M100_Q0": -4.505,
    "M0_Q100": -5.782,
    "M100_Q100": -1.864
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 15032.68,
    "M100_Q0": 0.0,
    "M0_Q100": 15237.84,
    "M100_Q100": 15675.4
   },
   "pole_r": {
    "M0_Q0": 0.927074,
    "M100_Q0": 0.894548,
    "M0_Q100": 0.951996,
    "M100_Q100": 0.964227
   },
   "zero_hz": {
    "M0_Q0": 1182.49,
    "M100_Q0": 2006.74,
    "M0_Q100": 1932.48,
    "M100_Q100": 19531.25
   },
   "zero_r": {
    "M0_Q0": 0.897095,
    "M100_Q0": 0.978291,
    "M0_Q100": 0.939605,
    "M100_Q100": 0.383842
   },
   "carve_st": {
    "M0_Q0": -44.02,
    "M100_Q0": null,
    "M0_Q100": -35.75,
    "M100_Q100": 3.81
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": null,
   "q_lift_M0": 0.024922,
   "q_revoice_st_M0": 0.23,
   "scale_db": {
    "M0_Q0": -4.794,
    "M100_Q0": -4.505,
    "M0_Q100": -5.782,
    "M100_Q100": -1.864
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 752.59,
    "M100_Q0": 1156.16,
    "M0_Q100": 754.14,
    "M100_Q100": 13314.24
   },
   "pole_r": {
    "M0_Q0": 0.969266,
    "M100_Q0": 0.951996,
    "M0_Q100": 0.993143,
    "M100_Q100": 0.973287
   },
   "zero_hz": {
    "M0_Q0": 1038.65,
    "M100_Q0": 562.95,
    "M0_Q100": 15253.93,
    "M100_Q100": 12765.54
   },
   "zero_r": {
    "M0_Q0": 0.870577,
    "M100_Q0": 0.971279,
    "M0_Q100": 0.838635,
    "M100_Q100": 0.937524
   },
   "carve_st": {
    "M0_Q0": 5.58,
    "M100_Q0": -12.46,
    "M0_Q100": 52.06,
    "M100_Q100": -0.73
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 7.43,
   "q_lift_M0": 0.023877,
   "q_revoice_st_M0": 0.04,
   "scale_db": {
    "M0_Q0": -4.794,
    "M100_Q0": -4.505,
    "M0_Q100": -5.782,
    "M100_Q100": -1.864
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 357.13,
    "M100_Q0": 445.17,
    "M0_Q100": 1969.36,
    "M100_Q100": 17927.62
   },
   "pole_r": {
    "M0_Q0": 0.976293,
    "M100_Q0": 0.970273,
    "M0_Q100": 0.97429,
    "M100_Q100": 0.573035
   },
   "zero_hz": {
    "M0_Q0": 3090.69,
    "M100_Q0": 17960.83,
    "M0_Q100": 13476.05,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 37.36,
    "M100_Q0": 64.01,
    "M0_Q100": 33.3,
    "M100_Q100": 0.03
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": 3.81,
   "q_lift_M0": -0.002003,
   "q_revoice_st_M0": 29.56,
   "scale_db": {
    "M0_Q0": -4.794,
    "M100_Q0": -4.505,
    "M0_Q100": -5.782,
    "M100_Q100": -1.864
   }
  }
 ]
}
```
