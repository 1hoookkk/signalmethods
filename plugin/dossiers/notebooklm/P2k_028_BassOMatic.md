# BassOMatic — P2K reference 28 [REZ]

Architecture source: `P2k_028_BassOMatic.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**28 BassOMatic** [REZ] "Low boost; Q distorts at max"
- IS: bass cluster with S4 parked as a REAL pole at r=0.65 (lurker); zeros
  cap at 1932/1039.
- MORPH: modest reshuffle (±17 st), two sections go real (leave the ring
  domain entirely) — the cliff edge.
- Q: S4 +0.35 — the lurker becomes a screamer; at Q max its energy is what
  "goes to distortion" (feeds SLAM/AGC hot).
- RECIPE: single hidden section with a huge Q arc inside an honest bass EQ —
  BassTracer's trick with a real-pole start. The cliff IS the feature; let
  it clip the drive stage.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 15274 → 9856 | 0.950 → 0.985 | 0.999 → 0.927 | 357 → 0 | 0.901 → 0.977 | -5.5/-7.5/-5.3/1.1 |
| S2 | 122 → 329 | 0.991 → 0.996 | 0.999 → 0.914 | 1039 → 563 | 0.871 → 0.971 | -5.5/-7.5/-5.3/1.1 |
| S3 | 754 → 1131 | 0.992 → 0.962 | 0.995 → 0.946 | 1182 → 1236 | 0.897 → 0.973 | -5.5/-7.5/-5.3/1.1 |
| S4 | 0 → 2049 | 0.648 → 0.972 | 0.999 → 0.978 | 1932 → 2007 | 0.940 → 0.978 | -5.5/-7.5/-5.3/1.1 |
| S5 | 1298 → 627 | 0.979 → 0.935 | 0.962 → 0.978 | 15254 → 10046 | 0.839 → 0.857 | -5.5/-7.5/-5.3/1.1 |
| S6 | 1970 → 0 | 0.972 → 0.907 | 0.934 → 0.866 | 3039 → 13476 | 1.000 → 1.000 | -5.5/-19.6/-17.3/-17.0 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 28,
 "name": "BassOMatic",
 "x3_type": "REZ",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_028_BassOMatic.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 15274.31,
    "M100_Q0": 9855.71,
    "M0_Q100": 9574.92,
    "M100_Q100": 12640.08
   },
   "pole_r": {
    "M0_Q0": 0.949942,
    "M100_Q0": 0.985248,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.927074
   },
   "zero_hz": {
    "M0_Q0": 357.22,
    "M100_Q0": 0.0,
    "M0_Q100": 0.0,
    "M100_Q100": 12366.78
   },
   "zero_r": {
    "M0_Q0": 0.901439,
    "M100_Q0": 0.97718,
    "M0_Q100": 0.97718,
    "M100_Q100": 0.931278
   },
   "carve_st": {
    "M0_Q0": -65.02,
    "M100_Q0": null,
    "M0_Q100": null,
    "M100_Q100": -0.38
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -7.58,
   "q_lift_M0": 0.049081,
   "q_revoice_st_M0": -8.09,
   "scale_db": {
    "M0_Q0": -5.52,
    "M100_Q0": -7.549,
    "M0_Q100": -5.301,
    "M100_Q100": 1.098
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 121.63,
    "M100_Q0": 329.04,
    "M0_Q100": 388.7,
    "M100_Q100": 8397.24
   },
   "pole_r": {
    "M0_Q0": 0.991178,
    "M100_Q0": 0.995844,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.914346
   },
   "zero_hz": {
    "M0_Q0": 1038.65,
    "M100_Q0": 562.95,
    "M0_Q100": 562.95,
    "M100_Q100": 8720.93
   },
   "zero_r": {
    "M0_Q0": 0.870577,
    "M100_Q0": 0.971279,
    "M0_Q100": 0.971279,
    "M100_Q100": 0.750122
   },
   "carve_st": {
    "M0_Q0": 37.13,
    "M100_Q0": 9.3,
    "M0_Q100": 6.41,
    "M100_Q100": 0.65
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 17.23,
   "q_lift_M0": 0.007845,
   "q_revoice_st_M0": 20.11,
   "scale_db": {
    "M0_Q0": -5.52,
    "M100_Q0": -7.549,
    "M0_Q100": -5.301,
    "M100_Q100": 1.098
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 754.14,
    "M100_Q0": 1130.91,
    "M0_Q100": 1351.31,
    "M100_Q100": 9904.3
   },
   "pole_r": {
    "M0_Q0": 0.99216,
    "M100_Q0": 0.962199,
    "M0_Q100": 0.995108,
    "M100_Q100": 0.945821
   },
   "zero_hz": {
    "M0_Q0": 1182.49,
    "M100_Q0": 1235.72,
    "M0_Q100": 1235.72,
    "M100_Q100": 11098.99
   },
   "zero_r": {
    "M0_Q0": 0.897095,
    "M100_Q0": 0.973287,
    "M0_Q100": 0.973287,
    "M100_Q100": 0.951996
   },
   "carve_st": {
    "M0_Q0": 7.79,
    "M100_Q0": 1.53,
    "M0_Q100": -1.55,
    "M100_Q100": 1.97
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 7.01,
   "q_lift_M0": 0.002948,
   "q_revoice_st_M0": 10.1,
   "scale_db": {
    "M0_Q0": -5.52,
    "M100_Q0": -7.549,
    "M0_Q100": -5.301,
    "M100_Q100": 1.098
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 2048.95,
    "M0_Q100": 2028.55,
    "M100_Q100": 11718.26
   },
   "pole_r": {
    "M0_Q0": 0.647805,
    "M100_Q0": 0.972284,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.978291
   },
   "zero_hz": {
    "M0_Q0": 1932.48,
    "M100_Q0": 2006.74,
    "M0_Q100": 2006.74,
    "M100_Q100": 12237.16
   },
   "zero_r": {
    "M0_Q0": 0.939605,
    "M100_Q0": 0.978291,
    "M0_Q100": 0.978291,
    "M100_Q100": 0.684831
   },
   "carve_st": {
    "M0_Q0": null,
    "M100_Q0": -0.36,
    "M0_Q100": -0.19,
    "M100_Q100": 0.75
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": null,
   "q_lift_M0": 0.351218,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -5.52,
    "M100_Q0": -7.549,
    "M0_Q100": -5.301,
    "M100_Q100": 1.098
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 1297.76,
    "M100_Q0": 626.72,
    "M0_Q100": 1293.73,
    "M100_Q100": 1031.35
   },
   "pole_r": {
    "M0_Q0": 0.979289,
    "M100_Q0": 0.935439,
    "M0_Q100": 0.962199,
    "M100_Q100": 0.978291
   },
   "zero_hz": {
    "M0_Q0": 15253.93,
    "M100_Q0": 10045.75,
    "M0_Q100": 10045.75,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.838635,
    "M100_Q0": 0.857064,
    "M0_Q100": 0.857064,
    "M100_Q100": 0.979415
   },
   "carve_st": {
    "M0_Q0": 42.66,
    "M100_Q0": 48.03,
    "M0_Q100": 35.48,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -12.6,
   "q_lift_M0": -0.01709,
   "q_revoice_st_M0": -0.05,
   "scale_db": {
    "M0_Q0": -5.52,
    "M100_Q0": -7.549,
    "M0_Q100": -5.301,
    "M100_Q100": 1.098
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 1970.31,
    "M100_Q0": 0.0,
    "M0_Q100": 0.0,
    "M100_Q100": 14772.06
   },
   "pole_r": {
    "M0_Q0": 0.972284,
    "M100_Q0": 0.906972,
    "M0_Q100": 0.934204,
    "M100_Q100": 0.866078
   },
   "zero_hz": {
    "M0_Q0": 3039.38,
    "M100_Q0": 13476.05,
    "M0_Q100": 12152.16,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 7.5,
    "M100_Q0": null,
    "M0_Q100": null,
    "M100_Q100": 3.38
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": null,
   "q_lift_M0": -0.03808,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -5.52,
    "M100_Q0": -19.59,
    "M0_Q100": -17.342,
    "M100_Q100": -16.964
   }
  }
 ]
}
```
