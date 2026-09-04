# P2K prediction tests

Corpus: 33 exact factory `.bin` bodies, 4 corners, 6 sections, 5 little-endian u16 words per section. Decode datum: 39062.5 Hz. Corner order: C0=M0/Q0, C1=M100/Q0, C2=M0/Q100, C3=M100/Q100.

## Prediction 1 — slot identity across rows

- Minimum-cost log-frequency bijection is identity for 98/144 sections (68.1%) across 24 eligible bodies.
- The entire optimal permutation is identity in 13/24 eligible bodies.
- Independent nearest-neighbour identity rate: 47.9%; allowing exact-frequency ties, the own slot is a nearest match for 47.9%.
- Random-permutation baseline: expected 16.7%; 100,000-trial Monte Carlo mean 16.7%, 95% interval 10.4–23.6%.
- Threshold tested: >=80%.

## Prediction 2 — sharpening as a body-level operation

- 23/33 bodies have >=4/5 S1–S5 `delta ln(r)` values with the same exact sign: 18 positive, 5 negative, 0 zero-dominant. There are 158/165 valid conjugate stage pairs.
- Pooled within-body variance: 0.018675404.
- Between-body variance of body means: 0.0034536721 (between/within=0.185).

## Prediction 3 — coherent frame translation

- C1-C0 pole motion is unanimous among at least four nonzero live shifts in 8/33 bodies; >=5/6 same nonzero direction in 22/33, and >=5/6 the same exact sign (including zero) in 22/33.
- Same-section zero-shift on pole-shift regression (179 live conjugate pairs): slope=0.1508, intercept=-0.3672 octaves, Pearson r=0.2378. Through-origin slope=0.1592.
- Prediction tested: slope about 0.6 and r>0.5.

## Prediction 5 — gain-word level compensation

- Raw fifth words for every corner/section are retained in the JSON. DC corner range is <=1 dB in 28/33 bodies; median range=0.008 dB, max=36.126 dB.
- As a spectral-level sensitivity check, linear-frequency white-noise RMS range is <=1 dB in 0/33 bodies; median range=16.236 dB.
- Across both row transitions C2-C0 and C3-C1 (383 section pairs), raw gain-word delta vs `delta ln(r)` Pearson r=-0.0725; decoded `delta ln(scale)` vs `delta ln(r)` r=-0.0727.
- Restricting to the 269 actual radius increases, those correlations are 0.1349 raw and 0.1304 decoded.
