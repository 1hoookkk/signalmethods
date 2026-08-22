# MeatyGizmo — P2K reference 4 [REZ]

Architecture source: `P2k_004_MeatyGizmo.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**04 MeatyGizmo** [REZ] "Filter inverts at mid-Q"
- IS: Millennium's Q0 corners verbatim. The character IS the second axis:
  Q100 replaces the shape with its negative (corr ≈ 0 at M0).
- MORPH: identical journey to Millennium.
- Q: the wheel crossfades toward an inverted-geometry pose; mid-Q = the
  audible flip point. S2 −0.59 / S3 +0.24 — the inversion is a pole handoff.
- RECIPE: take ANY loved body, keep its Q0 edge, author a spectral-negative
  pose pair for Q100 (peaks→notches at the same Hz). One axis swap = new
  character. (E-MU proved the move; we own the op — COPY POSE + invert.)
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 59 → 5134 | 0.999 → 0.968 | 1.000 → 0.988 | 9642 → 6915 | 0.810 → 0.848 | -14.3/-4.8/-0.2/-4.3 |
| S2 | 10329 → 8217 | 0.944 → 0.952 | 0.354 → 0.988 | 10566 → 1544 | 0.940 → 0.958 | -14.3/-4.8/-0.2/-4.3 |
| S3 | 12893 → 871 | 0.760 → 0.986 | 1.000 → 0.992 | 13401 → 2585 | 0.986 → 0.944 | -14.3/-4.8/-0.2/-4.3 |
| S4 | 13615 → 3487 | 0.968 → 0.964 | 1.000 → 0.638 | 14391 → 4607 | 0.968 → 0.948 | -14.3/-4.8/-0.2/-4.3 |
| S5 | 14670 → 2364 | 0.968 → 0.981 | 1.000 → 0.897 | 16287 → 547 | 0.919 → 0.946 | -14.3/-4.8/-0.2/-4.3 |
| S6 | 16277 → 456 | 0.901 → 0.975 | 0.966 → 0.978 | 10935 → 11134 | 1.000 → 1.000 | -14.3/-4.8/-0.2/-4.3 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 4,
 "name": "MeatyGizmo",
 "x3_type": "REZ",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_004_MeatyGizmo.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 58.92,
    "M100_Q0": 5134.03,
    "M0_Q100": 13479.97,
    "M100_Q100": 5848.79
   },
   "pole_r": {
    "M0_Q0": 0.99884,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.999664,
    "M100_Q100": 0.988218
   },
   "zero_hz": {
    "M0_Q0": 9642.21,
    "M100_Q0": 6915.2,
    "M0_Q100": 1150.25,
    "M100_Q100": 11333.82
   },
   "zero_r": {
    "M0_Q0": 0.810206,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.838635,
    "M100_Q100": 0.780742
   },
   "carve_st": {
    "M0_Q0": 88.25,
    "M100_Q0": 5.16,
    "M0_Q100": -42.61,
    "M100_Q100": 11.45
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 77.34,
   "q_lift_M0": 0.000824,
   "q_revoice_st_M0": 94.05,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -0.186,
    "M100_Q100": -4.287
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 10329.14,
    "M100_Q0": 8216.52,
    "M0_Q100": 2377.3,
    "M100_Q100": 4520.78
   },
   "pole_r": {
    "M0_Q0": 0.943754,
    "M100_Q0": 0.951996,
    "M0_Q100": 0.353898,
    "M100_Q100": 0.987723
   },
   "zero_hz": {
    "M0_Q0": 10565.68,
    "M100_Q0": 1543.71,
    "M0_Q100": 10565.68,
    "M100_Q100": 1543.71
   },
   "zero_r": {
    "M0_Q0": 0.939605,
    "M100_Q0": 0.958131,
    "M0_Q100": 0.939605,
    "M100_Q100": 0.958131
   },
   "carve_st": {
    "M0_Q0": 0.39,
    "M100_Q0": -28.95,
    "M0_Q100": 25.82,
    "M100_Q100": -18.6
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -3.96,
   "q_lift_M0": -0.589856,
   "q_revoice_st_M0": -25.43,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -0.186,
    "M100_Q100": -4.287
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 12892.96,
    "M100_Q0": 871.0,
    "M0_Q100": 10349.19,
    "M100_Q100": 2482.98
   },
   "pole_r": {
    "M0_Q0": 0.760466,
    "M100_Q0": 0.985744,
    "M0_Q100": 0.999573,
    "M100_Q100": 0.99167
   },
   "zero_hz": {
    "M0_Q0": 13401.26,
    "M100_Q0": 2584.86,
    "M0_Q100": 13401.26,
    "M100_Q100": 2584.86
   },
   "zero_r": {
    "M0_Q0": 0.986239,
    "M100_Q0": 0.943754,
    "M0_Q100": 0.986239,
    "M100_Q100": 0.943754
   },
   "carve_st": {
    "M0_Q0": 0.67,
    "M100_Q0": 18.83,
    "M0_Q100": 4.47,
    "M100_Q100": 0.7
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -46.65,
   "q_lift_M0": 0.239107,
   "q_revoice_st_M0": -3.8,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -0.186,
    "M100_Q100": -4.287
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13615.49,
    "M100_Q0": 3486.67,
    "M0_Q100": 16821.67,
    "M100_Q100": 16163.02
   },
   "pole_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.964227,
    "M0_Q100": 0.999603,
    "M100_Q100": 0.637569
   },
   "zero_hz": {
    "M0_Q0": 14391.16,
    "M100_Q0": 4606.88,
    "M0_Q100": 14391.16,
    "M100_Q100": 4606.88
   },
   "zero_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.947884,
    "M0_Q100": 0.968258,
    "M100_Q100": 0.947884
   },
   "carve_st": {
    "M0_Q0": 0.96,
    "M100_Q0": 4.82,
    "M0_Q100": -2.7,
    "M100_Q100": -21.73
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -23.58,
   "q_lift_M0": 0.031345,
   "q_revoice_st_M0": 3.66,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -0.186,
    "M100_Q100": -4.287
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 14669.71,
    "M100_Q0": 2364.02,
    "M0_Q100": 13481.58,
    "M100_Q100": 9573.24
   },
   "pole_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.981282,
    "M0_Q100": 0.999527,
    "M100_Q100": 0.897095
   },
   "zero_hz": {
    "M0_Q0": 16286.92,
    "M100_Q0": 546.84,
    "M0_Q100": 16286.92,
    "M100_Q100": 6915.2
   },
   "zero_r": {
    "M0_Q0": 0.918608,
    "M100_Q0": 0.945821,
    "M0_Q100": 0.918608,
    "M100_Q100": 0.847899
   },
   "carve_st": {
    "M0_Q0": 1.81,
    "M100_Q0": -25.34,
    "M0_Q100": 3.27,
    "M100_Q100": -5.63
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -31.6,
   "q_lift_M0": 0.031269,
   "q_revoice_st_M0": -1.46,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -0.186,
    "M100_Q100": -4.287
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 16276.69,
    "M100_Q0": 456.06,
    "M0_Q100": 2632.27,
    "M100_Q100": 511.02
   },
   "pole_r": {
    "M0_Q0": 0.901439,
    "M100_Q0": 0.975292,
    "M0_Q100": 0.96625,
    "M100_Q100": 0.978291
   },
   "zero_hz": {
    "M0_Q0": 10935.17,
    "M100_Q0": 11133.58,
    "M0_Q100": 9374.54,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": -6.89,
    "M100_Q0": 55.31,
    "M0_Q100": 21.99,
    "M100_Q100": 61.62
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -61.89,
   "q_lift_M0": 0.064811,
   "q_revoice_st_M0": -31.54,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -0.186,
    "M100_Q100": -4.287
   }
  }
 ]
}
```
