# Forensic comparison: historical recipes vs the decoded E-mu corpus

Corpus: the 33 null-verified P2K architecture recipes, every decoded corner
(132 corners, 1530 conjugate roots; 54 real-pair root entries
participate in whole-response curves but are excluded from F/B root matching, since
historical sources specify conjugate F/Q roots). No decoded Morpheus module cubes
exist in this repository; the Morpheus manual contributes recipes, not corners.
All corpus bandwidths decoded at the 39,062.5 Hz datum. No slot identity or
pole-zero pairing is assumed anywhere; pairs are searched across all sections of
a corner. manual.md values that deviate from their primary papers are encoded as
separate labeled variants.

## 1. Nearest individual root matches (frequency, same kind)

| cents | historical root | corpus state |
|---|---|---|
| 0.1 | fujimura nasalized schwa: pole 1830 Hz B 154 | CruzPusher · M100_Q0 · S3 pole 1829.9 Hz r=0.99853 (B 18 Hz) |
| 0.2 | fant sh (full): pole 7000 Hz B 7000 | MultiQVox · M100_Q0 · S1 pole 7000.7 Hz r=0.92707 (B 942 Hz) |
| 0.2 | martony l after a: pole 3250 Hz | LucifersQ · M0_Q100 · S5 pole 3249.5 Hz r=0.99388 (B 76 Hz) |
| 0.2 | martony l after oe: pole 3250 Hz | LucifersQ · M0_Q100 · S5 pole 3249.5 Hz r=0.99388 (B 76 Hz) |
| 0.4 | martony l after I: pole 3260 Hz | ZoomPeaks · M100_Q0 · S4 pole 3260.7 Hz r=0.98673 (B 166 Hz) |
| 0.5 | martony l after E: zero 2000 Hz | DeepBouche · M100_Q100 · S3 zero 2000.6 Hz r=0.98723 (B 160 Hz) |
| 0.8 | paravowel A: pole 4950 Hz | RadioCraze · M0_Q0 · S1 pole 4947.6 Hz r=0.98772 (B 154 Hz) |
| 0.8 | paravowel O: pole 4950 Hz | RadioCraze · M0_Q0 · S1 pole 4947.6 Hz r=0.98772 (B 154 Hz) |
| 0.8 | paravowel U: pole 4950 Hz | RadioCraze · M0_Q0 · S1 pole 4947.6 Hz r=0.98772 (B 154 Hz) |
| 1.0 | fant sh (full): pole 1500 Hz B 469 | AngelzHairz · M100_Q0 · S6 pole 1500.8 Hz r=0.87505 (B 1660 Hz) |
| 1.1 | martony l after u:: pole 2900 Hz | Eeh-To-Aah · M0_Q0 · S5 pole 2898.2 Hz r=0.98723 (B 160 Hz) |
| 1.2 | fujimura nasalized schwa: pole 5000 Hz | MegaSweepz · M100_Q100 · S5 pole 5003.3 Hz r=0.80050 (B 2767 Hz) |
| 1.3 | paravowel O: pole 2830 Hz | DeepBouche · M100_Q100 · S1 pole 2827.9 Hz r=0.99835 (B 21 Hz) |
| 1.7 | fant s (full): pole 8500 Hz B 6071 | BolandBass · M100_Q100 · S5 pole 8508.3 Hz r=0.93544 (B 830 Hz) |
| 1.7 | martony l after oe: pole 2280 Hz | ZoomPeaks · M0_Q100 · S3 pole 2282.3 Hz r=0.99584 (B 52 Hz) |
| 2.0 | martony l after oe: zero 2060 Hz | TalkingHedz · M100_Q100 · S3 zero 2062.3 Hz r=0.88834 (B 1472 Hz) |
| 2.0 | martony l after u:: pole 2380 Hz | MeatyGizmo · M0_Q100 · S2 pole 2377.3 Hz r=0.35390 (B 12916 Hz) |
| 2.1 | kerkhoff a (bart): pole 1320 Hz B 200 | ZoomPeaks · M100_Q0 · S2 pole 1321.6 Hz r=0.99719 (B 35 Hz) |
| 2.1 | martony l after E: pole 2250 Hz | Ace of Bass · M100_Q0 · S6 pole 2247.3 Hz r=0.84790 (B 2052 Hz) |
| 2.2 | fant s (2p1z): pole 8000 Hz B 2500 | Ace of Bass · M100_Q100 · S4 pole 8010.2 Hz r=0.99584 (B 52 Hz) |
| 2.2 | manual.md s variant: pole 8000 Hz B 2667 | Ace of Bass · M100_Q100 · S4 pole 8010.2 Hz r=0.99584 (B 52 Hz) |
| 2.7 | fujimura nasalized schwa: pole 270 Hz B 46 | EarlyRizer · M0_Q100 · S2 pole 269.6 Hz r=0.99560 (B 55 Hz) |
| 2.7 | martony l after I: pole 2300 Hz | BassBox-303 · M100_Q0 · S1 pole 2296.4 Hz r=0.72899 (B 3930 Hz) |
| 2.7 | fujimura schwa: pole 2300 Hz B 62 | BassBox-303 · M100_Q0 · S1 pole 2296.4 Hz r=0.72899 (B 3930 Hz) |
| 2.9 | fant s (2p1z): zero 4500 Hz B 1800 | FuzziFace · M100_Q0 · S6 zero 4492.4 Hz r=1.00000 (B 0 Hz) |

