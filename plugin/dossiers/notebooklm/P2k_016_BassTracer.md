# BassTracer — P2K reference 16 [EQ+]

Architecture source: `P2k_016_BassTracer.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**16 BassTracer** [EQ+] "Low Q boosts bass; Q to 115 = self-player"
- IS: the bass vocabulary again, wired so S6 sits mid (1570 @ r=0.64) at Q0.
- MORPH: frame tightens onto the bassline (mixed ±16..27 st).
- Q: S6 +0.36 — the SET'S BIGGEST single lift. The whole character is one
  section going from bystander (0.64) to oscillator-adjacent (1.0): the
  self-player.
- RECIPE: five sections do honest EQ; ONE section owns the entire Q axis
  with a huge arc. Single-bloomer architecture — cheap to author, huge feel.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 17683 → 9433 | 0.990 → 0.956 | 0.999 → 0.988 | 489 → 426 | 0.971 → 0.954 | -2.5/-5.3/-5.6/-0.8 |
| S2 | 530 → 1344 | 0.848 → 0.948 | 0.934 → 0.981 | 2316 → 1395 | 0.914 → 0.940 | -2.5/-5.3/-5.6/-0.8 |
| S3 | 479 → 2336 | 0.976 → 0.718 | 0.919 → 0.673 | 13367 → 4170 | 0.884 → 0.848 | -2.5/-5.3/-5.6/-0.8 |
| S4 | 15675 → 6367 | 0.964 → 0.857 | 0.999 → 0.996 | 15232 → 6458 | 0.801 → 0.871 | -2.5/-5.3/-5.6/-0.8 |
| S5 | 13058 → 4182 | 0.975 → 0.829 | 0.991 → 0.998 | 17433 → 9469 | 0.914 → 0.950 | -2.5/-5.3/-5.6/-0.8 |
| S6 | 1570 → 357 | 0.638 → 0.969 | 0.999 → 0.999 | 3336 → 17961 | 1.000 → 1.000 | -2.5/-5.3/-5.6/-0.8 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 16,
 "name": "BassTracer",
 "x3_type": "EQ+",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_016_BassTracer.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 17683.33,
    "M100_Q0": 9432.73,
    "M0_Q100": 9769.42,
    "M100_Q100": 12700.94
   },
   "pole_r": {
    "M0_Q0": 0.990192,
    "M100_Q0": 0.95609,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.987723
   },
   "zero_hz": {
    "M0_Q0": 489.09,
    "M100_Q0": 425.97,
    "M0_Q100": 10045.75,
    "M100_Q100": 12237.16
   },
   "zero_r": {
    "M0_Q0": 0.971279,
    "M100_Q0": 0.954045,
    "M0_Q100": 0.857064,
    "M100_Q100": 0.684831
   },
   "carve_st": {
    "M0_Q0": -62.11,
    "M100_Q0": -53.63,
    "M0_Q100": 0.48,
    "M100_Q100": -0.64
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -10.88,
   "q_lift_M0": 0.008831,
   "q_revoice_st_M0": -10.27,
   "scale_db": {
    "M0_Q0": -2.509,
    "M100_Q0": -5.31,
    "M0_Q100": -5.629,
    "M100_Q100": -0.775
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 529.67,
    "M100_Q0": 1344.14,
    "M0_Q100": 0.0,
    "M100_Q100": 8485.86
   },
   "pole_r": {
    "M0_Q0": 0.847899,
    "M100_Q0": 0.947884,
    "M0_Q100": 0.933622,
    "M100_Q100": 0.981282
   },
   "zero_hz": {
    "M0_Q0": 2315.58,
    "M100_Q0": 1395.02,
    "M0_Q100": 0.0,
    "M100_Q100": 8720.93
   },
   "zero_r": {
    "M0_Q0": 0.914346,
    "M100_Q0": 0.939605,
    "M0_Q100": 0.976568,
    "M100_Q100": 0.750122
   },
   "carve_st": {
    "M0_Q0": 25.54,
    "M100_Q0": 0.64,
    "M0_Q100": null,
    "M100_Q100": 0.47
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 16.12,
   "q_lift_M0": 0.085723,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -2.509,
    "M100_Q0": -5.31,
    "M0_Q100": -5.629,
    "M100_Q100": -0.775
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 478.59,
    "M100_Q0": 2335.68,
    "M0_Q100": 1395.45,
    "M100_Q100": 14445.57
   },
   "pole_r": {
    "M0_Q0": 0.976293,
    "M100_Q0": 0.718198,
    "M0_Q100": 0.918608,
    "M100_Q100": 0.673327
   },
   "zero_hz": {
    "M0_Q0": 13366.63,
    "M100_Q0": 4169.59,
    "M0_Q100": 1235.72,
    "M100_Q100": 12366.78
   },
   "zero_r": {
    "M0_Q0": 0.883935,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.973287,
    "M100_Q100": 0.931278
   },
   "carve_st": {
    "M0_Q0": 57.64,
    "M100_Q0": 10.03,
    "M0_Q100": -2.1,
    "M100_Q100": -2.69
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 27.44,
   "q_lift_M0": -0.057685,
   "q_revoice_st_M0": 18.53,
   "scale_db": {
    "M0_Q0": -2.509,
    "M100_Q0": -5.31,
    "M0_Q100": -5.629,
    "M100_Q100": -0.775
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 15675.4,
    "M100_Q0": 6366.59,
    "M0_Q100": 2028.55,
    "M100_Q100": 10571.09
   },
   "pole_r": {
    "M0_Q0": 0.964227,
    "M100_Q0": 0.857064,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.995844
   },
   "zero_hz": {
    "M0_Q0": 15232.04,
    "M100_Q0": 6457.95,
    "M0_Q100": 2006.74,
    "M100_Q100": 11098.99
   },
   "zero_r": {
    "M0_Q0": 0.800505,
    "M100_Q0": 0.870577,
    "M0_Q100": 0.978291,
    "M100_Q100": 0.951996
   },
   "carve_st": {
    "M0_Q0": -0.5,
    "M100_Q0": 0.25,
    "M0_Q100": -0.19,
    "M100_Q100": 0.84
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -15.6,
   "q_lift_M0": 0.034796,
   "q_revoice_st_M0": -35.4,
   "scale_db": {
    "M0_Q0": -2.509,
    "M100_Q0": -5.31,
    "M0_Q100": -5.629,
    "M100_Q100": -0.775
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 13057.83,
    "M100_Q0": 4181.97,
    "M0_Q100": 1137.73,
    "M100_Q100": 11553.17
   },
   "pole_r": {
    "M0_Q0": 0.975292,
    "M100_Q0": 0.829267,
    "M0_Q100": 0.991178,
    "M100_Q100": 0.997802
   },
   "zero_hz": {
    "M0_Q0": 17432.67,
    "M100_Q0": 9468.97,
    "M0_Q100": 562.95,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.914346,
    "M100_Q0": 0.949942,
    "M0_Q100": 0.971279,
    "M100_Q100": 0.979415
   },
   "carve_st": {
    "M0_Q0": 5.0,
    "M100_Q0": 14.15,
    "M0_Q100": -12.18,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -19.71,
   "q_lift_M0": 0.015886,
   "q_revoice_st_M0": -42.25,
   "scale_db": {
    "M0_Q0": -2.509,
    "M100_Q0": -5.31,
    "M0_Q100": -5.629,
    "M100_Q100": -0.775
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 1569.67,
    "M100_Q0": 357.43,
    "M0_Q100": 434.56,
    "M100_Q100": 382.57
   },
   "pole_r": {
    "M0_Q0": 0.637569,
    "M100_Q0": 0.969266,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.999023
   },
   "zero_hz": {
    "M0_Q0": 3335.86,
    "M100_Q0": 17960.83,
    "M0_Q100": 17960.83,
    "M100_Q100": 10151.41
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 13.05,
    "M100_Q0": 67.81,
    "M0_Q100": 64.43,
    "M100_Q100": 56.76
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -25.62,
   "q_lift_M0": 0.361454,
   "q_revoice_st_M0": -22.23,
   "scale_db": {
    "M0_Q0": -2.509,
    "M100_Q0": -5.31,
    "M0_Q100": -5.629,
    "M100_Q100": -0.775
   }
  }
 ]
}
```
