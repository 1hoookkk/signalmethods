# TalkingHedz — P2K reference 13 [VOW]

Architecture source: `P2k_013_TalkingHedz.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**13 TalkingHedz** [VOW] "'Oui' morphing filter. Q adds peaks"
- IS: full vowel column 199 Hz–9.3 k, formants at 891/1570/2348/4607 (the
  measured aw), zeros shaping the mouth between them.
- MORPH: 'oui' = S2 drops to 201 (chest) as S6 rises 199→1789 (+38 st) —
  the ANCHOR RELAY: the frame never loses its floor because two sections
  trade it mid-word.
- Q: small uniform (+0.01..+0.05) — same words, more spit.
- RECIPE: pick the two vowel poses, then wire the relay: one section's END
  is another's START. The relay is what makes it talk instead of sweep.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 9321 → 8376 | 0.975 → 0.962 | 0.999 → 0.999 | 347 → 1711 | 0.935 → 0.944 | -5.0/-5.7/-5.2/-5.8 |
| S2 | 891 → 201 | 0.979 → 0.992 | 0.999 → 0.999 | 1113 → 806 | 0.946 → 0.962 | -5.0/-5.7/-5.2/-5.8 |
| S3 | 1570 → 2363 | 0.977 → 0.982 | 0.999 → 0.999 | 2014 → 2300 | 0.960 → 0.888 | -5.0/-5.7/-5.2/-5.8 |
| S4 | 2348 → 2732 | 0.997 → 0.985 | 0.999 → 0.999 | 2971 → 2885 | 0.972 → 0.946 | -5.0/-5.7/-5.2/-5.8 |
| S5 | 4607 → 4782 | 0.948 → 0.935 | 0.996 → 0.972 | 7922 → 5989 | 0.750 → 0.685 | -5.0/-5.7/-5.2/-5.8 |
| S6 | 199 → 1789 | 0.991 → 0.997 | 0.999 → 0.999 | 6396 → 17313 | 1.000 → 1.000 | -5.0/-5.7/-5.2/-5.8 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 13,
 "name": "TalkingHedz",
 "x3_type": "VOW",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_013_TalkingHedz.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 9320.86,
    "M100_Q0": 8376.03,
    "M0_Q100": 10157.86,
    "M100_Q100": 8989.02
   },
   "pole_r": {
    "M0_Q0": 0.975292,
    "M100_Q0": 0.962199,
    "M0_Q100": 0.999023,
    "M100_Q100": 0.999115
   },
   "zero_hz": {
    "M0_Q0": 346.74,
    "M100_Q0": 1710.72,
    "M0_Q100": 192.11,
    "M100_Q100": 1617.91
   },
   "zero_r": {
    "M0_Q0": 0.935439,
    "M100_Q0": 0.943754,
    "M0_Q100": 0.922851,
    "M100_Q100": 0.954045
   },
   "carve_st": {
    "M0_Q0": -56.98,
    "M100_Q0": -27.5,
    "M0_Q100": -68.69,
    "M100_Q100": -29.69
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -1.85,
   "q_lift_M0": 0.023731,
   "q_revoice_st_M0": 1.49,
   "scale_db": {
    "M0_Q0": -5.007,
    "M100_Q0": -5.653,
    "M0_Q100": -5.213,
    "M100_Q100": -5.824
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 890.72,
    "M100_Q0": 200.92,
    "M0_Q100": 952.88,
    "M100_Q100": 194.24
   },
   "pole_r": {
    "M0_Q0": 0.979289,
    "M100_Q0": 0.99216,
    "M0_Q100": 0.999115,
    "M100_Q100": 0.999359
   },
   "zero_hz": {
    "M0_Q0": 1113.22,
    "M100_Q0": 805.7,
    "M0_Q100": 1073.62,
    "M100_Q100": 786.69
   },
   "zero_r": {
    "M0_Q0": 0.945821,
    "M100_Q0": 0.962199,
    "M0_Q100": 0.943754,
    "M100_Q100": 0.96625
   },
   "carve_st": {
    "M0_Q0": 3.86,
    "M100_Q0": 24.04,
    "M0_Q100": 2.07,
    "M100_Q100": 24.22
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -25.78,
   "q_lift_M0": 0.019826,
   "q_revoice_st_M0": 1.17,
   "scale_db": {
    "M0_Q0": -5.007,
    "M100_Q0": -5.653,
    "M0_Q100": -5.213,
    "M100_Q100": -5.824
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 1569.58,
    "M100_Q0": 2363.12,
    "M0_Q100": 1508.93,
    "M100_Q100": 2139.17
   },
   "pole_r": {
    "M0_Q0": 0.977293,
    "M100_Q0": 0.982276,
    "M0_Q100": 0.999146,
    "M100_Q100": 0.999237
   },
   "zero_hz": {
    "M0_Q0": 2014.45,
    "M100_Q0": 2300.49,
    "M0_Q100": 1933.04,
    "M100_Q100": 2062.34
   },
   "zero_r": {
    "M0_Q0": 0.960167,
    "M100_Q0": 0.888343,
    "M0_Q100": 0.962199,
    "M100_Q100": 0.888343
   },
   "carve_st": {
    "M0_Q0": 4.32,
    "M100_Q0": -0.47,
    "M0_Q100": 4.29,
    "M100_Q100": -0.63
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 7.08,
   "q_lift_M0": 0.021853,
   "q_revoice_st_M0": -0.68,
   "scale_db": {
    "M0_Q0": -5.007,
    "M100_Q0": -5.653,
    "M0_Q100": -5.213,
    "M100_Q100": -5.824
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 2348.03,
    "M100_Q0": 2731.8,
    "M0_Q100": 2210.16,
    "M100_Q100": 2410.57
   },
   "pole_r": {
    "M0_Q0": 0.996945,
    "M100_Q0": 0.985248,
    "M0_Q100": 0.999176,
    "M100_Q100": 0.999176
   },
   "zero_hz": {
    "M0_Q0": 2971.28,
    "M100_Q0": 2885.37,
    "M0_Q100": 2856.26,
    "M100_Q100": 2583.85
   },
   "zero_r": {
    "M0_Q0": 0.972284,
    "M100_Q0": 0.945821,
    "M0_Q100": 0.976293,
    "M100_Q100": 0.945821
   },
   "carve_st": {
    "M0_Q0": 4.08,
    "M100_Q0": 0.95,
    "M0_Q100": 4.44,
    "M100_Q100": 1.2
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 2.62,
   "q_lift_M0": 0.002231,
   "q_revoice_st_M0": -1.05,
   "scale_db": {
    "M0_Q0": -5.007,
    "M100_Q0": -5.653,
    "M0_Q100": -5.213,
    "M100_Q100": -5.824
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 4606.88,
    "M100_Q0": 4782.12,
    "M0_Q100": 4351.71,
    "M100_Q100": 4403.84
   },
   "pole_r": {
    "M0_Q0": 0.947884,
    "M100_Q0": 0.935439,
    "M0_Q100": 0.996333,
    "M100_Q100": 0.972284
   },
   "zero_hz": {
    "M0_Q0": 7921.93,
    "M100_Q0": 5988.62,
    "M0_Q100": 8049.24,
    "M100_Q100": 5466.81
   },
   "zero_r": {
    "M0_Q0": 0.750122,
    "M100_Q0": 0.684831,
    "M0_Q100": 0.718198,
    "M100_Q100": 0.637569
   },
   "carve_st": {
    "M0_Q0": 9.38,
    "M100_Q0": 3.89,
    "M0_Q100": 10.65,
    "M100_Q100": 3.74
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 0.65,
   "q_lift_M0": 0.048449,
   "q_revoice_st_M0": -0.99,
   "scale_db": {
    "M0_Q0": -5.007,
    "M100_Q0": -5.653,
    "M0_Q100": -5.213,
    "M100_Q100": -5.824
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 199.43,
    "M100_Q0": 1789.21,
    "M0_Q100": 157.28,
    "M100_Q100": 1508.98
   },
   "pole_r": {
    "M0_Q0": 0.991178,
    "M100_Q0": 0.996578,
    "M0_Q100": 0.999146,
    "M100_Q100": 0.999084
   },
   "zero_hz": {
    "M0_Q0": 6396.33,
    "M100_Q0": 17312.96,
    "M0_Q100": 6050.18,
    "M100_Q100": 17312.96
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 60.04,
    "M100_Q0": 39.29,
    "M0_Q100": 63.19,
    "M100_Q100": 42.24
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": 37.98,
   "q_lift_M0": 0.007968,
   "q_revoice_st_M0": -4.11,
   "scale_db": {
    "M0_Q0": -5.007,
    "M100_Q0": -5.653,
    "M0_Q100": -5.213,
    "M100_Q100": -5.824
   }
  }
 ]
}
```
