# Ace of Bass — P2K reference 0 [EQ+]

Architecture source: `P2k_000_Ace of Bass.json` (datum 39062.5 Hz). Lanes are fixed slot numbers; never frequency-sort them.

## Dossier (verbatim, recipes/INTENT.md — manual quote included)

```text
**00 Ace of Bass** [EQ+] "Bass-boost to bass-cut morph"
- IS: M0 is an anti-bass pose — a REAL pole pair at r=1.0 pinned at DC with a
  13 dB-down scale and zeros stacked 12–18 k (dark, hollowed). M100 is the
  boost pose: poles respread 800–16.7 k at gentle r=0.85–0.87, scale −3 dB.
- MORPH: polarity ride — the low shelf swings cut→flat→boost while the top
  re-opens; travel is all downward (−8..−34 st), the frame collapsing onto bass.
- Q: almost nothing except S6 (+0.15) — one reserved bloomer.
- RECIPE: author two OPPOSITE tonal verdicts of the same spectrum (cut-pose /
  boost-pose), not two positions of one sweep. The wheel is a fader, so keep
  every section's job constant and flip only gain geography.
```

## Section anatomy (all numbers from the architecture)

| lane | pole Hz M0→M100 (Q0) | pole r Q0 | pole r Q100 | zero Hz M0→M100 (Q0) | zero r Q0 | scale dB (corners) |
|---|---|---|---|---|---|---|
| S1 | 0 → 16748 | 0.996 → 0.866 | 0.998 → 0.954 | 18217 → 2230 | 0.914 → 0.977 | -12.6/-2.8/-13.9/-2.6 |
| S2 | 3516 → 798 | 0.942 → 0.848 | 0.999 → 0.994 | 15340 → 2592 | 0.559 → 0.919 | -12.6/-2.8/-13.9/-2.6 |
| S3 | 10259 → 4429 | 0.954 → 0.857 | 0.998 → 0.996 | 13416 → 4871 | 0.718 → 0.750 | -12.6/-2.8/-13.9/-2.6 |
| S4 | 13647 → 7981 | 0.986 → 0.857 | 0.998 → 0.996 | 12014 → 8874 | 0.871 → 0.946 | -12.6/-2.8/-13.9/-2.6 |
| S5 | 15731 → 9877 | 0.996 → 0.848 | 0.984 → 0.976 | 15690 → 14994 | 0.946 → 0.966 | -12.6/-2.8/-13.9/-2.6 |
| S6 | 16269 → 2247 | 0.848 → 0.848 | 0.998 → 0.988 | 17961 → 9375 | 1.000 → 1.000 | -12.6/-2.8/-13.9/-2.6 |

## Machine block (architecture JSON, whole)

