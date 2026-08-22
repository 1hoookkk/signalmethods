# DeadRinger — P2K reference 8 [REZ]

Architecture source: `P2k_008_DeadRinger.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**08 DeadRinger** [REZ] "Permanent ringy Q, many variations"
- IS: rings at BOTH ends — always ≥2 sections at r≥0.99 (2.4 k @1.00 at M0;
  849/541/4372 @0.98-0.99 at M100). The wheel picks WHICH set rings.
- MORPH: whole frame down ~2 octaves, ZERO crossings — rings never collide.
- Q: near-zero POSITIVE lift; S1 +0.17, S3 −0.15 — swaps which ring leads.
- RECIPE: never author a non-ringing pose; morph = ring selector. Daylight
  angle: tune the ring sets to musical intervals (ROM's are inharmonic).
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 14366 → 3622 | 0.810 → 0.994 | 0.984 → 0.995 | 2523 → 783 | 0.935 → 0.950 | -3.9/-6.8/-3.3/-7.4 |
| S2 | 2415 → 849 | 0.995 → 0.994 | 0.993 → 0.992 | 1067 → 1893 | 0.996 → 0.729 | -3.9/-6.8/-3.3/-7.4 |
| S3 | 0 → 541 | 0.998 → 0.980 | 0.848 → 0.999 | 10729 → 2376 | 0.946 → 0.966 | -3.9/-6.8/-3.3/-7.4 |
| S4 | 15607 → 4372 | 0.985 → 0.987 | 0.982 → 0.996 | 11846 → 3101 | 0.914 → 0.952 | -3.9/-6.8/-3.3/-7.4 |
| S5 | 10407 → 2154 | 0.991 → 0.983 | 0.884 → 0.984 | 15202 → 3774 | 0.954 → 0.956 | -3.9/-6.8/-3.3/-7.4 |
| S6 | 11243 → 2841 | 0.960 → 0.989 | 0.966 → 0.966 | 11134 → 10151 | 1.000 → 1.000 | -3.9/-6.8/-3.3/-7.4 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 8,
 "name": "DeadRinger",
 "x3_type": "REZ",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_008_DeadRinger.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 14366.02,
    "M100_Q0": 3621.98,
    "M0_Q100": 12516.1,
    "M100_Q100": 4430.2
   },
   "pole_r": {
    "M0_Q0": 0.810206,
    "M100_Q0": 0.993881,
    "M0_Q100": 0.984257,
    "M100_Q100": 0.994863
   },
   "zero_hz": {
    "M0_Q0": 2523.28,
    "M100_Q0": 783.33,
    "M0_Q100": 2523.28,
    "M100_Q100": 783.33
   },
   "zero_r": {
    "M0_Q0": 0.935439,
    "M100_Q0": 0.949942,
    "M0_Q100": 0.935439,
    "M100_Q100": 0.949942
   },
   "carve_st": {
    "M0_Q0": -30.11,
    "M100_Q0": -26.51,
    "M0_Q100": -27.72,
    "M100_Q100": -30.0
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -23.85,
   "q_lift_M0": 0.174051,
   "q_revoice_st_M0": -2.39,
   "scale_db": {
    "M0_Q0": -3.876,
    "M100_Q0": -6.835,
    "M0_Q100": -3.292,
    "M100_Q100": -7.389
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 2415.36,
    "M100_Q0": 848.96,
    "M0_Q100": 2779.77,
    "M100_Q100": 2781.06
   },
   "pole_r": {
    "M0_Q0": 0.995108,
    "M100_Q0": 0.994126,
    "M0_Q100": 0.992652,
    "M100_Q100": 0.99167
   },
   "zero_hz": {
    "M0_Q0": 1067.13,
    "M100_Q0": 1893.32,
    "M0_Q100": 1067.13,
    "M100_Q100": 1893.32
   },
   "zero_r": {
    "M0_Q0": 0.995844,
    "M100_Q0": 0.728995,
    "M0_Q100": 0.995844,
    "M100_Q100": 0.728995
   },
   "carve_st": {
    "M0_Q0": -14.14,
    "M100_Q0": 13.89,
    "M0_Q100": -16.57,
    "M100_Q100": -6.66
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -18.1,
   "q_lift_M0": -0.002456,
   "q_revoice_st_M0": 2.43,
   "scale_db": {
    "M0_Q0": -3.876,
    "M100_Q0": -6.835,
    "M0_Q100": -3.292,
    "M100_Q100": -7.389
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 541.09,
    "M0_Q100": 10335.9,
    "M100_Q100": 740.44
   },
   "pole_r": {
    "M0_Q0": 0.998169,
    "M100_Q0": 0.980286,
    "M0_Q100": 0.847899,
    "M100_Q100": 0.998963
   },
   "zero_hz": {
    "M0_Q0": 10728.97,
    "M100_Q0": 2375.77,
    "M0_Q100": 10728.97,
    "M100_Q100": 2375.77
   },
   "zero_r": {
    "M0_Q0": 0.945821,
    "M100_Q0": 0.96625,
    "M0_Q100": 0.945821,
    "M100_Q100": 0.96625
   },
   "carve_st": {
    "M0_Q0": null,
    "M100_Q0": 25.61,
    "M0_Q100": 0.65,
    "M100_Q100": 20.18
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": null,
   "q_lift_M0": -0.15027,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -3.876,
    "M100_Q0": -6.835,
    "M0_Q100": -3.292,
    "M100_Q100": -7.389
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 15607.02,
    "M100_Q0": 4371.87,
    "M0_Q100": 14461.07,
    "M100_Q100": 3618.87
   },
   "pole_r": {
    "M0_Q0": 0.985248,
    "M100_Q0": 0.987229,
    "M0_Q100": 0.982276,
    "M100_Q100": 0.995599
   },
   "zero_hz": {
    "M0_Q0": 11845.71,
    "M100_Q0": 3100.98,
    "M0_Q100": 11845.71,
    "M100_Q100": 3100.98
   },
   "zero_r": {
    "M0_Q0": 0.914346,
    "M100_Q0": 0.951996,
    "M0_Q100": 0.914346,
    "M100_Q100": 0.951996
   },
   "carve_st": {
    "M0_Q0": -4.77,
    "M100_Q0": -5.95,
    "M0_Q100": -3.45,
    "M100_Q100": -2.67
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -22.03,
   "q_lift_M0": -0.002972,
   "q_revoice_st_M0": -1.32,
   "scale_db": {
    "M0_Q0": -3.876,
    "M100_Q0": -6.835,
    "M0_Q100": -3.292,
    "M100_Q100": -7.389
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 10406.82,
    "M100_Q0": 2154.04,
    "M0_Q100": 10978.4,
    "M100_Q100": 2490.99
   },
   "pole_r": {
    "M0_Q0": 0.991178,
    "M100_Q0": 0.98327,
    "M0_Q100": 0.883935,
    "M100_Q100": 0.984257
   },
   "zero_hz": {
    "M0_Q0": 15201.83,
    "M100_Q0": 3774.01,
    "M0_Q100": 15201.83,
    "M100_Q100": 3774.01
   },
   "zero_r": {
    "M0_Q0": 0.954045,
    "M100_Q0": 0.95609,
    "M0_Q100": 0.954045,
    "M100_Q100": 0.95609
   },
   "carve_st": {
    "M0_Q0": 6.56,
    "M100_Q0": 9.71,
    "M0_Q100": 5.63,
    "M100_Q100": 7.19
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -27.27,
   "q_lift_M0": -0.107243,
   "q_revoice_st_M0": 0.93,
   "scale_db": {
    "M0_Q0": -3.876,
    "M100_Q0": -6.835,
    "M0_Q100": -3.292,
    "M100_Q100": -7.389
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 11243.16,
    "M100_Q0": 2841.05,
    "M0_Q100": 166.27,
    "M100_Q100": 587.45
   },
   "pole_r": {
    "M0_Q0": 0.960167,
    "M100_Q0": 0.988712,
    "M0_Q100": 0.96625,
    "M100_Q100": 0.96625
   },
   "zero_hz": {
    "M0_Q0": 11133.58,
    "M100_Q0": 10151.41,
    "M0_Q100": 8392.25,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": -0.17,
    "M100_Q0": 22.05,
    "M0_Q100": 67.89,
    "M100_Q100": 59.21
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -23.81,
   "q_lift_M0": 0.006083,
   "q_revoice_st_M0": -72.95,
   "scale_db": {
    "M0_Q0": -3.876,
    "M100_Q0": -6.835,
    "M0_Q100": -3.292,
    "M100_Q100": -7.389
   }
  }
 ]
}
```
