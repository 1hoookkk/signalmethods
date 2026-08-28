# Measurement Quartet: Primary Acoustic & Synthesis Sources

This cleanroom reference records the four foundational acoustic and analysis-by-synthesis measurement sources used for cascade pole-zero filter reconstruction.

---

## 1. Bell et al. (1961) — Analysis-by-Synthesis
- **Citation**: Bell, C. G., Fujisaki, H., Heinz, J. M., Stevens, K. N., & House, A. S. (1961). *Reduction of Speech Spectra by Analysis-by-Synthesis Techniques*. Journal of the Acoustical Society of America, 33(12), 1725–1736.
- **Repository Location**: `recipes/sources/Bell_1961_Reduction_of_Speech_Spectra_by_Analysis-by-Synthesis.pdf`
- **Core Contribution**: Mathematical closed-loop spectrum matching; Taylor series parameter-error minimization ($\Delta F_n, \Delta B_n$); discrete pole-zero cascade decomposition of vocal tract transfer functions; baseline bandwidth tables ($B_1 \dots B_4$).

---

## 2. Fant (1960) — Acoustic Theory of Speech Production
- **Citation**: Fant, G. (1960). *Acoustic Theory of Speech Production: With Calculations based on X-Ray Studies of Russian Articulations*. Mouton & Co., 's-Gravenhage / Walter de Gruyter.
- **Core Contribution**: Formant frequency distributions, bound-pair pole-zero pairs for fricatives ([s], [ʃ], [f]), nasal coupling anti-resonances, and the source-filter model ($S(s) = G(s) \cdot H(s) \cdot R(s)$).

---

## 3. Fujimura & Lindqvist (1964) / Fujimura (1962) — Vocal Tract Sinusoidal Response & Nasalization
- **Citations**: 
  - Fujimura, O. (1962). *Analysis of Nasal Consonants*. J. Acoust. Soc. Am., 34(12), 1865–1875.
  - Fujimura, O., & Lindqvist, J. (1964). *The Sinusoidal-Wave Response of the Vocal Tract*. STL-QPSR 5(1), 1–10.
- **Core Contribution**: Direct measurement of vocal tract transfer functions via external sinusoidal acoustic excitation; empirical pole-zero pairs for nasalized vowels and laterals ([l]); mouth-cavity zero coordinates ($Z_1$).

---

## 4. Klatt (1980) — Cascade Formant Synthesizer Architecture
- **Citation**: Klatt, D. H. (1980). *Software for a cascade/parallel formant synthesizer*. Journal of the Acoustical Society of America, 67(3), 971–995.
- **Core Contribution**: Cascade second-order section digital implementation; empirical vowel bandwidth approximations as functions of formant frequency ($B(F)$); glottal source spectral tilt ($-12\text{ dB/octave}$) and lip radiation differentiation ($+6\text{ dB/octave}$).
