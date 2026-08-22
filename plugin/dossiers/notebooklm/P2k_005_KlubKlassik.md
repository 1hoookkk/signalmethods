# KlubKlassik — P2K reference 5 [LPF]

Architecture source: `P2k_005_KlubKlassik.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**05 KlubKlassik** [LPF] "Responsive sweep, wide spectrum of Q sounds"
- IS: the friendly one — every pole r=0.89–0.95 (no danger), formant-spaced
  cluster 200 Hz–4.6 k at M0.
- MORPH: everything UP (+6..+50 st, only 2 crossings) — an opening mouth, no
  drama, maximum playability.
- Q: mixed signs (S2 −0.34, others +0.07) — "wide spectrum" = each Q position
  re-voices rather than intensifies.
- RECIPE: moderate radii, parallel upward travel, Q as re-voicer. This is the
  workhorse-character hybrid: safe at every wheel position.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 4616 → 10686 | 0.901 → 0.923 | 0.972 → 0.976 | 1252 → 0 | 0.897 → 0.797 | -5.1/-2.7/-9.9/-3.6 |
| S2 | 201 → 3611 | 0.944 → 0.942 | 0.600 → 0.935 | 2829 → 3431 | 0.944 → 0.999 | -5.1/-2.7/-9.9/-3.6 |
| S3 | 882 → 3978 | 0.931 → 0.938 | 0.997 → 0.984 | 2014 → 4145 | 0.960 → 0.940 | -5.1/-2.7/-9.9/-3.6 |
| S4 | 9396 → 13220 | 0.927 → 0.940 | 0.996 → 0.962 | 2971 → 7360 | 0.972 → 0.829 | -5.1/-2.7/-9.9/-3.6 |
| S5 | 1604 → 1041 | 0.931 → 0.927 | 0.997 → 0.972 | 7922 → 10781 | 0.750 → 0.718 | -5.1/-2.7/-9.9/-3.6 |
| S6 | 2315 → 5869 | 0.950 → 0.893 | 0.866 → 0.966 | 2065 → 10738 | 1.000 → 1.000 | -5.1/-2.7/-9.9/-3.6 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 5,
 "name": "KlubKlassik",
 "x3_type": "LPF",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_005_KlubKlassik.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 4615.73,
    "M100_Q0": 10686.43,
    "M0_Q100": 4555.27,
    "M100_Q100": 13046.9
   },
   "pole_r": {
    "M0_Q0": 0.901439,
    "M100_Q0": 0.922851,
    "M0_Q100": 0.972284,
    "M100_Q100": 0.976293
   },
   "zero_hz": {
    "M0_Q0": 1252.06,
    "M100_Q0": 0.0,
    "M0_Q100": 2971.28,
    "M100_Q100": 7359.79
   },
   "zero_r": {
    "M0_Q0": 0.897095,
    "M100_Q0": 0.796904,
    "M0_Q100": 0.972284,
    "M100_Q100": 0.829267
   },
   "carve_st": {
    "M0_Q0": -22.59,
    "M100_Q0": null,
    "M0_Q100": -7.4,
    "M100_Q100": -9.91
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 14.53,
   "q_lift_M0": 0.070845,
   "q_revoice_st_M0": -0.23,
   "scale_db": {
    "M0_Q0": -5.125,
    "M100_Q0": -2.661,
    "M0_Q100": -9.882,
    "M100_Q100": -3.57
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 200.74,
    "M100_Q0": 3611.37,
    "M0_Q100": 1504.9,
    "M100_Q100": 5752.99
   },
   "pole_r": {
    "M0_Q0": 0.943754,
    "M100_Q0": 0.941682,
    "M0_Q100": 0.599683,
    "M100_Q100": 0.935439
   },
   "zero_hz": {
    "M0_Q0": 2828.91,
    "M100_Q0": 3431.02,
    "M0_Q100": 1252.06,
    "M100_Q100": 10780.58
   },
   "zero_r": {
    "M0_Q0": 0.943754,
    "M100_Q0": 0.999237,
    "M0_Q100": 0.897095,
    "M100_Q100": 0.718198
   },
   "carve_st": {
    "M0_Q0": 45.8,
    "M100_Q0": -0.89,
    "M0_Q100": -3.18,
    "M100_Q100": 10.87
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 50.03,
   "q_lift_M0": -0.344071,
   "q_revoice_st_M0": 34.88,
   "scale_db": {
    "M0_Q0": -5.125,
    "M100_Q0": -2.661,
    "M0_Q100": -9.882,
    "M100_Q100": -3.57
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 881.79,
    "M100_Q0": 3977.95,
    "M0_Q100": 1560.58,
    "M100_Q100": 3456.24
   },
   "pole_r": {
    "M0_Q0": 0.931278,
    "M100_Q0": 0.937524,
    "M0_Q100": 0.996578,
    "M100_Q100": 0.984257
   },
   "zero_hz": {
    "M0_Q0": 2014.45,
    "M100_Q0": 4144.96,
    "M0_Q100": 2014.45,
    "M100_Q100": 4144.96
   },
   "zero_r": {
    "M0_Q0": 0.960167,
    "M100_Q0": 0.939605,
    "M0_Q100": 0.960167,
    "M100_Q100": 0.939605
   },
   "carve_st": {
    "M0_Q0": 14.3,
    "M100_Q0": 0.71,
    "M0_Q100": 4.42,
    "M100_Q100": 3.15
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 26.08,
   "q_lift_M0": 0.0653,
   "q_revoice_st_M0": 9.88,
   "scale_db": {
    "M0_Q0": -5.125,
    "M100_Q0": -2.661,
    "M0_Q100": -9.882,
    "M100_Q100": -3.57
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 9395.91,
    "M100_Q0": 13219.67,
    "M0_Q100": 2348.73,
    "M100_Q100": 10204.37
   },
   "pole_r": {
    "M0_Q0": 0.927074,
    "M100_Q0": 0.939605,
    "M0_Q100": 0.996333,
    "M100_Q100": 0.962199
   },
   "zero_hz": {
    "M0_Q0": 2971.28,
    "M100_Q0": 7359.79,
    "M0_Q100": 2828.91,
    "M100_Q100": 3431.02
   },
   "zero_r": {
    "M0_Q0": 0.972284,
    "M100_Q0": 0.829267,
    "M0_Q100": 0.943754,
    "M100_Q100": 0.999237
   },
   "carve_st": {
    "M0_Q0": -19.93,
    "M100_Q0": -10.14,
    "M0_Q100": 3.22,
    "M100_Q100": -18.87
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 5.91,
   "q_lift_M0": 0.069259,
   "q_revoice_st_M0": -24.0,
   "scale_db": {
    "M0_Q0": -5.125,
    "M100_Q0": -2.661,
    "M0_Q100": -9.882,
    "M100_Q100": -3.57
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 1603.87,
    "M100_Q0": 1040.76,
    "M0_Q100": 870.54,
    "M100_Q100": 3658.97
   },
   "pole_r": {
    "M0_Q0": 0.931278,
    "M100_Q0": 0.927074,
    "M0_Q100": 0.996578,
    "M100_Q100": 0.972284
   },
   "zero_hz": {
    "M0_Q0": 7921.93,
    "M100_Q0": 10780.58,
    "M0_Q100": 7921.93,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.750122,
    "M100_Q0": 0.718198,
    "M0_Q100": 0.750122,
    "M100_Q100": 0.796904
   },
   "carve_st": {
    "M0_Q0": 27.65,
    "M100_Q0": 40.47,
    "M0_Q100": 38.23,
    "M100_Q100": null
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -7.49,
   "q_lift_M0": 0.0653,
   "q_revoice_st_M0": -10.58,
   "scale_db": {
    "M0_Q0": -5.125,
    "M100_Q0": -2.661,
    "M0_Q100": -9.882,
    "M100_Q100": -3.57
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 2315.13,
    "M100_Q0": 5868.56,
    "M0_Q100": 925.34,
    "M100_Q100": 1024.76
   },
   "pole_r": {
    "M0_Q0": 0.949942,
    "M100_Q0": 0.89273,
    "M0_Q100": 0.866078,
    "M100_Q100": 0.96625
   },
   "zero_hz": {
    "M0_Q0": 2065.13,
    "M100_Q0": 10737.97,
    "M0_Q100": 11333.42,
    "M100_Q100": 13476.05
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": -1.98,
    "M100_Q0": 10.46,
    "M0_Q100": 43.37,
    "M100_Q100": 44.6
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": 16.1,
   "q_lift_M0": -0.083864,
   "q_revoice_st_M0": -15.88,
   "scale_db": {
    "M0_Q0": -5.125,
    "M100_Q0": -2.661,
    "M0_Q100": -9.882,
    "M100_Q100": -3.57
   }
  }
 ]
}
```
