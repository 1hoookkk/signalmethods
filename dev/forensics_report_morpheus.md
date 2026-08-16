# Forensic comparison: historical recipes vs the decoded Morpheus cube corpus

Corpus: all 289 decoded Morpheus module cubes (ref/morpheus/cubes_decoded.json),
1983 non-null corners, 12323 poles and 8332 zeros
(roots with r > 1e-06 in non-identity stages), frequencies at the 39,062.5 Hz
datum. Same recipes, metrics, thresholds, null model, and seed as the P2K run
(tools/forensic_compare.py): co-occurrence within 50 cents greedy-matched per
corner; whole-response rms on mean-removed dB curves, 512-point log grid
40-16,000 Hz; 400 null recipes per historical recipe, frequencies log-uniform
150-16,000 Hz, bandwidths log-uniform 30-6,000 Hz, seed 1999. The two ParaVowel
recipes are the module manual's own p36 peak lists and act as positive controls:
AEParaVowel and AOParaVowel are cubes in this corpus.

## Observed matches and significance

| recipe | best k / n | corner | p(co-occ) | best rms dB | corner | p(rms) |
|---|---|---|---|---|---|---|
| fant s (2p1z) | 2/3 | InHarMetric · c3 | 0.993 | 2.48 | Oh Shaper · c6 | 0.117 |
| fant f | 2/3 | 0>Shp2 · c6 | 0.995 | 0.52 | Separator · c2 | 0.003 |
| fant sh (full) | 3/9 | Harmonix.4 · c5 | 0.990 | 3.59 | Feedback · c3 | 0.013 |
| fant s (full) | 3/7 | Clr>Oboe · c6 | 0.912 | 3.23 | HeeghCube · c4 | 0.020 |
| martony l after a | 3/6 | Flange6R.4 · c5 | 0.830 | — | — | — |
| martony l after I | 5/6 | BrsSwell2.4 · c2 | 0.000 | — | — | — |
| martony l after E | 3/6 | GentleRez.4 · c6 | 0.800 | — | — | — |
| martony l after u: | 3/6 | MovingPick2 · c4 | 0.840 | — | — | — |
| martony l after oe | 3/6 | MovingPick2 · c0 | 0.805 | — | — | — |
| fujimura schwa | 3/5 | Tam · c5 | 0.657 | — | — | — |
| fujimura nasalized schwa | 3/7 | PowerSweeps · c2 | 0.932 | — | — | — |
| bell vowel I | 3/4 | MellowPeaks · c0 | 0.357 | 5.90 | LowPassPlus · c3 | 0.380 |
| klatt nasal pair | 2/2 | GuitXpress · c5 | 0.757 | 0.05 | Clear Water · c0 | 0.000 |
| klatt glottal zero | 1/1 | Flange2.4 · c4 | 1.000 | 1.35 | Spectra · c2 | 0.165 |
| kerkhoff o (bot) | 3/4 | Notcher2.4 · c5 | 0.400 | 6.70 | 6 Poles · c2 | 0.505 |
| kerkhoff a (bart) | 3/4 | Quartet.4 · c0 | 0.365 | 6.18 | Be-Ye.4 · c5 | 0.460 |
| paravowel A | 5/5 | Be-Ye.4 · c5 | 0.000 | — | — | — |
| paravowel E | 4/5 | AEParLPVow · c4 | 0.025 | — | — | — |
| paravowel O | 4/5 | AEParaVowel · c4 | 0.030 | — | — | — |
| paravowel U | 5/5 | AOParaVowel · c5 | 0.000 | — | — | — |
| manual.md f variant | 2/3 | 0>Shp2 · c6 | 0.983 | 1.07 | Separator · c2 | 0.013 |
| manual.md s variant | 2/3 | InHarMetric · c3 | 0.983 | 2.43 | Oh Shaper · c6 | 0.113 |

## Detail of the strongest co-occurrences

**martony l after I** (Martony & Fant 1/1961 Table I-1) — 5 of 6 roots at BrsSwell2.4 · c2 (p = 0.000):
- pole 3260 Hz -> S6 3260.4 Hz r=0.99164 (B 104 Hz), 0.2 cents
- pole 2300 Hz -> S4 2288.2 Hz r=0.99411 (B 73 Hz), 8.9 cents
- zero 1900 Hz -> S2 1887.7 Hz r=0.98133 (B 234 Hz), 11.3 cents
- pole 2770 Hz -> S5 2726.3 Hz r=0.99313 (B 86 Hz), 27.5 cents
- pole 1600 Hz -> S2 1563.4 Hz r=0.99609 (B 49 Hz), 40.0 cents

**paravowel A** (Morpheus module manual p36 (peaks only)) — 5 of 5 roots at Be-Ye.4 · c5 (p = 0.000):
- pole 800 Hz -> S1 800.8 Hz r=0.94580 (B 693 Hz), 1.7 cents
- pole 3500 Hz -> S4 3470.2 Hz r=0.97327 (B 337 Hz), 14.8 cents
- pole 1150 Hz -> S2 1134.6 Hz r=0.96632 (B 426 Hz), 23.4 cents
- pole 2800 Hz -> S3 2745.4 Hz r=0.96924 (B 389 Hz), 34.1 cents
- pole 4950 Hz -> S5 4843.5 Hz r=0.97229 (B 349 Hz), 37.7 cents

