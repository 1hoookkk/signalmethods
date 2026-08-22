# DJAlkaline — P2K reference 15 [EQ+]

Architecture source: `P2k_015_DJAlkaline.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**15 DJAlkaline** [EQ+] "Band accentuator, Q shifts ring frequency"
- IS: six r=0.99 poles spread 1.8–13.6 k against high-parked zeros — an
  isolator bank where every band is pre-armed to ring.
- MORPH: bands shift gently (+4..+13 st), one dives −22: band selector.
- Q: NO radius lift at all (+0.008 flat) — because Q instead MOVES the
  poles (Q100 rows sit at different Hz): "Q shifts ring frequency" is
  literal — the second axis is a TUNER, not an intensifier.
- RECIPE: park all radii hot at Q0; author the Q100 pose at shifted Hz.
  Q-as-pitch is a whole species the taxonomy hides (see also DreamWeava).
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 11617 → 14961 | 0.990 → 0.968 | 0.999 → 0.950 | 0 → 0 | 0.747 → 0.747 | -2.0/-1.9/-2.7/-4.1 |
| S2 | 1794 → 503 | 0.990 → 0.979 | 0.999 → 0.984 | 4162 → 4162 | 0.857 → 0.857 | -2.0/-1.9/-2.7/-4.1 |
| S3 | 3157 → 6622 | 0.990 → 0.968 | 0.999 → 0.976 | 7042 → 7042 | 0.848 → 0.848 | -2.0/-1.9/-2.7/-4.1 |
| S4 | 13587 → 17805 | 0.991 → 0.971 | 0.999 → 0.942 | 14901 → 14901 | 0.839 → 0.839 | -2.0/-1.9/-2.7/-4.1 |
| S5 | 5471 → 9757 | 0.990 → 0.969 | 0.999 → 0.976 | 16848 → 16848 | 0.791 → 0.791 | -2.0/-1.9/-2.7/-4.1 |
| S6 | 8636 → 12444 | 0.991 → 0.969 | 0.999 → 0.958 | 6396 → 10151 | 1.000 → 1.000 | -2.0/-1.9/-2.7/-4.1 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 15,
 "name": "DJAlkaline",
 "x3_type": "EQ+",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_015_DJAlkaline.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 11616.92,
    "M100_Q0": 14961.45,
    "M0_Q100": 11746.73,
    "M100_Q100": 13591.0
   },
   "pole_r": {
    "M0_Q0": 0.990192,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.949942
   },
   "zero_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 0.0,
    "M0_Q100": 0.0,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.746984,
    "M100_Q0": 0.746984,
    "M0_Q100": 0.746984,
    "M100_Q100": 0.746984
   },
   "carve_st": {
    "M0_Q0": null,
    "M100_Q0": null,
    "M0_Q100": null,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 4.38,
   "q_lift_M0": 0.008831,
   "q_revoice_st_M0": 0.19,
   "scale_db": {
    "M0_Q0": -2.018,
    "M100_Q0": -1.861,
    "M0_Q100": -2.676,
    "M100_Q100": -4.123
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 1794.42,
    "M100_Q0": 502.89,
    "M0_Q100": 1787.12,
    "M100_Q100": 426.58
   },
   "pole_r": {
    "M0_Q0": 0.989699,
    "M100_Q0": 0.979289,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.984257
   },
   "zero_hz": {
    "M0_Q0": 4162.19,
    "M100_Q0": 4162.19,
    "M0_Q100": 4162.19,
    "M100_Q100": 4162.19
   },
   "zero_r": {
    "M0_Q0": 0.857064,
    "M100_Q0": 0.857064,
    "M0_Q100": 0.857064,
    "M100_Q100": 0.857064
   },
   "carve_st": {
    "M0_Q0": 14.57,
    "M100_Q0": 36.59,
    "M0_Q100": 14.64,
    "M100_Q100": 39.44
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -22.02,
   "q_lift_M0": 0.009324,
   "q_revoice_st_M0": -0.07,
   "scale_db": {
    "M0_Q0": -2.018,
    "M100_Q0": -1.861,
    "M0_Q100": -2.676,
    "M100_Q100": -4.123
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 3157.21,
    "M100_Q0": 6622.42,
    "M0_Q100": 3142.8,
    "M100_Q100": 6246.17
   },
   "pole_r": {
    "M0_Q0": 0.989699,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.976293
   },
   "zero_hz": {
    "M0_Q0": 7042.33,
    "M100_Q0": 7042.33,
    "M0_Q100": 7042.33,
    "M100_Q100": 8265.67
   },
   "zero_r": {
    "M0_Q0": 0.847899,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.847899,
    "M100_Q100": 0.941682
   },
   "carve_st": {
    "M0_Q0": 13.89,
    "M100_Q0": 1.06,
    "M0_Q100": 13.97,
    "M100_Q100": 4.85
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 12.82,
   "q_lift_M0": 0.009324,
   "q_revoice_st_M0": -0.08,
   "scale_db": {
    "M0_Q0": -2.018,
    "M100_Q0": -1.861,
    "M0_Q100": -2.676,
    "M100_Q100": -4.123
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13586.76,
    "M100_Q0": 17804.86,
    "M0_Q100": 13487.51,
    "M100_Q100": 15424.9
   },
   "pole_r": {
    "M0_Q0": 0.990685,
    "M100_Q0": 0.971279,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.941682
   },
   "zero_hz": {
    "M0_Q0": 14901.1,
    "M100_Q0": 14901.1,
    "M0_Q100": 14901.1,
    "M100_Q100": 12014.26
   },
   "zero_r": {
    "M0_Q0": 0.838635,
    "M100_Q0": 0.838635,
    "M0_Q100": 0.838635,
    "M100_Q100": 0.870577
   },
   "carve_st": {
    "M0_Q0": 1.6,
    "M100_Q0": -3.08,
    "M0_Q100": 1.73,
    "M100_Q100": -4.33
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 4.68,
   "q_lift_M0": 0.008338,
   "q_revoice_st_M0": -0.13,
   "scale_db": {
    "M0_Q0": -2.018,
    "M100_Q0": -1.861,
    "M0_Q100": -2.676,
    "M100_Q100": -4.123
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 5471.18,
    "M100_Q0": 9756.94,
    "M0_Q100": 5444.23,
    "M100_Q100": 9713.47
   },
   "pole_r": {
    "M0_Q0": 0.989699,
    "M100_Q0": 0.969266,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.976293
   },
   "zero_hz": {
    "M0_Q0": 16847.86,
    "M100_Q0": 16847.86,
    "M0_Q100": 16847.86,
    "M100_Q100": 16847.86
   },
   "zero_r": {
    "M0_Q0": 0.790685,
    "M100_Q0": 0.790685,
    "M0_Q100": 0.790685,
    "M100_Q100": 0.790685
   },
   "carve_st": {
    "M0_Q0": 19.47,
    "M100_Q0": 9.46,
    "M0_Q100": 19.56,
    "M100_Q100": 9.53
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 10.01,
   "q_lift_M0": 0.009324,
   "q_revoice_st_M0": -0.09,
   "scale_db": {
    "M0_Q0": -2.018,
    "M100_Q0": -1.861,
    "M0_Q100": -2.676,
    "M100_Q100": -4.123
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 8636.16,
    "M100_Q0": 12443.98,
    "M0_Q100": 8793.11,
    "M100_Q100": 11469.08
   },
   "pole_r": {
    "M0_Q0": 0.991178,
    "M100_Q0": 0.969266,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.958131
   },
   "zero_hz": {
    "M0_Q0": 6396.33,
    "M100_Q0": 10151.41,
    "M0_Q100": 8590.69,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": -5.2,
    "M100_Q0": -3.53,
    "M0_Q100": -0.4,
    "M100_Q100": 7.77
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": 6.32,
   "q_lift_M0": 0.007845,
   "q_revoice_st_M0": 0.31,
   "scale_db": {
    "M0_Q0": -2.018,
    "M100_Q0": -1.861,
    "M0_Q100": -2.676,
    "M100_Q100": -4.123
   }
  }
 ]
}
```
