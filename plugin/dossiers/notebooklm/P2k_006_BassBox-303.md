# BassBox-303 — P2K reference 6 [LPF]

Architecture source: `P2k_006_BassBox-303.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**06 BassBox-303** [LPF] "Pumped lows with TB-like squelchy Q"
- IS: sub engine (real pole + 79 Hz pole under zeros at 426–489 Hz — boost
  with a lid) + a harmonic ladder above (479 / 1356 / 4170 / 6444 Hz at M100 —
  near-harmonic spacing, the "tuned to the note" squelch).
- MORPH: the ladder converges toward the mids from both directions (+20 and
  −20 st meeting, ZERO crossings — strict order, the acid identity never
  scrambles).
- Q: S1 +0.24 only — squelch = SUB resonance, not mid scream.
- RECIPE: keep section order locked, space poles harmonically over the root,
  put the whole Q budget in the bottom section. KEY-track it (the one thing
  the ROM couldn't do).
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 0 → 2296 | 0.750 → 0.729 | 0.992 → 0.962 | 489 → 426 | 0.971 → 0.954 | -6.7/-7.6/-6.9/0.1 |
| S2 | 79 → 257 | 0.962 → 0.998 | 0.989 → 0.997 | 2316 → 1395 | 0.914 → 0.940 | -6.7/-7.6/-6.9/0.1 |
| S3 | 479 → 1356 | 0.976 → 0.971 | 0.952 → 0.975 | 13367 → 4170 | 0.884 → 0.848 | -6.7/-7.6/-6.9/0.1 |
| S4 | 13173 → 4170 | 0.944 → 0.848 | 0.880 → 0.977 | 15232 → 6458 | 0.801 → 0.871 | -6.7/-7.6/-6.9/0.1 |
| S5 | 15543 → 6444 | 0.935 → 0.875 | 0.962 → 0.966 | 17433 → 17262 | 0.914 → 0.760 | -6.7/-7.6/-6.9/0.1 |
| S6 | 17356 → 8140 | 0.966 → 0.968 | 0.975 → 0.662 | 3784 → 17313 | 1.000 → 1.000 | -6.7/-7.6/-6.9/0.1 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 6,
 "name": "BassBox-303",
 "x3_type": "LPF",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_006_BassBox-303.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 2296.43,
    "M0_Q100": 1202.86,
    "M100_Q100": 8582.36
   },
   "pole_r": {
    "M0_Q0": 0.75001,
    "M100_Q0": 0.728995,
    "M0_Q100": 0.99216,
    "M100_Q100": 0.962199
   },
   "zero_hz": {
    "M0_Q0": 489.09,
    "M100_Q0": 425.97,
    "M0_Q100": 0.0,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.971279,
    "M100_Q0": 0.954045,
    "M0_Q100": 0.976568,
    "M100_Q100": 0.979415
   },
   "carve_st": {
    "M0_Q0": null,
    "M100_Q0": -29.17,
    "M0_Q100": null,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": null,
   "q_lift_M0": 0.24215,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -6.673,
    "M100_Q0": -7.648,
    "M0_Q100": -6.941,
    "M100_Q100": 0.147
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 79.43,
    "M100_Q0": 256.82,
    "M0_Q100": 158.84,
    "M100_Q100": 363.52
   },
   "pole_r": {
    "M0_Q0": 0.962199,
    "M100_Q0": 0.997557,
    "M0_Q100": 0.989205,
    "M100_Q100": 0.997067
   },
   "zero_hz": {
    "M0_Q0": 2315.58,
    "M100_Q0": 1395.02,
    "M0_Q100": 562.95,
    "M100_Q100": 8720.93
   },
   "zero_r": {
    "M0_Q0": 0.914346,
    "M100_Q0": 0.939605,
    "M0_Q100": 0.971279,
    "M100_Q100": 0.750122
   },
   "carve_st": {
    "M0_Q0": 58.39,
    "M100_Q0": 29.3,
    "M0_Q100": 21.91,
    "M100_Q100": 55.01
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 20.32,
   "q_lift_M0": 0.027006,
   "q_revoice_st_M0": 12.0,
   "scale_db": {
    "M0_Q0": -6.673,
    "M100_Q0": -7.648,
    "M0_Q100": -6.941,
    "M100_Q100": 0.147
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 478.59,
    "M100_Q0": 1356.05,
    "M0_Q100": 1156.16,
    "M100_Q100": 10718.61
   },
   "pole_r": {
    "M0_Q0": 0.976293,
    "M100_Q0": 0.971279,
    "M0_Q100": 0.951996,
    "M100_Q100": 0.975292
   },
   "zero_hz": {
    "M0_Q0": 13366.63,
    "M100_Q0": 4169.59,
    "M0_Q100": 1235.72,
    "M100_Q100": 11098.99
   },
   "zero_r": {
    "M0_Q0": 0.883935,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.973287,
    "M100_Q100": 0.951996
   },
   "carve_st": {
    "M0_Q0": 57.64,
    "M100_Q0": 19.45,
    "M0_Q100": 1.15,
    "M100_Q100": 0.6
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 18.03,
   "q_lift_M0": -0.024297,
   "q_revoice_st_M0": 15.27,
   "scale_db": {
    "M0_Q0": -6.673,
    "M100_Q0": -7.648,
    "M0_Q100": -6.941,
    "M100_Q100": 0.147
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13172.52,
    "M100_Q0": 4169.59,
    "M0_Q100": 1394.29,
    "M100_Q100": 11518.84
   },
   "pole_r": {
    "M0_Q0": 0.943754,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.879505,
    "M100_Q100": 0.977293
   },
   "zero_hz": {
    "M0_Q0": 15232.04,
    "M100_Q0": 6457.95,
    "M0_Q100": 2006.74,
    "M100_Q100": 12237.16
   },
   "zero_r": {
    "M0_Q0": 0.800505,
    "M100_Q0": 0.870577,
    "M0_Q100": 0.978291,
    "M100_Q100": 0.684831
   },
   "carve_st": {
    "M0_Q0": 2.51,
    "M100_Q0": 7.57,
    "M0_Q100": 6.3,
    "M100_Q100": 1.05
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -19.91,
   "q_lift_M0": -0.064249,
   "q_revoice_st_M0": -38.88,
   "scale_db": {
    "M0_Q0": -6.673,
    "M100_Q0": -7.648,
    "M0_Q100": -6.941,
    "M100_Q100": 0.147
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 15543.23,
    "M100_Q0": 6444.46,
    "M0_Q100": 2053.18,
    "M100_Q100": 12697.29
   },
   "pole_r": {
    "M0_Q0": 0.935439,
    "M100_Q0": 0.875052,
    "M0_Q100": 0.962199,
    "M100_Q100": 0.96625
   },
   "zero_hz": {
    "M0_Q0": 17432.67,
    "M100_Q0": 17261.79,
    "M0_Q100": 9724.37,
    "M100_Q100": 12366.78
   },
   "zero_r": {
    "M0_Q0": 0.914346,
    "M100_Q0": 0.760466,
    "M0_Q100": 0.941682,
    "M100_Q100": 0.931278
   },
   "carve_st": {
    "M0_Q0": 1.99,
    "M100_Q0": 17.06,
    "M0_Q100": 26.92,
    "M100_Q100": -0.46
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -15.24,
   "q_lift_M0": 0.02676,
   "q_revoice_st_M0": -35.04,
   "scale_db": {
    "M0_Q0": -6.673,
    "M100_Q0": -7.648,
    "M0_Q100": -6.941,
    "M100_Q100": 0.147
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 17355.73,
    "M100_Q0": 8139.52,
    "M0_Q100": 9918.86,
    "M100_Q100": 14652.45
   },
   "pole_r": {
    "M0_Q0": 0.96625,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.975292,
    "M100_Q100": 0.661622
   },
   "zero_hz": {
    "M0_Q0": 3784.15,
    "M100_Q0": 17312.96,
    "M0_Q100": 6396.33,
    "M100_Q100": 6396.33
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": -26.37,
    "M100_Q0": 13.07,
    "M0_Q100": -7.6,
    "M100_Q100": -14.35
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -13.11,
   "q_lift_M0": 0.009042,
   "q_revoice_st_M0": -9.69,
   "scale_db": {
    "M0_Q0": -6.673,
    "M100_Q0": -7.648,
    "M0_Q100": -6.941,
    "M100_Q100": 0.147
   }
  }
 ]
}
```