## 2. Matches in both frequency and damping
(frequency within 30 cents AND bandwidth ratio within 1.5x)

| cents | B ratio | historical root | corpus state |
|---|---|---|---|
| 4.7 | 1.02 | fant sh (full): pole 2200 Hz B 550 | FuzziFace · M0_Q100 · S2 pole 2206.0 Hz r=0.95609 (B 558 Hz) |
| 6.0 | 1.05 | manual.md f variant: zero 2500 Hz B 431 | ZoomPeaks · M0_Q0 · S4 zero 2508.6 Hz r=0.96423 (B 453 Hz) |
| 2.7 | 1.19 | fujimura nasalized schwa: pole 270 Hz B 46 | EarlyRizer · M0_Q100 · S2 pole 269.6 Hz r=0.99560 (B 55 Hz) |
| 4.9 | 1.12 | fant s (full): pole 2500 Hz B 250 | ZoomPeaks · M0_Q0 · S5 pole 2493.0 Hz r=0.98228 (B 222 Hz) |
| 5.2 | 1.18 | fant s (full): zero 2200 Hz B 688 | ZoomPeaks · M0_Q100 · S3 zero 2206.7 Hz r=0.95405 (B 585 Hz) |
| 5.2 | 1.18 | fant s (full): zero 2200 Hz B 688 | ZoomPeaks · M100_Q0 · S3 zero 2206.7 Hz r=0.95405 (B 585 Hz) |
| 5.2 | 1.18 | fant s (full): zero 2200 Hz B 688 | LucifersQ · M0_Q100 · S3 zero 2206.7 Hz r=0.95405 (B 585 Hz) |
| 9.6 | 1.10 | fant f: pole 15000 Hz B 4688 | RogueHertz · M100_Q100 · S6 pole 15083.3 Hz r=0.66162 (B 5136 Hz) |
| 4.1 | 1.30 | fant s (2p1z): pole 8000 Hz B 2500 | Ace of Bass · M100_Q0 · S4 pole 7981.1 Hz r=0.85706 (B 1918 Hz) |
| 9.6 | 1.13 | manual.md f variant: pole 15000 Hz B 4546 | RogueHertz · M100_Q100 · S6 pole 15083.3 Hz r=0.66162 (B 5136 Hz) |
| 6.2 | 1.27 | fant s (full): pole 2500 Hz B 250 | DeadRinger · M100_Q100 · S5 pole 2491.0 Hz r=0.98426 (B 197 Hz) |
| 13.0 | 1.07 | fujimura schwa: pole 2300 Hz B 62 | FreakShifta · M100_Q0 · S3 pole 2282.8 Hz r=0.99535 (B 58 Hz) |
| 4.1 | 1.39 | manual.md s variant: pole 8000 Hz B 2667 | Ace of Bass · M100_Q0 · S4 pole 7981.1 Hz r=0.85706 (B 1918 Hz) |
| 15.2 | 1.06 | kerkhoff o (bot): pole 2200 Hz B 130 | ZoomPeaks · M100_Q0 · S3 pole 2219.4 Hz r=0.99019 (B 123 Hz) |
| 6.0 | 1.38 | fant sh (full): zero 2500 Hz B 625 | ZoomPeaks · M0_Q0 · S4 zero 2508.6 Hz r=0.96423 (B 453 Hz) |
| 14.3 | 1.14 | fant f: pole 15000 Hz B 4688 | RogueHertz · M100_Q0 · S6 pole 14876.5 Hz r=0.64971 (B 5362 Hz) |
| 14.3 | 1.14 | fant f: pole 15000 Hz B 4688 | LucifersQ · M100_Q100 · S6 pole 14876.5 Hz r=0.64971 (B 5362 Hz) |
| 13.4 | 1.20 | fujimura schwa: pole 2300 Hz B 62 | ZoomPeaks · M0_Q100 · S3 pole 2282.3 Hz r=0.99584 (B 52 Hz) |
| 13.4 | 1.20 | fujimura schwa: pole 2300 Hz B 62 | LucifersQ · M0_Q100 · S3 pole 2282.3 Hz r=0.99584 (B 52 Hz) |
| 14.3 | 1.18 | manual.md f variant: pole 15000 Hz B 4546 | RogueHertz · M100_Q0 · S6 pole 14876.5 Hz r=0.64971 (B 5362 Hz) |

