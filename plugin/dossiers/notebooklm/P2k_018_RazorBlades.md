# RazorBlades — P2K reference 18 [EQ-]

Architecture source: `P2k_018_RazorBlades.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**18 RazorBlades** [EQ-] "Cuts frequency bands. Q selects different bands"
- IS: subtraction machine — zeros do the cutting (1620/6046/12312/13063 at
  M0), poles at r≈1.0 sit BESIDE the cuts to sharpen their edges.
- MORPH: blade set slides down (−20..−29 st) = which bands bleed.
- Q: NEGATIVE lift across the board (−0.02..−0.07) — the only all-negative
  Q in the set: more Q = deeper cuts (pole backs off, zero wins).
- RECIPE: author the zeros as the melody, poles as the edge-sharpeners; Q
  drives zero dominance. Character by absence.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 12648 → 3871 | 0.993 → 0.999 | 0.931 → 0.991 | 0 → 0 | 0.939 → 0.594 | -2.3/-10.4/-2.6/-11.1 |
| S2 | 443 → 83 | 0.980 → 0.982 | 0.906 → 0.993 | 1620 → 779 | 0.966 → 0.995 | -2.3/-10.4/-2.6/-11.1 |
| S3 | 1383 → 1135 | 0.987 → 0.998 | 0.931 → 0.980 | 6046 → 1845 | 0.923 → 0.938 | -2.3/-10.4/-2.6/-11.1 |
| S4 | 13518 → 3144 | 0.976 → 0.998 | 0.927 → 0.989 | 12312 → 3030 | 0.960 → 0.968 | -2.3/-10.4/-2.6/-11.1 |
| S5 | 6183 → 5445 | 0.995 → 0.999 | 0.935 → 0.990 | 13063 → 4581 | 0.910 → 0.960 | -2.3/-10.4/-2.6/-11.1 |
| S6 | 10096 → 2540 | 0.948 → 0.996 | 0.931 → 0.989 | 10151 → 17961 | 1.000 → 1.000 | -2.3/-10.4/-2.6/-11.1 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 18,
 "name": "RazorBlades",
 "x3_type": "EQ-",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_018_RazorBlades.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 12648.48,
    "M100_Q0": 3870.84,
    "M0_Q100": 14370.02,
    "M100_Q100": 6542.48
   },
   "pole_r": {
    "M0_Q0": 0.992898,
    "M100_Q0": 0.998718,
    "M0_Q100": 0.931278,
    "M100_Q100": 0.990685
   },
   "zero_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 0.0,
    "M0_Q100": 3882.89,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.939016,
    "M100_Q0": 0.593766,
    "M0_Q100": 0.770671,
    "M100_Q100": 0.593766
   },
   "carve_st": {
    "M0_Q0": null,
    "M100_Q0": null,
    "M0_Q100": -22.65,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -20.5,
   "q_lift_M0": -0.06162,
   "q_revoice_st_M0": 2.21,
   "scale_db": {
    "M0_Q0": -2.256,
    "M100_Q0": -10.447,
    "M0_Q100": -2.643,
    "M100_Q100": -11.056
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 443.19,
    "M100_Q0": 82.64,
    "M0_Q100": 761.4,
    "M100_Q100": 117.97
   },
   "pole_r": {
    "M0_Q0": 0.980286,
    "M100_Q0": 0.982276,
    "M0_Q100": 0.905762,
    "M100_Q100": 0.992652
   },
   "zero_hz": {
    "M0_Q0": 1619.79,
    "M100_Q0": 778.76,
    "M0_Q100": 14827.0,
    "M100_Q100": 379.16
   },
   "zero_r": {
    "M0_Q0": 0.96625,
    "M100_Q0": 0.995353,
    "M0_Q100": 0.994863,
    "M100_Q100": 0.914346
   },
   "carve_st": {
    "M0_Q0": 22.44,
    "M100_Q0": 38.84,
    "M0_Q100": 51.4,
    "M100_Q100": 20.21
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -29.08,
   "q_lift_M0": -0.074524,
   "q_revoice_st_M0": 9.37,
   "scale_db": {
    "M0_Q0": -2.256,
    "M100_Q0": -10.447,
    "M0_Q100": -2.643,
    "M100_Q100": -11.056
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 1383.01,
    "M100_Q0": 1135.19,
    "M0_Q100": 1603.87,
    "M100_Q100": 18159.83
   },
   "pole_r": {
    "M0_Q0": 0.986734,
    "M100_Q0": 0.997802,
    "M0_Q100": 0.931278,
    "M100_Q100": 0.980286
   },
   "zero_hz": {
    "M0_Q0": 6046.21,
    "M100_Q0": 1845.27,
    "M0_Q100": 934.6,
    "M100_Q100": 2875.04
   },
   "zero_r": {
    "M0_Q0": 0.922851,
    "M100_Q0": 0.937524,
    "M0_Q100": 0.984257,
    "M100_Q100": 0.707236
   },
   "carve_st": {
    "M0_Q0": 25.54,
    "M100_Q0": 8.41,
    "M0_Q100": -9.35,
    "M100_Q100": -31.91
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -3.42,
   "q_lift_M0": -0.055456,
   "q_revoice_st_M0": 2.56,
   "scale_db": {
    "M0_Q0": -2.256,
    "M100_Q0": -10.447,
    "M0_Q100": -2.643,
    "M100_Q100": -11.056
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13518.46,
    "M100_Q0": 3143.87,
    "M0_Q100": 16088.56,
    "M100_Q100": 1382.12
   },
   "pole_r": {
    "M0_Q0": 0.976293,
    "M100_Q0": 0.998351,
    "M0_Q100": 0.927074,
    "M100_Q100": 0.989205
   },
   "zero_hz": {
    "M0_Q0": 12311.55,
    "M100_Q0": 3029.91,
    "M0_Q100": 17407.68,
    "M100_Q100": 12367.8
   },
   "zero_r": {
    "M0_Q0": 0.960167,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.997312,
    "M100_Q100": 0.728995
   },
   "carve_st": {
    "M0_Q0": -1.62,
    "M100_Q0": -0.64,
    "M0_Q100": 1.36,
    "M100_Q100": 37.94
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -25.25,
   "q_lift_M0": -0.049219,
   "q_revoice_st_M0": 3.01,
   "scale_db": {
    "M0_Q0": -2.256,
    "M100_Q0": -10.447,
    "M0_Q100": -2.643,
    "M100_Q100": -11.056
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 6182.56,
    "M100_Q0": 5444.77,
    "M0_Q100": 11660.64,
    "M100_Q100": 8641.25
   },
   "pole_r": {
    "M0_Q0": 0.995353,
    "M100_Q0": 0.99884,
    "M0_Q100": 0.935439,
    "M100_Q100": 0.990192
   },
   "zero_hz": {
    "M0_Q0": 13063.29,
    "M100_Q0": 4581.35,
    "M0_Q100": 12690.87,
    "M100_Q100": 13211.2
   },
   "zero_r": {
    "M0_Q0": 0.910064,
    "M100_Q0": 0.960167,
    "M0_Q100": 0.988712,
    "M100_Q100": 0.897095
   },
   "carve_st": {
    "M0_Q0": 12.95,
    "M100_Q0": -2.99,
    "M0_Q100": 1.47,
    "M100_Q100": 7.35
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -2.2,
   "q_lift_M0": -0.059914,
   "q_revoice_st_M0": 10.98,
   "scale_db": {
    "M0_Q0": -2.256,
    "M100_Q0": -10.447,
    "M0_Q100": -2.643,
    "M100_Q100": -11.056
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 10096.29,
    "M100_Q0": 2539.91,
    "M0_Q100": 15994.72,
    "M100_Q100": 1879.07
   },
   "pole_r": {
    "M0_Q0": 0.947884,
    "M100_Q0": 0.996088,
    "M0_Q100": 0.931278,
    "M100_Q100": 0.989205
   },
   "zero_hz": {
    "M0_Q0": 10151.41,
    "M100_Q0": 17960.83,
    "M0_Q100": 12363.18,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 0.09,
    "M100_Q0": 33.86,
    "M0_Q100": -4.46,
    "M100_Q100": 39.08
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -23.89,
   "q_lift_M0": -0.016606,
   "q_revoice_st_M0": 7.97,
   "scale_db": {
    "M0_Q0": -2.256,
    "M100_Q0": -10.447,
    "M0_Q100": -2.643,
    "M100_Q100": -11.056
   }
  }
 ]
}
```
