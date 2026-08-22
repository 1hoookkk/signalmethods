# ZoomPeaks — P2K reference 14 [REZ]

Architecture source: `P2k_014_ZoomPeaks.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**14 ZoomPeaks** [REZ] "High resonance nasal filter"
- IS: nasal = harmonic-ish pole train 357/1048/1749/2493 all at r≈1.0 with
  deep scale cuts (−18/−24 dB) — a comb that rings.
- MORPH: core train rides up in parallel (+9..+23 st) while S1 rockets
  +70 st and S6 falls −56: outer sections RELAY to keep the ends framed
  while the middle zooms.
- Q: only S6 +0.13 — the frame blooms, the train doesn't.
- RECIPE: parallel train + framing relay. Daylight: tune the train to a
  chord; ROM's is quasi-harmonic on no particular root.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 90 → 5093 | 0.998 → 0.927 | 0.927 → 0.950 | 425 → 415 | 0.942 → 0.960 | -18.1/-1.6/-2.4/3.8 |
| S2 | 357 → 1322 | 1.000 → 0.997 | 0.999 → 0.986 | 3581 → 1231 | 0.829 → 0.962 | -18.1/-1.6/-2.4/3.8 |
| S3 | 1048 → 2219 | 0.998 → 0.990 | 0.996 → 0.966 | 1760 → 2207 | 0.954 → 0.954 | -24.1/-13.6/-14.4/-8.3 |
| S4 | 1749 → 3261 | 0.992 → 0.987 | 0.987 → 0.962 | 2509 → 3189 | 0.964 → 0.966 | -18.1/-1.6/-2.4/3.8 |
| S5 | 2493 → 4139 | 0.982 → 0.987 | 0.994 → 0.952 | 16781 → 4244 | 0.829 → 0.974 | -18.1/-1.6/-2.4/3.8 |
| S6 | 16748 → 645 | 0.866 → 0.998 | 0.999 → 0.650 | 10935 → 17961 | 1.000 → 1.000 | -24.1/-13.6/-14.4/-8.3 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 14,
 "name": "ZoomPeaks",
 "x3_type": "REZ",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_014_ZoomPeaks.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 90.02,
    "M100_Q0": 5092.69,
    "M0_Q100": 5092.69,
    "M100_Q100": 12637.07
   },
   "pole_r": {
    "M0_Q0": 0.998046,
    "M100_Q0": 0.927074,
    "M0_Q100": 0.927074,
    "M100_Q100": 0.949942
   },
   "zero_hz": {
    "M0_Q0": 425.47,
    "M100_Q0": 414.66,
    "M0_Q100": 414.66,
    "M100_Q100": 12366.78
   },
   "zero_r": {
    "M0_Q0": 0.941682,
    "M100_Q0": 0.960167,
    "M0_Q100": 0.960167,
    "M100_Q100": 0.931278
   },
   "carve_st": {
    "M0_Q0": 26.89,
    "M100_Q0": -43.42,
    "M0_Q100": -43.42,
    "M100_Q100": -0.37
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 69.86,
   "q_lift_M0": -0.070972,
   "q_revoice_st_M0": 69.86,
   "scale_db": {
    "M0_Q0": -18.087,
    "M100_Q0": -1.581,
    "M0_Q100": -2.375,
    "M100_Q100": 3.781
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 356.92,
    "M100_Q0": 1321.57,
    "M0_Q100": 1377.2,
    "M100_Q100": 1437.9
   },
   "pole_r": {
    "M0_Q0": 0.999786,
    "M100_Q0": 0.99719,
    "M0_Q100": 0.998596,
    "M100_Q100": 0.986239
   },
   "zero_hz": {
    "M0_Q0": 3580.53,
    "M100_Q0": 1231.16,
    "M0_Q100": 1231.16,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.829267,
    "M100_Q0": 0.962199,
    "M0_Q100": 0.962199,
    "M100_Q100": 0.979415
   },
   "carve_st": {
    "M0_Q0": 39.92,
    "M100_Q0": -1.23,
    "M0_Q100": -1.94,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 22.66,
   "q_lift_M0": -0.00119,
   "q_revoice_st_M0": 23.38,
   "scale_db": {
    "M0_Q0": -18.087,
    "M100_Q0": -1.581,
    "M0_Q100": -2.375,
    "M100_Q100": 3.781
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 1048.11,
    "M100_Q0": 2219.41,
    "M0_Q100": 2282.27,
    "M100_Q100": 9976.8
   },
   "pole_r": {
    "M0_Q0": 0.99829,
    "M100_Q0": 0.990192,
    "M0_Q100": 0.995844,
    "M100_Q100": 0.96625
   },
   "zero_hz": {
    "M0_Q0": 1760.2,
    "M100_Q0": 2206.66,
    "M0_Q100": 2206.66,
    "M100_Q100": 11098.99
   },
   "zero_r": {
    "M0_Q0": 0.954045,
    "M100_Q0": 0.954045,
    "M0_Q100": 0.954045,
    "M100_Q100": 0.951996
   },
   "carve_st": {
    "M0_Q0": 8.98,
    "M100_Q0": -0.1,
    "M0_Q100": -0.58,
    "M100_Q100": 1.85
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 12.99,
   "q_lift_M0": -0.002446,
   "q_revoice_st_M0": 13.47,
   "scale_db": {
    "M0_Q0": -24.108,
    "M100_Q0": -13.622,
    "M0_Q100": -14.416,
    "M100_Q100": -8.26
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 1749.46,
    "M100_Q0": 3260.69,
    "M0_Q100": 4139.42,
    "M100_Q100": 11227.0
   },
   "pole_r": {
    "M0_Q0": 0.99167,
    "M100_Q0": 0.986734,
    "M0_Q100": 0.986734,
    "M100_Q100": 0.962199
   },
   "zero_hz": {
    "M0_Q0": 2508.63,
    "M100_Q0": 3189.38,
    "M0_Q100": 3189.38,
    "M100_Q100": 12237.16
   },
   "zero_r": {
    "M0_Q0": 0.964227,
    "M100_Q0": 0.96625,
    "M0_Q100": 0.96625,
    "M100_Q100": 0.684831
   },
   "carve_st": {
    "M0_Q0": 6.24,
    "M100_Q0": -0.38,
    "M0_Q100": -4.51,
    "M100_Q100": 1.49
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 10.78,
   "q_lift_M0": -0.004936,
   "q_revoice_st_M0": 14.91,
   "scale_db": {
    "M0_Q0": -18.087,
    "M100_Q0": -1.581,
    "M0_Q100": -2.375,
    "M100_Q100": 3.781
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 2493.0,
    "M100_Q0": 4139.42,
    "M0_Q100": 3249.54,
    "M100_Q100": 8426.24
   },
   "pole_r": {
    "M0_Q0": 0.982276,
    "M100_Q0": 0.986734,
    "M0_Q100": 0.993881,
    "M100_Q100": 0.951996
   },
   "zero_hz": {
    "M0_Q0": 16780.67,
    "M100_Q0": 4243.96,
    "M0_Q100": 4243.96,
    "M100_Q100": 8720.93
   },
   "zero_r": {
    "M0_Q0": 0.829267,
    "M100_Q0": 0.97429,
    "M0_Q100": 0.97429,
    "M100_Q100": 0.750122
   },
   "carve_st": {
    "M0_Q0": 33.01,
    "M100_Q0": 0.43,
    "M0_Q100": 4.62,
    "M100_Q100": 0.6
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 8.78,
   "q_lift_M0": 0.011605,
   "q_revoice_st_M0": 4.59,
   "scale_db": {
    "M0_Q0": -18.087,
    "M100_Q0": -1.581,
    "M0_Q100": -2.375,
    "M100_Q100": 3.781
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 16747.8,
    "M100_Q0": 645.04,
    "M0_Q100": 455.72,
    "M100_Q100": 14452.43
   },
   "pole_r": {
    "M0_Q0": 0.866078,
    "M100_Q0": 0.997802,
    "M0_Q100": 0.999481,
    "M100_Q100": 0.649707
   },
   "zero_hz": {
    "M0_Q0": 10935.17,
    "M100_Q0": 17960.83,
    "M0_Q100": 17960.83,
    "M100_Q100": 6282.31
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": -7.38,
    "M100_Q0": 57.59,
    "M0_Q100": 63.61,
    "M100_Q100": -14.42
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -56.38,
   "q_lift_M0": 0.133403,
   "q_revoice_st_M0": -62.4,
   "scale_db": {
    "M0_Q0": -24.108,
    "M100_Q0": -13.622,
    "M0_Q100": -14.416,
    "M100_Q100": -8.26
   }
  }
 ]
}
```
