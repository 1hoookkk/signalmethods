# Millennium — P2K reference 3 [LPF]

Architecture source: `P2k_003_Millennium.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**03 Millennium** [LPF] "Aggressive LPF, spiky tonal peaks"
- IS: M0 is the scoop skeleton — S1 sub pole at 59 Hz r=1.00 under −14 dB
  scale, everything else parked 10–16 k as a dark ceiling frame; mids EMPTY
  (−45 dB). M100 grows the body: poles respread 456–8.2 k at r=0.95–0.99.
- MORPH: birth of a midrange — poles migrate down en masse (−24..−62 st,
  12 crossing pairs, the set's most crossing-heavy journey with MeatyGizmo).
- Q: S3 +0.21 but S6 −0.62 — the spiky peaks come WITH a collapse elsewhere.
- RECIPE: author the empty club skeleton first (sub + air only), then the
  full-body pose; let sections cross freely mid-morph — the crossings ARE the
  aggression. Q trades a ceiling for a spike.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 59 → 5134 | 0.999 → 0.968 | 0.966 → 0.446 | 9642 → 6915 | 0.810 → 0.848 | -14.3/-4.8/-1.8/-3.9 |
| S2 | 10329 → 8217 | 0.944 → 0.952 | 0.966 → 0.983 | 10566 → 1544 | 0.940 → 0.958 | -14.3/-4.8/-1.8/-3.9 |
| S3 | 12893 → 871 | 0.760 → 0.986 | 0.966 → 0.996 | 13401 → 2585 | 0.986 → 0.944 | -14.3/-4.8/-1.8/-3.9 |
| S4 | 13615 → 3487 | 0.968 → 0.964 | 0.969 → 0.914 | 14391 → 4607 | 0.968 → 0.948 | -14.3/-4.8/-1.8/-3.9 |
| S5 | 14670 → 2364 | 0.968 → 0.981 | 0.906 → 0.935 | 16287 → 547 | 0.919 → 0.946 | -14.3/-4.8/-1.8/-3.9 |
| S6 | 16277 → 456 | 0.901 → 0.975 | 0.280 → 0.999 | 10935 → 11134 | 1.000 → 1.000 | -14.3/-4.8/-1.8/-3.9 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 3,
 "name": "Millennium",
 "x3_type": "LPF",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_003_Millennium.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 58.92,
    "M100_Q0": 5134.03,
    "M0_Q100": 14700.52,
    "M100_Q100": 19531.25
   },
   "pole_r": {
    "M0_Q0": 0.99884,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.96625,
    "M100_Q100": 0.445669
   },
   "zero_hz": {
    "M0_Q0": 9642.21,
    "M100_Q0": 6915.2,
    "M0_Q100": 10565.68,
    "M100_Q100": 13808.05
   },
   "zero_r": {
    "M0_Q0": 0.810206,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.939605,
    "M100_Q100": 0.760466
   },
   "carve_st": {
    "M0_Q0": 88.25,
    "M100_Q0": 5.16,
    "M0_Q100": -5.72,
    "M100_Q100": -6.0
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 77.34,
   "q_lift_M0": -0.03259,
   "q_revoice_st_M0": 95.55,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -1.753,
    "M100_Q100": -3.888
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 10329.14,
    "M100_Q0": 8216.52,
    "M0_Q100": 10379.1,
    "M100_Q100": 7443.55
   },
   "pole_r": {
    "M0_Q0": 0.943754,
    "M100_Q0": 0.951996,
    "M0_Q100": 0.96625,
    "M100_Q100": 0.98327
   },
   "zero_hz": {
    "M0_Q0": 10565.68,
    "M100_Q0": 1543.71,
    "M0_Q100": 12683.74,
    "M100_Q100": 6915.2
   },
   "zero_r": {
    "M0_Q0": 0.939605,
    "M100_Q0": 0.958131,
    "M0_Q100": 0.922851,
    "M100_Q100": 0.847899
   },
   "carve_st": {
    "M0_Q0": 0.39,
    "M100_Q0": -28.95,
    "M0_Q100": 3.47,
    "M100_Q100": -1.27
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -3.96,
   "q_lift_M0": 0.022496,
   "q_revoice_st_M0": 0.08,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -1.753,
    "M100_Q100": -3.888
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 12892.96,
    "M100_Q0": 871.0,
    "M0_Q100": 12925.17,
    "M100_Q100": 4427.41
   },
   "pole_r": {
    "M0_Q0": 0.760466,
    "M100_Q0": 0.985744,
    "M0_Q100": 0.96625,
    "M100_Q100": 0.996088
   },
   "zero_hz": {
    "M0_Q0": 13401.26,
    "M100_Q0": 2584.86,
    "M0_Q100": 13401.26,
    "M100_Q100": 4606.88
   },
   "zero_r": {
    "M0_Q0": 0.986239,
    "M100_Q0": 0.943754,
    "M0_Q100": 0.986239,
    "M100_Q100": 0.947884
   },
   "carve_st": {
    "M0_Q0": 0.67,
    "M100_Q0": 18.83,
    "M0_Q100": 0.63,
    "M100_Q100": 0.69
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -46.65,
   "q_lift_M0": 0.205784,
   "q_revoice_st_M0": 0.04,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -1.753,
    "M100_Q100": -3.888
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13615.49,
    "M100_Q0": 3486.67,
    "M0_Q100": 13360.7,
    "M100_Q100": 9257.62
   },
   "pole_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.964227,
    "M0_Q100": 0.969266,
    "M100_Q100": 0.914346
   },
   "zero_hz": {
    "M0_Q0": 14391.16,
    "M100_Q0": 4606.88,
    "M0_Q100": 14391.16,
    "M100_Q100": 2584.86
   },
   "zero_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.947884,
    "M0_Q100": 0.968258,
    "M100_Q100": 0.943754
   },
   "carve_st": {
    "M0_Q0": 0.96,
    "M100_Q0": 4.82,
    "M0_Q100": 1.29,
    "M100_Q100": -22.09
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -23.58,
   "q_lift_M0": 0.001008,
   "q_revoice_st_M0": -0.33,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -1.753,
    "M100_Q100": -3.888
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 14669.71,
    "M100_Q0": 2364.02,
    "M0_Q100": 1400.71,
    "M100_Q100": 316.3
   },
   "pole_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.981282,
    "M0_Q100": 0.905762,
    "M100_Q100": 0.935439
   },
   "zero_hz": {
    "M0_Q0": 16286.92,
    "M100_Q0": 546.84,
    "M0_Q100": 1736.57,
    "M100_Q100": 1543.71
   },
   "zero_r": {
    "M0_Q0": 0.918608,
    "M100_Q0": 0.945821,
    "M0_Q100": 0.707236,
    "M100_Q100": 0.958131
   },
   "carve_st": {
    "M0_Q0": 1.81,
    "M100_Q0": -25.34,
    "M0_Q100": 3.72,
    "M100_Q100": 27.44
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -31.6,
   "q_lift_M0": -0.062496,
   "q_revoice_st_M0": -40.66,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -1.753,
    "M100_Q100": -3.888
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 16276.69,
    "M100_Q0": 456.06,
    "M0_Q100": 5423.97,
    "M100_Q100": 2714.44
   },
   "pole_r": {
    "M0_Q0": 0.901439,
    "M100_Q0": 0.975292,
    "M0_Q100": 0.279945,
    "M100_Q100": 0.99884
   },
   "zero_hz": {
    "M0_Q0": 10935.17,
    "M100_Q0": 11133.58,
    "M0_Q100": 6509.11,
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
    "M0_Q100": 3.16,
    "M100_Q100": 32.71
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -61.89,
   "q_lift_M0": -0.621494,
   "q_revoice_st_M0": -19.02,
   "scale_db": {
    "M0_Q0": -14.255,
    "M100_Q0": -4.828,
    "M0_Q100": -1.753,
    "M100_Q100": -3.888
   }
  }
 ]
}
```
