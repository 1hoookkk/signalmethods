# Primary Cleanroom Source Catalog & Manifest

This manifest documents all 18 authoritative, cleanroom primary sources (patents, physics papers, formant corpora, PCA foundations, and textbook chapters) required for E-mu Z-Plane / cascade filter authoring.

---

## 1. Digital Filter & Cascade Biquad Foundations (Textbook DSP)

1. **Julius O. Smith III — *Two-Pole Filter Sections & Resonator Bandwidth***
   - **URL**: [https://ccrma.stanford.edu/~jos/filters/Two_Pole.html](https://ccrma.stanford.edu/~jos/filters/Two_Pole.html)
   - **Bandwidth Mapping**: [https://ccrma.stanford.edu/~jos/filters/Resonator_Bandwidth_Pole_Radius.html](https://ccrma.stanford.edu/~jos/filters/Resonator_Bandwidth_Pole_Radius.html)
   - **Role**: Mathematical derivation connecting formant bandwidth $B$ (Hz) to pole radius $r = e^{-\pi B / F_s}$ and $Q \approx \frac{\pi f_0}{F_s (1 - r)}$.

2. **Julius O. Smith III — *Bilinear Transformation & Frequency Warping***
   - **URL**: [https://ccrma.stanford.edu/~jos/pasp/Bilinear_Transformation.html](https://ccrma.stanford.edu/~jos/pasp/Bilinear_Transformation.html)
   - **Role**: Mathematical de-warping between continuous acoustic frequencies ($\Omega$) and discrete-time digital frequencies ($\omega$) at $39,062.5\text{ Hz}$.

3. **Julius O. Smith III — *Serial Acoustic Tube Model of the Vocal Tract***
   - **URL**: [https://ccrma.stanford.edu/~jos/pasp/Vocal_Tract.html](https://ccrma.stanford.edu/~jos/pasp/Vocal_Tract.html)
   - **Role**: Proves the vocal tract is physically a serial all-pole cascade with explicit side-branch zeros.

4. **J. L. Flanagan (1972) — *Speech Analysis, Synthesis and Perception*** (Chapter 5)
   - **URL**: [https://archive.org/details/speech-analysis-synthesis-perception-flanagan-1972](https://archive.org/details/speech-analysis-synthesis-perception-flanagan-1972)
   - **Role**: Standard textbook treatment of acoustic tube wave equations, transfer function derivations, and glottal impedance.

---

## 2. Serial Analysis-by-Synthesis & Acoustic Literature

5. **Bell, Fujisaki, Heinz, Stevens, & House (1961)**
   - **Title**: *Reduction of Speech Spectra by Analysis-by-Synthesis Techniques* (JASA 33(12), pp. 1725–1736)
   - **File**: `recipes/sources/Bell_1961_Reduction_of_Speech_Spectra_by_Analysis-by-Synthesis.pdf`
   - **Role**: Taylor series parameter sensitivity derivatives ($\frac{\partial \ln |T|}{\partial F_k}, \frac{\partial \ln |T|}{\partial B_k}$) and higher-pole correction network.

6. **Fujimura & Lindqvist (1964)**
   - **Title**: *The Sinusoidal-Wave Response of the Vocal Tract* (STL-QPSR 5(1), pp. 1–10)
   - **PDF Link**: [https://www.speech.kth.se/prod/publications/files/qpsr/1964/1964_5_1_001-010.pdf](https://www.speech.kth.se/prod/publications/files/qpsr/1964/1964_5_1_001-010.pdf)
   - **Role**: Measured sinusoidal pole-zero coordinates for nasalized vowels and lateral [l] mouth-cavity zeros ($Z_1$).

7. **Zhu (1996)**
   - **Title**: *Analysis-by-synthesis vocal tract modeling* (ICSLP 1996)
   - **Role**: Discrete-time pole-zero extraction algorithms under nasal/lateral acoustic coupling.

8. **Kerkhoff (1993)**
   - **Title**: *Formant and bandwidth measurements of Dutch vowels* (Eurospeech 1993)
   - **Role**: Steady-state European vowel formant frequencies and bandwidths.

---

## 3. Empirical Formant Ground Truth Corpora

9. **Hillenbrand, Getty, Clark, & Wheeler (1995)**
   - **Title**: *Acoustic characteristics of American English vowels* (JASA 97(5), pp. 3099–3111)
   - **PDF Link**: [https://homepages.wmich.edu/~hillenbr/Papers/HillenbrandEtAl1995.pdf](https://homepages.wmich.edu/~hillenbr/Papers/HillenbrandEtAl1995.pdf)
   - **Raw Data Archive**: [https://homepages.wmich.edu/~hillenbr/voweldata.html](https://homepages.wmich.edu/~hillenbr/voweldata.html)
   - **Role**: Multi-speaker formant coordinates ($F_1 \dots F_4$) measured at 20%, 50%, and 80% duration time-slices.

10. **Peterson & Barney (1952)**
    - **Title**: *Control Methods Used in a Study of the Vowels* (JASA 24(2), pp. 175–184)
    - **PDF Link**: [https://www.seas.upenn.edu/~cis5210/lectures/vowels_PB.pdf](https://www.seas.upenn.edu/~cis5210/lectures/vowels_PB.pdf)
    - **Role**: 76-speaker steady-state vowel averages ($F_1, F_2, F_3$).

11. **Kent & Vorperian (2018)**
    - **Title**: *Vowel Formant Acoustics: Lifespan Trends and Reference Values* (nihms971733)
    - **Role**: Comprehensive lifespan reference data across age and gender.

12. **Dennis Klatt (1980) — CASCADE SECTION ONLY**
    - **Title**: *Software for a cascade/parallel formant synthesizer* (JASA 67(3), pp. 971–995)
    - **PDF Link**: [https://www.cs.cmu.edu/~dod/papers/klatt80.pdf](https://www.cs.cmu.edu/~dod/papers/klatt80.pdf)
    - **Role**: Cascade difference equations, $B_n(F_n)$ empirical bandwidth formula, and nominal nasal pole-zero pair ($F_{nz}, F_{np} = 250\text{ Hz}$). *(Ignore parallel branch $A_1 \dots A_6$ amplitude tables).*

---

## 4. E-mu & Dave Rossum Z-Plane Patents & Manuals

13. **US Patent 5,170,369 (Dave Rossum / E-mu Systems, 1992)**
    - **Title**: *Interpolating Filter for Electronic Music Synthesizer*
    - **Link**: [https://patents.google.com/patent/US5170369A/en](https://patents.google.com/patent/US5170369A/en)
    - **Role**: Log-polar display mapping ($\theta', R'$), 3D corner addressing ($m \mid q \ll 1 \mid z \ll 2$), and word quantization bounds.

14. **US Patent 5,952,599 (Dave Rossum / E-mu Systems, 1999)**
    - **Title**: *Parameter-interpolated multi-dimensional filter*
    - **Link**: [https://patents.google.com/patent/US5952599A/en](https://patents.google.com/patent/US5952599A/en)
    - **Role**: Parameter word packing and multi-axis morphing mechanics.

15. **US Patent 10,514,883 (Dave Rossum / Rossum Electro-Music, 2019)**
    - **Title**: *Parameter Interpolation and Exponentiation in Cascade Filters*
    - **Link**: [https://patents.google.com/patent/US10514883B2/en](https://patents.google.com/patent/US10514883B2/en)
    - **Role**: 14th-order (7-stage) cascade structure, log-space parameter interpolation followed by exponentiation decode, DC stabilization accumulation at section 7, and soft clipping.

16. **E-mu Morpheus Operation Manual (1993)**
    - **PDF Link**: [https://cdn.shopify.com/s/files/1/0277/4548/4865/files/Morpheus_manual_170206.pdf?v=1785520097](https://cdn.shopify.com/s/files/1/0277/4548/4865/files/Morpheus_manual_170206.pdf?v=1785520097)
    - **Role**: Official 289-cube reference taxonomy, character sonic intents, and tracking laws.

---

## 5. Martens PCA & Spectral Resynthesis Lineage

17. **William L. Martens (1985 ICMC)**
    - **Title**: *Palette: An Interactive System for Sound Synthesis by Perceptual Distance*
    - **Link**: [https://quod.lib.umich.edu/i/icmc/bbp2372.1985.011/1](https://quod.lib.umich.edu/i/icmc/bbp2372.1985.011/1)
    - **Role**: Perceptual bisection and equal perceptual distance simplex fitting.

18. **William L. Martens (1987 ICMC)**
    - **Title**: *Principal Components Analysis and Resynthesis of Spectral Envelopes*
    - **Link**: [https://quod.lib.umich.edu/i/icmc/bbp2372.1987.039/1](https://quod.lib.umich.edu/i/icmc/bbp2372.1987.039/1)
    - **Role**: PCA eigensystem over spectral curves, score space trajectory tracking, and inverse PCA resynthesis.

19. **Burred, Röbel, & Rodet (2006 DAFx / LSAS)**
    - **Title**: *Polyphonic Timbre Analysis by PCA of Spectral Envelopes*
    - **Role**: Standard 1024-point log-frequency grid ($40\text{ Hz} \to 16\text{ kHz}$) for spectral envelope normalization prior to PCA.
