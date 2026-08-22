# MegaSweepz — P2K reference 1 [LPF]

Architecture source: `P2k_001_MegaSweepz.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**01 MegaSweepz** [LPF] "'Loud' LPF with a hard Q"
- IS: M0 = air-frame lattice (poles 9.7–16.3 k, near-cancelling zeros) — the
  "open" pose is barely a filter. M100 = everything crushed under ~6 k with
  r→1.0 mids (1.2 k @ r=1.00) — the "loud" is resonant mids, not level.
- MORPH: the whole frame slides down 2–3 octaves (−13..−42 st) — the DJ sweep.
- Q: violent and asymmetric — S1 +0.23, S6 +0.22, S2 −0.66 (a section DEFUSES
  as others arm: a relay in the Q axis).
- RECIPE: build the open pose as near-transparent lattice, closed pose as a
  mid-heavy wall; give Q an arming/defusing trade, never a uniform lift.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 12893 → 6049 | 0.760 → 0.500 | 0.994 → 0.999 | 13401 → 5605 | 0.986 → 0.987 | -4.7/-9.5/-2.0/-7.3 |
| S2 | 10329 → 1200 | 0.944 → 0.998 | 0.280 → 0.996 | 10959 → 2374 | 0.857 → 0.968 | -4.7/-9.5/-2.0/-7.3 |
| S3 | 16277 → 3475 | 0.901 → 0.972 | 0.956 → 0.997 | 16287 → 4948 | 0.919 → 0.988 | -4.7/-9.5/-2.0/-7.3 |
| S4 | 13615 → 5606 | 0.968 → 0.987 | 0.994 → 0.999 | 14391 → 1619 | 0.968 → 0.972 | -4.7/-9.5/-2.0/-7.3 |
| S5 | 9713 → 2884 | 0.976 → 0.997 | 0.866 → 0.801 | 16811 → 839 | 0.638 → 0.970 | -4.7/-9.5/-2.0/-7.3 |
| S6 | 1295 → 113 | 0.771 → 0.994 | 0.994 → 0.995 | 7162 → 17961 | 1.000 → 1.000 | -4.7/-9.5/-2.0/-7.3 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 1,
 "name": "MegaSweepz",
 "x3_type": "LPF",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_001_MegaSweepz.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 12892.96,
    "M100_Q0": 6049.03,
    "M0_Q100": 14563.49,
    "M100_Q100": 5936.6
   },
   "pole_r": {
    "M0_Q0": 0.760466,
    "M100_Q0": 0.500244,
    "M0_Q100": 0.993881,
    "M100_Q100": 0.998535
   },
   "zero_hz": {
    "M0_Q0": 13401.26,
    "M100_Q0": 5604.75,
    "M0_Q100": 19531.25,
    "M100_Q100": 839.37
   },
   "zero_r": {
    "M0_Q0": 0.986239,
    "M100_Q0": 0.987229,
    "M0_Q100": 0.883116,
    "M100_Q100": 0.970273
   },
   "carve_st": {
    "M0_Q0": 0.67,
    "M100_Q0": -1.32,
    "M0_Q100": 5.08,
    "M100_Q100": -33.87
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -13.1,
   "q_lift_M0": 0.233415,
   "q_revoice_st_M0": 2.11,
   "scale_db": {
    "M0_Q0": -4.728,
    "M100_Q0": -9.472,
    "M0_Q100": -2.043,
    "M100_Q100": -7.337
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 10329.14,
    "M100_Q0": 1200.36,
    "M0_Q100": 5423.97,
    "M100_Q100": 2477.53
   },
   "pole_r": {
    "M0_Q0": 0.943754,
    "M100_Q0": 0.997802,
    "M0_Q100": 0.279945,
    "M100_Q100": 0.996333
   },
   "zero_hz": {
    "M0_Q0": 10958.81,
    "M100_Q0": 2374.4,
    "M0_Q100": 10958.81,
    "M100_Q100": 1619.34
   },
   "zero_r": {
    "M0_Q0": 0.857064,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.857064,
    "M100_Q100": 0.972284
   },
   "carve_st": {
    "M0_Q0": 1.02,
    "M100_Q0": 11.81,
    "M0_Q100": 12.18,
    "M100_Q100": -7.36
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -37.26,
   "q_lift_M0": -0.663809,
   "q_revoice_st_M0": -11.15,
   "scale_db": {
    "M0_Q0": -4.728,
    "M100_Q0": -9.472,
    "M0_Q100": -2.043,
    "M100_Q100": -7.337
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 16276.69,
    "M100_Q0": 3474.89,
    "M0_Q100": 2242.26,
    "M100_Q100": 1608.62
   },
   "pole_r": {
    "M0_Q0": 0.901439,
    "M100_Q0": 0.972284,
    "M0_Q100": 0.95609,
    "M100_Q100": 0.996578
   },
   "zero_hz": {
    "M0_Q0": 16286.92,
    "M100_Q0": 4947.63,
    "M0_Q100": 13401.26,
    "M100_Q100": 2374.4
   },
   "zero_r": {
    "M0_Q0": 0.918608,
    "M100_Q0": 0.987723,
    "M0_Q100": 0.986239,
    "M100_Q100": 0.968258
   },
   "carve_st": {
    "M0_Q0": 0.01,
    "M100_Q0": 6.12,
    "M0_Q100": 30.95,
    "M100_Q100": 6.74
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -26.73,
   "q_lift_M0": 0.054651,
   "q_revoice_st_M0": -34.32,
   "scale_db": {
    "M0_Q0": -4.728,
    "M100_Q0": -9.472,
    "M0_Q100": -2.043,
    "M100_Q100": -7.337
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13615.49,
    "M100_Q0": 5606.21,
    "M0_Q100": 16521.76,
    "M100_Q100": 4495.52
   },
   "pole_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.986734,
    "M0_Q100": 0.994372,
    "M100_Q100": 0.998657
   },
   "zero_hz": {
    "M0_Q0": 14391.16,
    "M100_Q0": 1619.34,
    "M0_Q100": 14391.16,
    "M100_Q100": 4947.63
   },
   "zero_r": {
    "M0_Q0": 0.968258,
    "M100_Q0": 0.972284,
    "M0_Q100": 0.968258,
    "M100_Q100": 0.987723
   },
   "carve_st": {
    "M0_Q0": 0.96,
    "M100_Q0": -21.5,
    "M0_Q100": -2.39,
    "M100_Q100": 1.66
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -15.36,
   "q_lift_M0": 0.026114,
   "q_revoice_st_M0": 3.35,
   "scale_db": {
    "M0_Q0": -4.728,
    "M100_Q0": -9.472,
    "M0_Q100": -2.043,
    "M100_Q100": -7.337
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 9713.47,
    "M100_Q0": 2884.31,
    "M0_Q100": 12796.64,
    "M100_Q100": 5003.35
   },
   "pole_r": {
    "M0_Q0": 0.976293,
    "M100_Q0": 0.997312,
    "M0_Q100": 0.866078,
    "M100_Q100": 0.800505
   },
   "zero_hz": {
    "M0_Q0": 16811.22,
    "M100_Q0": 839.37,
    "M0_Q100": 2620.35,
    "M100_Q100": 5604.75
   },
   "zero_r": {
    "M0_Q0": 0.637569,
    "M100_Q0": 0.970273,
    "M0_Q100": 0.625195,
    "M100_Q100": 0.987229
   },
   "carve_st": {
    "M0_Q0": 9.5,
    "M100_Q0": -21.37,
    "M0_Q100": -27.46,
    "M100_Q100": 1.97
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -21.02,
   "q_lift_M0": -0.110215,
   "q_revoice_st_M0": 4.77,
   "scale_db": {
    "M0_Q0": -4.728,
    "M100_Q0": -9.472,
    "M0_Q100": -2.043,
    "M100_Q100": -7.337
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 1294.65,
    "M100_Q0": 112.97,
    "M0_Q100": 13548.46,
    "M100_Q100": 175.69
   },
   "pole_r": {
    "M0_Q0": 0.770671,
    "M100_Q0": 0.993881,
    "M0_Q100": 0.993881,
    "M100_Q100": 0.994617
   },
   "zero_hz": {
    "M0_Q0": 7162.29,
    "M100_Q0": 17960.83,
    "M0_Q100": 9569.05,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 29.61,
    "M100_Q0": 87.75,
    "M0_Q100": -6.02,
    "M100_Q100": 80.11
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -42.22,
   "q_lift_M0": 0.22321,
   "q_revoice_st_M0": 40.65,
   "scale_db": {
    "M0_Q0": -4.728,
    "M100_Q0": -9.472,
    "M0_Q100": -2.043,
    "M100_Q100": -7.337
   }
  }
 ]
}
```