## 3. Pole-zero spacing matches, and the bound-pair census

Historical pole-zero pairs (all combinations within a recipe, no pairing assumed)
matched against all cross-section pole-zero pairs of each corner: both endpoints
within 50 cents. These appear in section 4 as 2-of-k co-occurrences.

Bound-pair signature (pole and zero within 200 Hz, any sections): 218
instances in the corpus. Tightest 10:

| dF Hz | corner | pole | zero |
|---|---|---|---|
| 0 | BassBox-303 · M100_Q0 | S4 4170 Hz r=0.8479 | S3 4170 Hz r=0.8479 |
| 0 | EarlyRizer · M0_Q100 | S3 9592 Hz r=0.9962 | S2 9592 Hz r=0.9962 |
| 0 | TB-OrNot-TB · M0_Q0 | S6 357 Hz r=0.9763 | S1 357 Hz r=0.9014 |
| 0 | TB-OrNot-TB · M0_Q100 | S3 357 Hz r=0.9995 | S1 357 Hz r=0.9014 |
| 1 | MegaSweepz · M100_Q0 | S4 5606 Hz r=0.9867 | S1 5605 Hz r=0.9872 |
| 2 | TB-OrNot-TB · M0_Q0 | S2 1934 Hz r=0.9499 | S3 1932 Hz r=0.9396 |
| 3 | EarlyRizer · M0_Q0 | S4 13161 Hz r=0.9014 | S3 13158 Hz r=0.8795 |
| 3 | EarBender · M0_Q0 | S5 14394 Hz r=0.9101 | S3 14391 Hz r=0.9683 |
| 6 | MultiQVox · M0_Q0 | S1 6288 Hz r=0.3539 | S6 6282 Hz r=1.0000 |
| 9 | EarBender · M100_Q0 | S3 2384 Hz r=0.9520 | S3 2374 Hz r=0.9683 |

## 4. Multi-root co-occurrence within a corner
(each historical root matched to a distinct corner root of the same kind, within 50 cents)

**fant sh (full)** (Fant & Martony 1/1960 Fig I-8): best corner matches 3 of 9 roots —
DeadRinger · M100_Q0
- zero 1900 Hz -> S2 1893.3 Hz r=0.72899 (B 3930 Hz), 6.1 cents
- pole 2800 Hz -> S6 2841.0 Hz r=0.98871 (B 141 Hz), 25.2 cents
- pole 2200 Hz -> S5 2154.0 Hz r=0.98327 (B 210 Hz), 36.5 cents

**fant s (full)** (Fant & Martony 1/1960 Fig I-8): best corner matches 3 of 7 roots —
EarlyRizer · M100_Q100
- zero 3000 Hz -> S2 3024.4 Hz r=0.43329 (B 10399 Hz), 14.0 cents
- zero 4700 Hz -> S3 4771.4 Hz r=0.58651 (B 6634 Hz), 26.1 cents
- pole 2500 Hz -> S3 2455.6 Hz r=0.93961 (B 775 Hz), 31.0 cents

**martony l after oe** (Martony & Fant 1/1961 Table I-1): best corner matches 3 of 6 roots —
TalkingHedz · M100_Q100
- zero 2060 Hz -> S3 2062.3 Hz r=0.88834 (B 1472 Hz), 2.0 cents
- pole 2460 Hz -> S4 2410.6 Hz r=0.99918 (B 10 Hz), 35.1 cents
- pole 1475 Hz -> S6 1509.0 Hz r=0.99908 (B 11 Hz), 39.4 cents

**bell vowel I** (Bell et al. JASA 1961 Fig 9): best corner matches 3 of 4 roots —
DeepBouche · M0_Q0
- pole 2580 Hz -> S3 2543.2 Hz r=0.99339 (B 82 Hz), 24.9 cents
- pole 3400 Hz -> S1 3483.8 Hz r=0.96625 (B 427 Hz), 42.1 cents
- pole 1870 Hz -> S4 1916.5 Hz r=0.99339 (B 82 Hz), 42.6 cents

