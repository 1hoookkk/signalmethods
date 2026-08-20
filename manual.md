# Complex Serial Pole-Zero Speech Filter Design Manual

A cleanroom specification and engineering reference for modeling vocal tract transfer functions and speech phonemes as serial cascades of second-order pole-zero sections (Z-plane biquads).

---

## 1. Acoustic Foundations & Source-Filter Formulation

Speech production is modeled as a linear source-filter system where the acoustic output spectrum $P(s)$ is the product of the glottal/excitation source $S(s)$, the vocal tract transfer function $T(s)$, and the lip radiation characteristic $R(s)$:

$$P(s) = S(s) \cdot T(s) \cdot R(s)$$

### 1.1 Source & Radiation Spectral Tilt
* **Glottal Source $S(s)$**: Characterized by a $-12\text{ dB/octave}$ spectral roll-off (two real poles in the source excitation function).
* **Lip Radiation $R(s)$**: Modeled as pressure differentiation at the open lips ($\propto s$), producing a $+6\text{ dB/octave}$ high-frequency lift.
* **Effective Excitation Spectrum**: The composite source-radiation transfer function $S(s) \cdot R(s)$ exhibits a net $-6\text{ dB/octave}$ slope. When matching acoustic measurements, target spectra must be normalized by this $-6\text{ dB/octave}$ baseline to isolate the pure vocal tract filter function $T(s)$.
* **Frication/Noise Sources**:
  * Free noise ($A_C$): Flat spectrum ($0\text{ dB/octave}$).
  * Integrated noise ($A_H$): $-6\text{ dB/octave}$ slope with high-pass attenuation below $500\text{ Hz}$ to avoid low-frequency rumble.

---

## 2. Vocal Tract System Function $T(s)$

For non-nasalized vowels, the vocal tract acts as an all-pole acoustic tube resonator. For nasals, laterals, and fricative constrictions, acoustic branching and parallel pathways introduce anti-resonances (zeros):

$$T(s) = T_0 \cdot \prod_{k=1}^{N_p} \frac{s_{pk} s_{pk}^*}{(s - s_{pk})(s - s_{pk}^*)} \cdot \prod_{m=1}^{N_z} \frac{(s - s_{zm})(s - s_{zm}^*)}{s_{zm} s_{zm}^*}$$

Where:
* Complex conjugate pole: $s_{pk}, s_{pk}^* = -\pi B_k \pm j 2\pi F_k$
* Complex conjugate zero: $s_{zm}, s_{zm}^* = -\pi B_{0m} \pm j 2\pi F_{0m}$
* Formant frequency: $F_k\text{ (Hz)}$
* Formant 3-dB bandwidth: $B_k\text{ (Hz)}$
* Quality factor: $Q_k = \frac{F_k}{B_k}$

---

## 3. Standard Acoustic Parameters from Primary Literature

### 3.1 Vowel Formants & Baseline Bandwidths (Fant 1960, Bell et al. 1961, Klatt 1980)

Standard empirical bandwidth approximations for neutral male vocal tracts:
* $B_1 \approx 50\text{ Hz}$ ($F_1 < 800\text{ Hz}$)
* $B_2 \approx 60\text{ Hz}$ to $90\text{ Hz}$
* $B_3 \approx 100\text{ Hz}$ to $130\text{ Hz}$
* $B_4 \approx 180\text{ Hz}$ to $250\text{ Hz}$