**paravowel U** (Morpheus module manual p36) — 5 of 5 roots at AOParaVowel · c5 (p = 0.000):
- pole 4950 Hz -> S6 4956.7 Hz r=0.99066 (B 117 Hz), 2.3 cents
- pole 325 Hz -> S2 321.7 Hz r=0.99896 (B 13 Hz), 17.6 cents
- pole 3500 Hz -> S5 3432.0 Hz r=0.99512 (B 61 Hz), 33.9 cents
- pole 700 Hz -> S3 686.4 Hz r=0.99768 (B 29 Hz), 34.1 cents
- pole 2530 Hz -> S4 2478.4 Hz r=0.99609 (B 49 Hz), 35.7 cents

**paravowel E** (Morpheus module manual p36) — 4 of 5 roots at AEParLPVow · c4 (p = 0.025):
- pole 2700 Hz -> S4 2669.1 Hz r=0.99066 (B 117 Hz), 19.9 cents
- pole 4900 Hz -> S6 4843.5 Hz r=0.97730 (B 286 Hz), 20.1 cents
- pole 3300 Hz -> S5 3260.4 Hz r=0.98871 (B 141 Hz), 20.9 cents
- pole 1600 Hz -> S3 1563.4 Hz r=0.99264 (B 92 Hz), 40.0 cents

**paravowel O** (Morpheus module manual p36) — 4 of 5 roots at AEParaVowel · c4 (p = 0.030):
- pole 450 Hz -> S7 445.7 Hz r=0.99792 (B 26 Hz), 16.7 cents
- pole 3500 Hz -> S4 3451.1 Hz r=0.98724 (B 160 Hz), 24.4 cents
- pole 2830 Hz -> S5 2783.5 Hz r=0.98969 (B 129 Hz), 28.7 cents
- pole 4950 Hz -> S3 4843.5 Hz r=0.97730 (B 286 Hz), 37.7 cents

## Matched-structure (jitter) null

The log-uniform null under-represents evenly-spaced ladder recipes, which the
corpus is full of. Second null: each root of the real recipe multiplied by
2^(U(−300,300)/1200) — the recipe's internal spacing is preserved, only its
placement is randomized. 2000 trials, seed 777. This asks whether the exact
frequencies matter or only the shape. (Refined log-uniform p for martony l
after I: 13/5000 = 0.0026.)

| recipe | obs k | p_jitter(k) | obs rms | p_jitter(rms) |
|---|---|---|---|---|
| martony l after I | 5 | 0.006 | — | — |
| paravowel A | 5 | 0.002 | — | — |
| paravowel U | 5 | 0.000 | — | — |
| paravowel E | 4 | 0.058 | — | — |
| paravowel O | 4 | 0.056 | — | — |
| fant f | 2 | 0.997 | 0.52 | 0.209 |
| fant sh (full) | 3 | 1.000 | 3.59 | 0.441 |
| fant s (full) | 3 | 1.000 | 3.23 | 0.571 |
| bell vowel I | 3 | 0.539 | 5.90 | 0.295 |

## Verdicts

- **Positive controls confirmed.** The module manual's own p36 paravowel
  peak lists are in the shipped cube data: paravowel A 5/5 and U 5/5 at
  p ≈ 0.000 under both nulls (A's best match is Be-Ye.4 c5, not a ParaVowel
  cube — but this is a within-50-cents frequency match, not pose reuse:
  Be-Ye.4 is a distinct all-pole five-pole vowel with no zeros, while the
  paravowel A pole-set proper recurs bit-identically across six cubes,
  AEParLPVow/AEParaVowel/AOParaVowel/AUParaVow.4/SoftEOAE/CO>FlngT). E and O reach 4/5. The metric has
  power to find planted numbers.
- **The Fant rms cluster is retracted.** fant f / sh / s looked significant
  under the log-uniform null (p = 0.003–0.020) but die under the jitter null
  (p = 0.21–0.57): mild consonant-shaped relief curves fit this corpus
  wherever you slide them. Shape, not numbers.
- **klatt nasal pair rms p=0.000 is degenerate**: the recipe is a coincident
  pole-zero pair, nearly flat; the corpus contains near-flat corners; the
  null randomizes pole and zero independently and can never be flat. Artifact
  of the null model, retracted.
- **One genuine tail event: martony l after I** — 5 of 6 roots (four poles
  0.2–40 cents, plus the documented 1900 Hz zero at 11.3 cents) at
  BrsSwell2.4 c2, p = 0.0026 log-uniform, p = 0.006 shape-preserving. The
  matched corner is a log-spaced pole/zero ladder (ratio ≈ 1.19–1.22,
  1563→3890 Hz), a brass-formant idiom unique to this cube, not an obvious
  vocal transplant. With ~16 genuinely historical recipes tested, the
  family-wise chance of one recipe reaching p ≤ 0.006 is roughly 9%. An
  intriguing lead, not a conviction.
- Everything else: the cube corpus does not carry the historical recipes'
  numbers, only their methods.

