# RogueHertz — P2K reference 17 [EQ+]

Architecture source: `P2k_017_RogueHertz.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**17 RogueHertz** [EQ+] "Bass with mid boost and smooth Q"
- IS: bass floor + the "shouldn't work" mid bump at 913 Hz backed by a zero
  at 4 k; top held by weak real zeros.
- MORPH: mids reshuffle upward (S3 +39 st), floor stays — the bump migrates.
- Q: tiny everywhere (+0.02 max) — "smooth Q" = radius almost untouched;
  Q100 rows show small Hz nudges instead (gentle tuner).
- RECIPE: one deliberate wrong-place bump, kept smooth on both axes. The
  character is a mix decision, not a resonance.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 12180 → 11595 | 0.927 → 0.968 | 0.950 → 0.997 | 0 → 0 | 0.374 → 0.979 | -3.2/-0.7/-8.2/-2.0 |
| S2 | 0 → 385 | 0.992 → 0.990 | 0.999 → 0.998 | 485 → 8721 | 0.950 → 0.750 | -3.2/-0.7/-8.2/-2.0 |
| S3 | 913 → 8634 | 0.982 → 0.952 | 0.981 → 0.995 | 4032 → 11099 | 0.897 → 0.952 | -3.2/-0.7/-8.2/-2.0 |
| S4 | 15772 → 12781 | 0.994 → 0.958 | 0.997 → 0.995 | 11735 → 12237 | 0.976 → 0.685 | -3.2/-0.7/-8.2/-2.0 |
| S5 | 4389 → 10582 | 0.979 → 0.966 | 0.980 → 0.990 | 15491 → 12367 | 0.956 → 0.931 | -3.2/-0.7/-8.2/-2.0 |
| S6 | 10813 → 14876 | 0.906 → 0.650 | 0.931 → 0.662 | 7162 → 9569 | 1.000 → 1.000 | -3.2/-0.7/-8.2/-2.0 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 17,
 "name": "RogueHertz",
 "x3_type": "EQ+",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_017_RogueHertz.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 12179.85,
    "M100_Q0": 11594.53,
    "M0_Q100": 11537.36,
    "M100_Q100": 11357.97
   },
   "pole_r": {
    "M0_Q0": 0.927074,
    "M100_Q0": 0.968258,
    "M0_Q100": 0.949942,
    "M100_Q100": 0.996945
   },
   "zero_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 0.0,
    "M0_Q100": 0.0,
    "M100_Q100": 0.0
   },
   "zero_r": {
    "M0_Q0": 0.374323,
    "M100_Q0": 0.979415,
    "M0_Q100": 0.374323,
    "M100_Q100": 0.979415
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
   "travel_st_Q0": -0.85,
   "q_lift_M0": 0.022868,
   "q_revoice_st_M0": -0.94,
   "scale_db": {
    "M0_Q0": -3.222,
    "M100_Q0": -0.703,
    "M0_Q100": -8.157,
    "M100_Q100": -2.003
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 385.23,
    "M0_Q100": 0.0,
    "M100_Q100": 495.69
   },
   "pole_r": {
    "M0_Q0": 0.991703,
    "M100_Q0": 0.989699,
    "M0_Q100": 0.999115,
    "M100_Q100": 0.997924
   },
   "zero_hz": {
    "M0_Q0": 485.46,
    "M100_Q0": 8720.93,
    "M0_Q100": 485.46,
    "M100_Q100": 8720.93
   },
   "zero_r": {
    "M0_Q0": 0.949942,
    "M100_Q0": 0.750122,
    "M0_Q100": 0.949942,
    "M100_Q100": 0.750122
   },
   "carve_st": {
    "M0_Q0": null,
    "M100_Q0": 54.01,
    "M0_Q100": null,
    "M100_Q100": 49.64
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": null,
   "q_lift_M0": 0.007412,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -3.222,
    "M100_Q0": -0.703,
    "M0_Q100": -8.157,
    "M100_Q100": -2.003
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 913.27,
    "M100_Q0": 8634.43,
    "M0_Q100": 891.56,
    "M100_Q100": 9600.05
   },
   "pole_r": {
    "M0_Q0": 0.982276,
    "M100_Q0": 0.951996,
    "M0_Q100": 0.981282,
    "M100_Q100": 0.994863
   },
   "zero_hz": {
    "M0_Q0": 4031.66,
    "M100_Q0": 11098.99,
    "M0_Q100": 4031.66,
    "M100_Q100": 11098.99
   },
   "zero_r": {
    "M0_Q0": 0.897095,
    "M100_Q0": 0.951996,
    "M0_Q100": 0.897095,
    "M100_Q100": 0.951996
   },
   "carve_st": {
    "M0_Q0": 25.71,
    "M100_Q0": 4.35,
    "M0_Q100": 26.12,
    "M100_Q100": 2.51
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 38.89,
   "q_lift_M0": -0.000994,
   "q_revoice_st_M0": -0.42,
   "scale_db": {
    "M0_Q0": -3.222,
    "M100_Q0": -0.703,
    "M0_Q100": -8.157,
    "M100_Q100": -2.003
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 15772.14,
    "M100_Q0": 12781.35,
    "M0_Q100": 15090.09,
    "M100_Q100": 12846.12
   },
   "pole_r": {
    "M0_Q0": 0.994126,
    "M100_Q0": 0.958131,
    "M0_Q100": 0.996578,
    "M100_Q100": 0.995108
   },
   "zero_hz": {
    "M0_Q0": 11735.47,
    "M100_Q0": 12237.16,
    "M0_Q100": 11735.47,
    "M100_Q100": 12237.16
   },
   "zero_r": {
    "M0_Q0": 0.976293,
    "M100_Q0": 0.684831,
    "M0_Q100": 0.976293,
    "M100_Q100": 0.684831
   },
   "carve_st": {
    "M0_Q0": -5.12,
    "M100_Q0": -0.75,
    "M0_Q100": -4.35,
    "M100_Q100": -0.84
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -3.64,
   "q_lift_M0": 0.002452,
   "q_revoice_st_M0": -0.77,
   "scale_db": {
    "M0_Q0": -3.222,
    "M100_Q0": -0.703,
    "M0_Q100": -8.157,
    "M100_Q100": -2.003
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 4389.03,
    "M100_Q0": 10581.51,
    "M0_Q100": 3734.33,
    "M100_Q100": 5469.76
   },
   "pole_r": {
    "M0_Q0": 0.979289,
    "M100_Q0": 0.96625,
    "M0_Q100": 0.980286,
    "M100_Q100": 0.990192
   },
   "zero_hz": {
    "M0_Q0": 15491.06,
    "M100_Q0": 12366.78,
    "M0_Q100": 15491.06,
    "M100_Q100": 12366.78
   },
   "zero_r": {
    "M0_Q0": 0.95609,
    "M100_Q0": 0.931278,
    "M0_Q100": 0.95609,
    "M100_Q100": 0.931278
   },
   "carve_st": {
    "M0_Q0": 21.83,
    "M100_Q0": 2.7,
    "M0_Q100": 24.63,
    "M100_Q100": 14.12
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": 15.23,
   "q_lift_M0": 0.000997,
   "q_revoice_st_M0": -2.8,
   "scale_db": {
    "M0_Q0": -3.222,
    "M100_Q0": -0.703,
    "M0_Q100": -8.157,
    "M100_Q100": -2.003
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 10812.56,
    "M100_Q0": 14876.5,
    "M0_Q100": 10206.56,
    "M100_Q100": 15083.28
   },
   "pole_r": {
    "M0_Q0": 0.905762,
    "M100_Q0": 0.649707,
    "M0_Q100": 0.931278,
    "M100_Q100": 0.661622
   },
   "zero_hz": {
    "M0_Q0": 7162.29,
    "M100_Q0": 9569.05,
    "M0_Q100": 10737.97,
    "M100_Q100": 14474.2
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": -7.13,
    "M100_Q0": -7.64,
    "M0_Q100": 0.88,
    "M100_Q100": -0.71
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": 5.52,
   "q_lift_M0": 0.025516,
   "q_revoice_st_M0": -1.0,
   "scale_db": {
    "M0_Q0": -3.222,
    "M100_Q0": -0.703,
    "M0_Q100": -8.157,
    "M100_Q100": -2.003
   }
  }
 ]
}
```