```json
{
 "schema": "trench-architecture-v1",
 "index": 0,
 "name": "Ace of Bass",
 "x3_type": "EQ+",
 "datum_sr_hz": 39062.5,
 "source": "dossiers/characters/P2k_000_Ace of Bass.json",
 "sections": [
  {
   "slot": 1,
   "pole_hz": {
    "M0_Q0": 0.0,
    "M100_Q0": 16747.8,
    "M0_Q100": 86.99,
    "M100_Q100": 10669.12
   },
   "pole_r": {
    "M0_Q0": 0.995607,
    "M100_Q0": 0.866078,
    "M0_Q100": 0.998474,
    "M100_Q100": 0.954045
   },
   "zero_hz": {
    "M0_Q0": 18217.17,
    "M100_Q0": 2230.37,
    "M0_Q100": 17446.38,
    "M100_Q100": 1442.94
   },
   "zero_r": {
    "M0_Q0": 0.914346,
    "M100_Q0": 0.977293,
    "M0_Q100": 0.897095,
    "M100_Q100": 0.931278
   },
   "carve_st": {
    "M0_Q0": null,
    "M100_Q0": -34.9,
    "M0_Q100": 91.77,
    "M100_Q100": -34.64
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": null,
   "q_lift_M0": 0.002867,
   "q_revoice_st_M0": null,
   "scale_db": {
    "M0_Q0": -12.612,
    "M100_Q0": -2.767,
    "M0_Q100": -13.887,
    "M100_Q100": -2.636
   }
  },
  {
   "slot": 2,
   "pole_hz": {
    "M0_Q0": 3516.26,
    "M100_Q0": 798.04,
    "M0_Q100": 2882.46,
    "M100_Q100": 871.14
   },
   "pole_r": {
    "M0_Q0": 0.941682,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.998596,
    "M100_Q100": 0.993881
   },
   "zero_hz": {
    "M0_Q0": 15339.96,
    "M100_Q0": 2591.75,
    "M0_Q100": 3981.64,
    "M100_Q100": 2591.75
   },
   "zero_r": {
    "M0_Q0": 0.559235,
    "M100_Q0": 0.918608,
    "M0_Q100": 0.847899,
    "M100_Q100": 0.918608
   },
   "carve_st": {
    "M0_Q0": 25.5,
    "M100_Q0": 20.39,
    "M0_Q100": 5.59,
    "M100_Q100": 18.88
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -25.67,
   "q_lift_M0": 0.056914,
   "q_revoice_st_M0": -3.44,
   "scale_db": {
    "M0_Q0": -12.612,
    "M100_Q0": -2.767,
    "M0_Q100": -13.887,
    "M100_Q100": -2.636
   }
  },
  {
   "slot": 3,
   "pole_hz": {
    "M0_Q0": 10259.18,
    "M100_Q0": 4429.0,
    "M0_Q100": 15688.76,
    "M100_Q100": 4426.57
   },
   "pole_r": {
    "M0_Q0": 0.954045,
    "M100_Q0": 0.857064,
    "M0_Q100": 0.998351,
    "M100_Q100": 0.996455
   },
   "zero_hz": {
    "M0_Q0": 13415.75,
    "M100_Q0": 4870.88,
    "M0_Q100": 14150.96,
    "M100_Q100": 4207.68
   },
   "zero_r": {
    "M0_Q0": 0.718198,
    "M100_Q0": 0.750122,
    "M0_Q100": 0.96625,
    "M100_Q100": 0.897095
   },
   "carve_st": {
    "M0_Q0": 4.64,
    "M100_Q0": 1.65,
    "M0_Q100": -1.79,
    "M100_Q100": -0.88
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -14.54,
   "q_lift_M0": 0.044306,
   "q_revoice_st_M0": 7.35,
   "scale_db": {
    "M0_Q0": -12.612,
    "M100_Q0": -2.767,
    "M0_Q100": -13.887,
    "M100_Q100": -2.636
   }
  },
  {
   "slot": 4,
   "pole_hz": {
    "M0_Q0": 13646.66,
    "M100_Q0": 7981.11,
    "M0_Q100": 14240.36,
    "M100_Q100": 8010.17
   },
   "pole_r": {
    "M0_Q0": 0.985744,
    "M100_Q0": 0.857064,
    "M0_Q100": 0.997924,
    "M100_Q100": 0.995844
   },
   "zero_hz": {
    "M0_Q0": 12014.26,
    "M100_Q0": 8874.19,
    "M0_Q100": 16958.5,
    "M100_Q100": 8477.3
   },
   "zero_r": {
    "M0_Q0": 0.870577,
    "M100_Q0": 0.945821,
    "M0_Q100": 0.994863,
    "M100_Q100": 0.941682
   },
   "carve_st": {
    "M0_Q0": -2.21,
    "M100_Q0": 1.84,
    "M0_Q100": 3.02,
    "M100_Q100": 0.98
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -9.29,
   "q_lift_M0": 0.01218,
   "q_revoice_st_M0": 0.74,
   "scale_db": {
    "M0_Q0": -12.612,
    "M100_Q0": -2.767,
    "M0_Q100": -13.887,
    "M100_Q100": -2.636
   }
  },
  {
   "slot": 5,
   "pole_hz": {
    "M0_Q0": 15730.75,
    "M100_Q0": 9876.84,
    "M0_Q100": 17950.98,
    "M100_Q100": 9713.47
   },
   "pole_r": {
    "M0_Q0": 0.99621,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.984257,
    "M100_Q100": 0.976293
   },
   "zero_hz": {
    "M0_Q0": 15690.2,
    "M100_Q0": 14994.46,
    "M0_Q100": 8255.01,
    "M100_Q100": 10078.13
   },
   "zero_r": {
    "M0_Q0": 0.945821,
    "M100_Q0": 0.96625,
    "M0_Q100": 0.58651,
    "M100_Q100": 0.981282
   },
   "carve_st": {
    "M0_Q0": -0.04,
    "M100_Q0": 7.23,
    "M0_Q100": -13.45,
    "M100_Q100": 0.64
   },
   "unit_zero": {
    "M0_Q0": false,
    "M100_Q0": false,
    "M0_Q100": false,
    "M100_Q100": false
   },
   "travel_st_Q0": -8.06,
   "q_lift_M0": -0.011953,
   "q_revoice_st_M0": 2.29,
   "scale_db": {
    "M0_Q0": -12.612,
    "M100_Q0": -2.767,
    "M0_Q100": -13.887,
    "M100_Q100": -2.636
   }
  },
  {
   "slot": 6,
   "pole_hz": {
    "M0_Q0": 16268.61,
    "M100_Q0": 2247.26,
    "M0_Q100": 10556.37,
    "M100_Q100": 2221.72
   },
   "pole_r": {
    "M0_Q0": 0.847899,
    "M100_Q0": 0.847899,
    "M0_Q100": 0.997924,
    "M100_Q100": 0.987723
   },
   "zero_hz": {
    "M0_Q0": 17960.83,
    "M100_Q0": 9374.54,
    "M0_Q100": 17960.83,
    "M100_Q100": 17960.83
   },
   "zero_r": {
    "M0_Q0": 0.999998,
    "M100_Q0": 0.999998,
    "M0_Q100": 0.999998,
    "M100_Q100": 0.999998
   },
   "carve_st": {
    "M0_Q0": 1.71,
    "M100_Q0": 24.73,
    "M0_Q100": 9.2,
    "M100_Q100": 36.18
   },
   "unit_zero": {
    "M0_Q0": true,
    "M100_Q0": true,
    "M0_Q100": true,
    "M100_Q100": true
   },
   "travel_st_Q0": -34.27,
   "q_lift_M0": 0.150025,
   "q_revoice_st_M0": -7.49,
   "scale_db": {
    "M0_Q0": -12.612,
    "M100_Q0": -2.767,
    "M0_Q100": -13.887,
    "M100_Q100": -2.636
   }
  }
 ]
}
```
