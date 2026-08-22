# FuzziFace — P2K reference 7 [DST]

Architecture source: `P2k_007_FuzziFace.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**07 FuzziFace** [DST] "Nasty clipped distortion, Q = tone control"
- IS: five poles CROWDED 322–600 Hz at r≈1.0 (a razor mid-formant made of
  stacked unison poles) + all zeros parked on ONE spot (9644 Hz, r=0.02 —
  a broadband tilt platform, and scale −40..−52 dB feeding the AGC hard).
- MORPH: the crowd shifts up one octave together (+12..+13 st, parallel) —
  tone knob, not journey.
- Q: exactly zero lift — Q re-voices the crowd position. The DISTORTION is
  level war with the AGC, not filter shape.
- RECIPE: unison pole stack = the fuzz formant; morph moves the stack; keep
  Q dead. GRIT supplies the actual clipping honestly.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 4036 → 935 | 0.998 → 0.992 | 0.998 → 0.968 | 9644 → 9644 | 0.016 → 0.016 | -39.9/-31.5/-20.4/-9.5 |
| S2 | 548 → 1103 | 0.989 → 0.993 | 0.956 → 0.972 | 9644 → 9644 | 0.016 → 0.016 | -39.9/-31.5/-20.4/-9.5 |
| S3 | 600 → 1171 | 0.994 → 0.990 | 0.978 → 0.960 | 9644 → 9644 | 0.016 → 0.016 | -51.9/-43.6/-32.4/-21.6 |
| S4 | 376 → 803 | 0.997 → 0.992 | 0.990 → 0.970 | 9644 → 9644 | 0.016 → 0.016 | -39.9/-31.5/-20.4/-9.5 |
| S5 | 322 → 688 | 0.996 → 0.992 | 0.985 → 0.968 | 9644 → 9644 | 0.016 → 0.016 | -39.9/-31.5/-20.4/-9.5 |
| S6 | 580 → 1170 | 0.986 → 0.971 | 0.944 → 0.888 | 10935 → 4492 | 1.000 → 1.000 | -51.9/-43.6/-32.4/-21.6 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 7,
 "name": "FuzziFace",
 "x3_type": "DST",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_007_FuzziFace.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 4035.53,
    "M100_Q0": 934.77,
    "M0_Q100": 4035.53,
    "M100_Q100": 3754.68
   },
   "pole_r": {
    "M0_Q0": 0.99829,
    "M100_Q0": 0.99216,
    "M0_Q100": 0.99829,
    "M100_Q100": 0.968258
   },
   "zero_hz": {
    "M0_Q0": 9644.19,
    "M100_Q0": 9644.19,
    "M0_Q100": 9644.19,
    "M100_Q100": 9644.19
   },
   "zero_r": {
    "M0_Q0": 0.015625,
    "M100_Q0": 0.015625,
    "M0_Q100": 0.015625,
    "M100_Q100": 0.015625
   },
   "carve_st": {
    "M0_Q0": 15.08,
    "M100_Q0": 40.4,
    "M0_Q100": 15.08,
    "M100_Q100": 16.33
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -25.32,
   "q_lift_M0": 0.0,
   "q_revoice_st_M0": 0.0,
   "scale_db": {
    "M0_Q0": -39.903,
    "M100_Q0": -31.516,
    "M0_Q100": -20.354,
    "M100_Q100": -9.521
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 548.19,
    "M100_Q0": 1103.3,
    "M0_Q100": 2205.99,
    "M100_Q100": 4403.84
   },
   "pole_r": {
    "M0_Q0": 0.988712,
    "M100_Q0": 0.992898,
    "M0_Q100": 0.95609,
    "M100_Q100": 0.972284
   },
   "zero_hz": {
    "M0_Q0": 9644.19,
    "M100_Q0": 9644.19,
    "M0_Q100": 9644.19,
    "M100_Q100": 9644.19
   },
   "zero_r": {
    "M0_Q0": 0.015625,
    "M100_Q0": 0.015625,
    "M0_Q100": 0.015625,
    "M100_Q100": 0.015625
   },
   "carve_st": {
    "M0_Q0": 49.64,
    "M100_Q0": 37.53,
    "M0_Q100": 25.54,
    "M100_Q100": 13.57
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 12.11,
   "q_lift_M0": -0.032622,
   "q_revoice_st_M0": 24.1,
   "scale_db": {
    "M0_Q0": -39.903,
    "M100_Q0": -31.516,
    "M0_Q100": -20.354,
    "M100_Q100": -9.521
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 599.53,
    "M100_Q0": 1171.32,
    "M0_Q100": 2366.64,
    "M100_Q100": 4729.09
   },
   "pole_r": {
    "M0_Q0": 0.994372,
    "M100_Q0": 0.989699,
    "M0_Q100": 0.978291,
    "M100_Q100": 0.960167
   },
   "zero_hz": {
    "M0_Q0": 9644.19,
    "M100_Q0": 9644.19,
    "M0_Q100": 9644.19,
    "M100_Q100": 9644.19
   },
   "zero_r": {
    "M0_Q0": 0.015625,
    "M100_Q0": 0.015625,
    "M0_Q100": 0.015625,
    "M100_Q100": 0.015625
   },
   "carve_st": {
    "M0_Q0": 48.09,
    "M100_Q0": 36.5,
    "M0_Q100": 24.32,
    "M100_Q100": 12.34
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 11.59,
   "q_lift_M0": -0.016081,
   "q_revoice_st_M0": 23.77,
   "scale_db": {
    "M0_Q0": -51.944,
    "M100_Q0": -43.557,
    "M0_Q100": -32.395,
    "M100_Q100": -21.562
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 376.35,
    "M100_Q0": 802.98,
    "M0_Q100": 1514.78,
    "M100_Q100": 3184.29
   },
   "pole_r": {
    "M0_Q0": 0.997435,
    "M100_Q0": 0.992406,
    "M0_Q100": 0.989699,
    "M100_Q100": 0.970273
   },
   "zero_hz": {
    "M0_Q0": 9644.19,
    "M100_Q0": 9644.19,
    "M0_Q100": 9644.19,
    "M100_Q100": 9644.19
   },
   "zero_r": {
    "M0_Q0": 0.015625,
    "M100_Q0": 0.015625,
    "M0_Q100": 0.015625,
    "M100_Q100": 0.015625
   },
   "carve_st": {
    "M0_Q0": 56.15,
    "M100_Q0": 43.03,
    "M0_Q100": 32.05,
    "M100_Q100": 19.18
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 13.12,
   "q_lift_M0": -0.007736,
   "q_revoice_st_M0": 24.11,
   "scale_db": {
    "M0_Q0": -39.903,
    "M100_Q0": -31.516,
    "M0_Q100": -20.354,
    "M100_Q100": -9.521
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 321.88,
    "M100_Q0": 688.05,
    "M0_Q100": 1266.94,
    "M100_Q100": 2750.14
   },
   "pole_r": {
    "M0_Q0": 0.99621,
    "M100_Q0": 0.99216,
    "M0_Q100": 0.985248,
    "M100_Q100": 0.968258
   },
   "zero_hz": {
    "M0_Q0": 9644.19,
    "M100_Q0": 9644.19,
    "M0_Q100": 9644.19,
    "M100_Q100": 9644.19
   },
   "zero_r": {
    "M0_Q0": 0.015625,
    "M100_Q0": 0.015625,
    "M0_Q100": 0.015625,
    "M100_Q100": 0.015625
   },
   "carve_st": {
    "M0_Q0": 58.86,
    "M100_Q0": 45.71,
    "M0_Q100": 35.14,
    "M100_Q100": 21.72
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 13.15,
   "q_lift_M0": -0.010962,
   "q_revoice_st_M0": 23.72,
   "scale_db": {
    "M0_Q0": -39.903,
    "M100_Q0": -31.516,
    "M0_Q100": -20.354,
    "M100_Q100": -9.521
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 580.23,
    "M100_Q0": 1170.17,
    "M0_Q100": 2316.85,
    "M100_Q100": 4717.13
   },
   "pole_r": {
    "M0_Q0": 0.985744,
    "M100_Q0": 0.971279,
    "M0_Q100": 0.943754,
    "M100_Q100": 0.888343
   },
   "zero_hz": {
    "M0_Q0": 10935.17,
    "M100_Q0": 4492.38,
    "M0_Q100": 12363.18,
    "M100_Q100": 7990.82
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 50.83,
    "M100_Q0": 23.29,
    "M0_Q100": 28.99,
    "M100_Q100": 9.13
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": 12.14,
   "q_lift_M0": -0.04199,
   "q_revoice_st_M0": 23.97,
   "scale_db": {
    "M0_Q0": -51.944,
    "M100_Q0": -43.557,
    "M0_Q100": -32.395,
    "M100_Q100": -21.562
   }
  }
 ]
}
```