## 5. Whole-response similarity
(mean-removed dB curves at the datum; only recipes whose every root has a published
bandwidth — frequency-only recipes are skipped rather than given invented damping)

| historical recipe | best corner | rms dB | runners-up |
|---|---|---|---|
| klatt nasal pair | FreakShifta · M0_Q0 | 6.63 | BolandBass · M100_Q100 (7.1); Ace of Bass · M100_Q100 (7.7) |
| fant f | BassOMatic · M100_Q100 | 8.91 | FreakShifta · M0_Q0 (9.1); LucifersQ · M100_Q100 (9.3) |
| manual.md s variant | FreakShifta · M0_Q100 | 8.96 | BassOMatic · M100_Q100 (9.2); Ace of Bass · M100_Q100 (10.5) |
| fant s (2p1z) | FreakShifta · M0_Q100 | 9.01 | BassOMatic · M100_Q100 (9.2); Ace of Bass · M100_Q100 (10.6) |
| manual.md f variant | BassOMatic · M100_Q100 | 9.05 | FreakShifta · M0_Q0 (9.2); LucifersQ · M100_Q100 (9.5) |
| klatt glottal zero | BassOMatic · M100_Q100 | 9.76 | LucifersQ · M100_Q100 (10.4); ZoomPeaks · M100_Q100 (10.6) |
| fant sh (full) | MeatyGizmo · M0_Q100 | 9.77 | RadioCraze · M0_Q100 (10.4); BassOMatic · M100_Q100 (10.7) |
| fant s (full) | BassOMatic · M100_Q100 | 10.46 | FreakShifta · M0_Q100 (10.5); MeatyGizmo · M0_Q100 (11.5) |
| kerkhoff a (bart) | EarlyRizer · M100_Q100 | 13.50 | AngelzHairz · M0_Q100 (13.8); EarlyRizer · M100_Q0 (15.6) |
| bell vowel I | EarlyRizer · M100_Q100 | 13.75 | AngelzHairz · M0_Q100 (15.5); EarlyRizer · M100_Q0 (15.8) |
| kerkhoff o (bot) | EarlyRizer · M100_Q100 | 15.14 | EarlyRizer · M100_Q0 (17.6); DreamWeava · M0_Q100 (18.0) |

## 6. Significance against randomized null recipes
(400 null recipes per historical recipe, same root counts and kinds,
frequencies log-uniform 150-16000 Hz, bandwidths log-uniform 30-6000 Hz, seed 1999;
p = fraction of nulls matching the corpus at least as well)

| historical recipe | co-occurrence best k | p(co-occ) | best rms dB | p(rms) |
|---|---|---|---|---|
| fant s (2p1z) | 2 | 0.565 | 9.01 | 0.750 |
| fant f | 1 | 1.000 | 8.91 | 0.765 |
| fant sh (full) | 3 | 0.630 | 9.77 | 0.360 |
| fant s (full) | 3 | 0.365 | 10.46 | 0.552 |
| martony l after a | 2 | 0.963 | — | — |
| martony l after I | 2 | 0.970 | — | — |
| martony l after E | 2 | 0.970 | — | — |
| martony l after u: | 2 | 0.965 | — | — |
| martony l after oe | 3 | 0.253 | — | — |
| fujimura schwa | 2 | 0.895 | — | — |
| fujimura nasalized schwa | 2 | 0.980 | — | — |
| bell vowel I | 3 | 0.052 | 13.75 | 0.713 |
| klatt nasal pair | 1 | 0.990 | 6.63 | 0.405 |
| klatt glottal zero | 1 | 0.785 | 9.76 | 0.290 |
| kerkhoff o (bot) | 2 | 0.762 | 15.14 | 0.745 |
| kerkhoff a (bart) | 2 | 0.795 | 13.50 | 0.682 |
| paravowel A | 2 | 0.915 | — | — |
| paravowel E | 2 | 0.902 | — | — |
| paravowel O | 2 | 0.907 | — | — |
| paravowel U | 2 | 0.917 | — | — |
| manual.md f variant | 1 | 1.000 | 9.05 | 0.748 |
| manual.md s variant | 2 | 0.545 | 8.96 | 0.757 |

## 7. Ranked verdicts

No historical recipe matches the corpus better than randomized null recipes at
p < 0.05 on either metric. The corpus does not carry these recipes' numbers.