| Vowel (IPA) | Label | $F_1\text{ (Hz)}$ | $B_1\text{ (Hz)}$ | $F_2\text{ (Hz)}$ | $B_2\text{ (Hz)}$ | $F_3\text{ (Hz)}$ | $B_3\text{ (Hz)}$ | $F_4\text{ (Hz)}$ | $B_4\text{ (Hz)}$ |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **/i/** | fleece | 270 | 50 | 2290 | 70 | 3010 | 110 | 3500 | 180 |
| **/e/** | dress | 530 | 60 | 1840 | 90 | 2480 | 120 | 3600 | 200 |
| **/æ/** | trap | 660 | 70 | 1720 | 100 | 2410 | 140 | 3600 | 220 |
| **/ɑ/** | father | 730 | 80 | 1090 | 90 | 2440 | 130 | 3500 | 200 |
| **/ɔ/** | thought| 570 | 80 | 840 | 90 | 2410 | 130 | 3500 | 200 |
| **/u/** | goose | 300 | 50 | 870 | 80 | 2240 | 110 | 3500 | 200 |
| **/ɜː/** | nurse | 490 | 60 | 1350 | 80 | 1690 | 120 | 3500 | 200 |

### 3.2 Consonants, Fricatives & Bound-Pair Formulations (Fant 1960, Fujimura 1964)

Constrictions create localized acoustic cavities, characterized by high-Q primary poles and paired zeros ("bound pairs") that suppress out-of-band energy:

| Phoneme | Component | Frequency | Bandwidth / Q | Acoustic Role |
| :--- | :--- | :--- | :--- | :--- |
| **Fricative [s]** | Main Pole | $5800\text{ Hz}$ | $B = 580\text{ Hz}$ ($Q = 10.0$) | Primary anterior cavity resonance peak. |
| | High Pole | $8000\text{ Hz}$ | $B = 2667\text{ Hz}$ ($Q \approx 3.0$) | High-frequency shelf / presence build-up. |
| | High-pass Zero | $4500\text{ Hz}$ | $B = 1800\text{ Hz}$ ($Q = 2.5$) | Suppresses low/mid transfer below the main peak. |
| **Fricative [ʃ]** | Main Pole | $2500\text{–}3000\text{ Hz}$ | $B \approx 300\text{ Hz}$ ($Q \approx 9.0$) | Palato-alveolar cavity resonance. |
| | High-pass Zero | $2000\text{ Hz}$ | $B \approx 1000\text{ Hz}$ ($Q \approx 2.0$) | ~2 kHz lower than [s] zero. |
| **Fricative [f]** | Bound Pole | $2700\text{ Hz}$ | $B = 465\text{ Hz}$ ($Q = 5.8$) | Labiodental boundary resonance. |
| | Bound Zero | $2500\text{ Hz}$ | $B = 431\text{ Hz}$ ($Q = 5.8$) | Closely coupled zero (~200 Hz below pole). |
| | Auxiliary Pole| $15000\text{ Hz}$ | $B = 4545\text{ Hz}$ ($Q = 3.3$) | Damped high-frequency open termination. |
| **Lateral [l] (a)**| Pole $F_1, F_2$| $375, 1250\text{ Hz}$ | $B_1 = 50, B_2 = 60\text{ Hz}$ | Primary vowel-like formants. |
| | Zero $Z_1$ | $2700\text{ Hz}$ | — | Mouth-cavity anti-resonance. |
| **Lateral [l] (i)**| Pole $F_1, F_2$| $235, 1600\text{ Hz}$ | $B_1 = 50, B_2 = 60\text{ Hz}$ | Formants for /i/-allophone context. |
| | Zero $Z_1$ | $1900\text{ Hz}$ | — | Mouth-cavity anti-resonance. |
| **Nasalized [ɜ]** | Pole $P_1, P_2$| $517, 1230\text{ Hz}$ | $B_1 = 56, B_2 = 55\text{ Hz}$ | Nasal tract coupled resonances. |
| | Zero $Z_1$ | $312\text{ Hz}$ | $B = 60\text{ Hz}$ | Low-frequency pharyngeal-nasal anti-resonance. |

---

## 4. Analysis-by-Synthesis Loop (Bell et al. 1961)

The closed-loop spectrum reconstruction algorithm minimizes the squared error between measured speech log-magnitude spectrum $M(\omega)$ and synthesized transfer function $T(\omega; \mathbf{\theta})$ where $\mathbf{\theta} = [F_1, B_1, \dots, F_k, B_k, F_{0m}, B_{0m}]$:

$$\epsilon^2 = \int_{\omega_a}^{\omega_b} W(\omega) \left[ \ln |M(\omega)| - \ln |T(\omega; \mathbf{\theta})| \right]^2 d\omega$$

### 4.1 First-Order Parameter Sensitivity Expansion
For small parameter increments, the transfer function log-magnitude delta is linearized via Taylor series:

$$\Delta \ln |T(\omega)| \approx \sum_{k} \frac{\partial \ln |T(\omega)|}{\partial F_k} \Delta F_k + \sum_{k} \frac{\partial \ln |T(\omega)|}{\partial B_k} \Delta B_k + \sum_{m} \frac{\partial \ln |T(\omega)|}{\partial F_{0m}} \Delta F_{0m} + \sum_{m} \frac{\partial \ln |T(\omega)|}{\partial B_{0m}} \Delta B_{0m}$$

* **Pole frequency error gradient**: $\frac{\partial \ln |T(\omega)|}{\partial F_k}$ creates an asymmetric dispersion curve crossing zero at $\omega = 2\pi F_k$.
* **Bandwidth error gradient**: $\frac{\partial \ln |T(\omega)|}{\partial B_k}$ creates a symmetric bell-shaped curve centered at $\omega = 2\pi F_k$.

Solving this over-determined system via orthogonal least-squares yields the optimal iterative parameter adjustments $(\Delta F, \Delta B)$.

---

## 5. Discrete-Time Realization in the Z-Domain

To implement continuous vocal-tract system functions on discrete-time DSP hardware (e.g., E-mu Z-plane / Trench Authoring engine):

### 5.1 Pole/Zero Coordinate Mapping ($s \to z$)
Given sampling rate $F_s$ (Legacy datum: $39,062.5\text{ Hz}$; Authoring default: $44,100\text{ Hz}$):

* **Pole radius $r$**:
  $$r = e^{-\pi B / F_s}$$
* **Log-polar radius representation $R'$**:
  $$R' = 20 \log_{10}\left(\frac{1}{1 - r}\right)\text{ dB}$$
* **Pole digital angular frequency $\theta$**:
  $$\theta = \frac{2\pi F}{F_s}$$
* **Log-polar angle representation $\theta'$**:
  $$\theta' = \frac{\pi}{10} \left(10 + \log_2 \frac{\theta}{\pi}\right)$$

### 5.2 Second-Order Section (Biquad) Cascade
The transfer function is factored into a serial cascade of $N$ second-order sections:

$$H(z) = g \cdot \prod_{i=1}^{N} \frac{1 - 2 r_{zi} \cos(\theta_{zi}) z^{-1} + r_{zi}^2 z^{-2}}{1 - 2 r_{pi} \cos(\theta_{pi}) z^{-1} + r_{pi}^2 z^{-2}}$$

### 5.3 Authoring Constraints & Invariants
1. **Stability Ceiling**: Pole radii must satisfy $r < 1.0$ (typically clamped to $r_{\max} \approx 0.998$ to avoid limit cycles and overflow).
2. **Traveling Nulls**: Zero radii are allowed to reach exactly $r = 1.0$ on the unit circle to produce infinite notches ($-\infty\text{ dB}$).
3. **Geometric Level Control**: Section gain shaping is avoided; overall passband level is established through pole tightness ($r \to 1.0$) against balancing zeroes, with one aggregate cascade gain $g$ distributed across active stages.
