# DeepBouche — P2K reference 22 [VOW]

Architecture source: `P2k_022_DeepBouche.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**22 DeepBouche** [VOW] "French vowels, Ou-Est at low Q"
- IS: DOWNWARD formant stack (3484 down to 228 — sections ordered high→low,
  unique in the vowel set) with the sub section scale-cut −18 dB: throat,
  not lips. All r=0.97–1.0: a dark face fully armed.
- MORPH: nearly static (±2 st) except S1 +14 st — 'ou'→'est' is ONE formant
  fronting while the throat holds.
- Q: essentially zero — the vowel is already at tension; Q would break the
  face. E-MU left it alone.
- RECIPE: order sections against convention (descending), cut the sub scale,
  move one formant only. Round-vowel spec for the mic session.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 3484 → 7747 | 0.966 → 0.962 | 0.977 → 0.998 | 3319 → 6526 | 0.942 → 0.848 | -6.2/-3.1/-7.0/-8.1 |
| S2 | 2958 → 3060 | 0.983 → 0.986 | 0.992 → 0.998 | 2647 → 2880 | 0.946 → 0.952 | -6.2/-3.1/-7.0/-8.1 |
| S3 | 2543 → 2782 | 0.993 → 0.991 | 0.998 → 0.999 | 2372 → 2171 | 0.971 → 0.948 | -6.2/-3.1/-7.0/-8.1 |
| S4 | 1917 → 2217 | 0.993 → 0.993 | 0.998 → 0.999 | 1225 → 1383 | 0.954 → 0.964 | -6.2/-3.1/-7.0/-8.1 |
| S5 | 1562 → 1655 | 0.995 → 0.997 | 0.998 → 0.999 | 373 → 407 | 0.946 → 0.958 | -6.2/-3.1/-7.0/-8.1 |
| S6 | 228 → 329 | 0.998 → 0.999 | 0.997 → 0.999 | 17961 → 10151 | 1.000 → 1.000 | -18.3/-15.1/-25.1/-20.1 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 22,
 "name": "DeepBouche",
 "x3_type": "VOW",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_022_DeepBouche.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 3483.77,
    "M100_Q0": 7746.53,
    "M0_Q100": 5110.94,
    "M100_Q100": 2827.94
   },
   "pole_r": {
    "M0_Q0": 0.96625,
    "M100_Q0": 0.962199,
    "M0_Q100": 0.977293,
    "M100_Q100": 0.998351
   },
   "zero_hz": {
    "M0_Q0": 3318.8,
    "M100_Q0": 6525.65,
    "M0_Q100": 3855.59,
    "M100_Q100": 2723.27
   },
   "zero_r": {
    "M0_Q0": 0.941682,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.960167,
    "M100_Q100": 0.99216
   },
   "carve_st": {
    "M0_Q0": -0.84,
    "M100_Q0": -2.97,
    "M0_Q100": -4.88,
    "M100_Q100": -0.65
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 13.83,
   "q_lift_M0": 0.011043,
   "q_revoice_st_M0": 6.64,
   "scale_db": {
    "M0_Q0": -6.248,
    "M100_Q0": -3.108,
    "M0_Q100": -7.003,
    "M100_Q100": -8.076
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 2957.69,
    "M100_Q0": 3060.37,
    "M0_Q100": 3050.84,
    "M100_Q100": 3243.01
   },
   "pole_r": {
    "M0_Q0": 0.98327,
    "M100_Q0": 0.985744,
    "M0_Q100": 0.992406,
    "M100_Q100": 0.997924
   },
   "zero_hz": {
    "M0_Q0": 2646.72,
    "M100_Q0": 2880.44,
    "M0_Q100": 2589.54,
    "M100_Q100": 3256.9
   },
   "zero_r": {
    "M0_Q0": 0.945821,
    "M100_Q0": 0.951996,
    "M0_Q100": 0.931278,
    "M100_Q100": 0.989205
   },
   "carve_st": {
    "M0_Q0": -1.92,
    "M100_Q0": -1.05,
    "M0_Q100": -2.84,
    "M100_Q100": 0.07
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 0.59,
   "q_lift_M0": 0.009136,
   "q_revoice_st_M0": 0.54,
   "scale_db": {
    "M0_Q0": -6.248,
    "M100_Q0": -3.108,
    "M0_Q100": -7.003,
    "M100_Q100": -8.076
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 2543.19,
    "M100_Q0": 2781.7,
    "M0_Q100": 2211.39,
    "M100_Q100": 3092.23
   },
   "pole_r": {
    "M0_Q0": 0.993389,
    "M100_Q0": 0.991178,
    "M0_Q100": 0.998046,
    "M100_Q100": 0.999023
   },
   "zero_hz": {
    "M0_Q0": 2372.23,
    "M100_Q0": 2171.07,
    "M0_Q100": 2304.16,
    "M100_Q100": 2000.63
   },
   "zero_r": {
    "M0_Q0": 0.971279,
    "M100_Q0": 0.947884,
    "M0_Q100": 0.971279,
    "M100_Q100": 0.987229
   },
   "carve_st": {
    "M0_Q0": -1.2,
    "M100_Q0": -4.29,
    "M0_Q100": 0.71,
    "M100_Q100": -7.54
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 1.55,
   "q_lift_M0": 0.004657,
   "q_revoice_st_M0": -2.42,
   "scale_db": {
    "M0_Q0": -6.248,
    "M100_Q0": -3.108,
    "M0_Q100": -7.003,
    "M100_Q100": -8.076
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 1916.55,
    "M100_Q0": 2216.51,
    "M0_Q100": 1349.75,
    "M100_Q100": 2536.24
   },
   "pole_r": {
    "M0_Q0": 0.993389,
    "M100_Q0": 0.993143,
    "M0_Q100": 0.997802,
    "M100_Q100": 0.999023
   },
   "zero_hz": {
    "M0_Q0": 1225.11,
    "M100_Q0": 1383.09,
    "M0_Q100": 2455.46,
    "M100_Q100": 778.06
   },
   "zero_r": {
    "M0_Q0": 0.954045,
    "M100_Q0": 0.964227,
    "M0_Q100": 0.905762,
    "M100_Q100": 0.998351
   },
   "carve_st": {
    "M0_Q0": -7.75,
    "M100_Q0": -8.16,
    "M0_Q100": 10.36,
    "M100_Q100": -20.46
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 2.52,
   "q_lift_M0": 0.004413,
   "q_revoice_st_M0": -6.07,
   "scale_db": {
    "M0_Q0": -6.248,
    "M100_Q0": -3.108,
    "M0_Q100": -7.003,
    "M100_Q100": -8.076
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 1561.91,
    "M100_Q0": 1655.48,
    "M0_Q100": 992.47,
    "M100_Q100": 1911.53
   },
   "pole_r": {
    "M0_Q0": 0.994617,
    "M100_Q0": 0.9967,
    "M0_Q100": 0.997802,
    "M100_Q100": 0.999023
   },
   "zero_hz": {
    "M0_Q0": 373.32,
    "M100_Q0": 406.93,
    "M0_Q100": 0.0,
    "M100_Q100": 4891.83
   },
   "zero_r": {
    "M0_Q0": 0.945821,
    "M100_Q0": 0.958131,
    "M0_Q100": 0.948421,
    "M100_Q100": 0.177466
   },
   "carve_st": {
    "M0_Q0": -24.78,
    "M100_Q0": -24.29,
    "M0_Q100": null,
    "M100_Q100": 16.27
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 1.01,
   "q_lift_M0": 0.003185,
   "q_revoice_st_M0": -7.85,
   "scale_db": {
    "M0_Q0": -6.248,
    "M100_Q0": -3.108,
    "M0_Q100": -7.003,
    "M100_Q100": -8.076
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 227.52,
    "M100_Q0": 329.47,
    "M0_Q100": 434.64,
    "M100_Q100": 242.83
   },
   "pole_r": {
    "M0_Q0": 0.997557,
    "M100_Q0": 0.999115,
    "M0_Q100": 0.996945,
    "M100_Q100": 0.999023
   },
   "zero_hz": {
    "M0_Q0": 17960.83,
    "M100_Q0": 10151.41,
    "M0_Q100": 17960.83,
    "M100_Q100": 7581.62
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 75.63,
    "M100_Q0": 59.34,
    "M0_Q100": 64.43,
    "M100_Q100": 59.57
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": 6.41,
   "q_lift_M0": -0.000612,
   "q_revoice_st_M0": 11.21,
   "scale_db": {
    "M0_Q0": -18.29,
    "M100_Q0": -15.149,
    "M0_Q100": -25.065,
    "M100_Q100": -20.117
   }
  }
 ]
}
```
