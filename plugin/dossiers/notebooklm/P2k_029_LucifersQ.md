# LucifersQ — P2K reference 29 [REZ]

Architecture source: `P2k_029_LucifersQ.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**29 LucifersQ** [REZ] "Violent mid Q! Take care 40–90"
- IS: the bass vocabulary AGAIN (identical M0 to BolandBass) — the danger is
  entirely in the wiring: S6 starts as a REAL pole r=0.75 (invisible).
- MORPH: S1 +83 st and S2 −67 st — the set's most violent register swap,
  8 crossings.
- Q: S6 +0.25: the invisible real pole SNAPS into a complex mid resonance
  mid-wheel — that's the 40–90 danger zone: the ring being BORN while morph
  crossings are in flight.
- RECIPE: same vocabulary, adversarial wiring — max crossings + a Q-axis
  birth. Proof that intent lives in the JOURNEY, not the poses.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 79 → 9433 | 0.962 → 0.956 | 0.927 → 0.958 | 2316 → 1395 | 0.914 → 0.940 | -13.1/-3.2/-4.0/2.6 |
| S2 | 17683 → 357 | 0.990 → 0.969 | 0.999 → 0.989 | 2896 → 426 | 0.857 → 0.954 | -13.1/-3.2/-4.0/2.6 |
| S3 | 479 → 3162 | 0.976 → 0.760 | 0.996 → 0.974 | 13367 → 4170 | 0.884 → 0.848 | -13.1/-3.2/-4.0/2.6 |
| S4 | 13058 → 6629 | 0.975 → 0.857 | 0.987 → 0.970 | 15232 → 6458 | 0.801 → 0.871 | -13.1/-3.2/-4.0/2.6 |
| S5 | 15675 → 4275 | 0.964 → 0.829 | 0.994 → 0.960 | 17433 → 9469 | 0.914 → 0.950 | -13.1/-3.2/-4.0/2.6 |
| S6 | 0 → 1036 | 0.753 → 0.888 | 0.999 → 0.650 | 6730 → 6948 | 1.000 → 1.000 | -13.1/-3.2/-4.0/2.6 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 29,
 "name": "LucifersQ",
 "x3_type": "REZ",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_029_LucifersQ.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 79.43,
    "M100_Q0": 9432.73,
    "M0_Q100": 5092.69,
    "M100_Q100": 12781.35
   },
   "pole_r": {
    "M0_Q0": 0.962199,
    "M100_Q0": 0.95609,
    "M0_Q100": 0.927074,
    "M100_Q100": 0.958131
   },
   "zero_hz": {
    "M0_Q0": 2315.58,
    "M100_Q0": 1395.02,
    "M0_Q100": 414.66,
    "M100_Q100": 12366.78
   },
   "zero_r": {
    "M0_Q0": 0.914346,
    "M100_Q0": 0.939605,
    "M0_Q100": 0.960167,
    "M100_Q100": 0.931278
   },
   "carve_st": {
    "M0_Q0": 58.39,
    "M100_Q0": -33.09,
    "M0_Q100": -43.42,
    "M100_Q100": -0.57
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 82.7,
   "q_lift_M0": -0.035125,
   "q_revoice_st_M0": 72.03,
   "scale_db": {
    "M0_Q0": -13.066,
    "M100_Q0": -3.196,
    "M0_Q100": -4.033,
    "M100_Q100": 2.555
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 17683.33,
    "M100_Q0": 357.43,
    "M0_Q100": 1377.2,
    "M100_Q100": 1171.54
   },
   "pole_r": {
    "M0_Q0": 0.990192,
    "M100_Q0": 0.969266,
    "M0_Q100": 0.998596,
    "M100_Q100": 0.988712
   },
   "zero_hz": {
    "M0_Q0": 2895.74,
    "M100_Q0": 425.97,
    "M0_Q100": 1231.16,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.857064,
    "M100_Q0": 0.954045,
    "M0_Q100": 0.962199,
    "M100_Q100": 0.979415
   },
   "carve_st": {
    "M0_Q0": -31.32,
    "M100_Q0": 3.04,
    "M0_Q100": -1.94,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -67.54,
   "q_lift_M0": 0.008404,
   "q_revoice_st_M0": -44.19,
   "scale_db": {
    "M0_Q0": -13.066,
    "M100_Q0": -3.196,
    "M0_Q100": -4.033,
    "M100_Q100": 2.555
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 478.59,
    "M100_Q0": 3162.15,
    "M0_Q100": 2282.27,
    "M100_Q100": 10124.06
   },
   "pole_r": {
    "M0_Q0": 0.976293,
    "M100_Q0": 0.760466,
    "M0_Q100": 0.995844,
    "M100_Q100": 0.97429
   },
   "zero_hz": {
    "M0_Q0": 13366.63,
    "M100_Q0": 4169.59,
    "M0_Q100": 2206.66,
    "M100_Q100": 11098.99
   },
   "zero_r": {
    "M0_Q0": 0.883935,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.954045,
    "M100_Q100": 0.951996
   },
   "carve_st": {
    "M0_Q0": 57.64,
    "M100_Q0": 4.79,
    "M0_Q100": -0.58,
    "M100_Q100": 1.59
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 32.69,
   "q_lift_M0": 0.019551,
   "q_revoice_st_M0": 27.04,
   "scale_db": {
    "M0_Q0": -13.066,
    "M100_Q0": -3.196,
    "M0_Q100": -4.033,
    "M100_Q100": 2.555
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13057.83,
    "M100_Q0": 6628.67,
    "M0_Q100": 4139.42,
    "M100_Q100": 11369.51
   },
   "pole_r": {
    "M0_Q0": 0.975292,
    "M100_Q0": 0.857064,
    "M0_Q100": 0.986734,
    "M100_Q100": 0.970273
   },
   "zero_hz": {
    "M0_Q0": 15232.04,
    "M100_Q0": 6457.95,
    "M0_Q100": 3189.38,
    "M100_Q100": 12237.16
   },
   "zero_r": {
    "M0_Q0": 0.800505,
    "M100_Q0": 0.870577,
    "M0_Q100": 0.96625,
    "M100_Q100": 0.684831
   },
   "carve_st": {
    "M0_Q0": 2.67,
    "M100_Q0": -0.45,
    "M0_Q100": -4.51,
    "M100_Q100": 1.27
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -11.74,
   "q_lift_M0": 0.011442,
   "q_revoice_st_M0": -19.89,
   "scale_db": {
    "M0_Q0": -13.066,
    "M100_Q0": -3.196,
    "M0_Q100": -4.033,
    "M100_Q100": 2.555
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 15675.4,
    "M100_Q0": 4275.1,
    "M0_Q100": 3249.54,
    "M100_Q100": 8592.7
   },
   "pole_r": {
    "M0_Q0": 0.964227,
    "M100_Q0": 0.829267,
    "M0_Q100": 0.993881,
    "M100_Q100": 0.960167
   },
   "zero_hz": {
    "M0_Q0": 17432.67,
    "M100_Q0": 9468.97,
    "M0_Q100": 4243.96,
    "M100_Q100": 8720.93
   },
   "zero_r": {
    "M0_Q0": 0.914346,
    "M100_Q0": 0.949942,
    "M0_Q100": 0.97429,
    "M100_Q100": 0.750122
   },
   "carve_st": {
    "M0_Q0": 1.84,
    "M100_Q0": 13.77,
    "M0_Q100": 4.62,
    "M100_Q100": 0.26
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -22.49,
   "q_lift_M0": 0.029654,
   "q_revoice_st_M0": -27.24,
   "scale_db": {
    "M0_Q0": -13.066,
    "M100_Q0": -3.196,
    "M0_Q100": -4.033,
    "M100_Q100": 2.555
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 1036.17,
    "M0_Q100": 257.01,
    "M100_Q100": 14876.5
   },
   "pole_r": {
    "M0_Q0": 0.75269,
    "M100_Q0": 0.888343,
    "M0_Q100": 0.998963,
    "M100_Q100": 0.649707
   },
   "zero_hz": {
    "M0_Q0": 6729.94,
    "M100_Q0": 6947.94,
    "M0_Q100": 17960.83,
    "M100_Q100": 8787.93
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": null,
    "M100_Q0": 32.94,
    "M0_Q100": 73.52,
    "M100_Q100": -9.11
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": null,
   "q_lift_M0": 0.246273,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -13.066,
    "M100_Q0": -3.196,
    "M0_Q100": -4.033,
    "M100_Q100": 2.555
   }
  }
 ]
}
```
