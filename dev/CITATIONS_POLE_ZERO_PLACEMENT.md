# Citations — serial pole-zero placement literature

Extraction only. Verbatim quotes, no reasoning. Source directory:
`C:\Users\hooki\Downloads\qcinst_extracted\`

`kerkhoff93_eurospeech.pdf` **is an image scan** — only the ISCA cover stamp has
a text layer. Its quotes were transcribed from 300 dpi page images; PDF pages
1–4 are printed pages 1705–1708. Two other files also lack a text layer on the
body page and were transcribed the same way: `1961_2_1_001-002.pdf` p.3 and
`1962_3_2_020-021.pdf` p.3.

File identification:
- `1961_2_1_001-002.pdf` is **not** Bell et al. It is Mártony, J. & Fant, G.,
  "Pole-zero matching of spectra of /l/", STL-QPSR 2:1, 1961, pp. 1–2.
- `56790.pdf` is Segers & Verhoeven, "Effects of lengthening the speech signal
  on auditory word discrimination in kindergartners with SLI", *Journal of
  Communication Disorders* 38(6), 2005 — **not relevant**.
- `3efcf749bbfff0f67322bfbf135f8a1d.pdf` is "Laboratory 9: Filter Design,
  Modeling, and the z-Plane", EECS 206, University of Michigan, v3.0, 2002 —
  relevant, contains explicit placement procedures.

---

## Kerkhoff & Boves 1993 — the architecture

**p. 1705, Abstract:**
> "The vocal tract filter is not conventional: we employ a pole-zero (ARMA)
> filter, implemented as a cascade of second order resonators and
> antiresonators."

> "The development of effective rules to control the ARMA vocal tract proved to
> be more difficult than anticipated, mainly because the physical interpretation
> of the zeros may change abruptly. From a system control point of view it
> became clear that zeros and poles cannot be allowed to move independently.
> Moreover, it appeared that the behaviour of a time-varying ARMA system is
> sensitive to internal group delays that are immaterial in stationary systems."

**p. 1706, §2:**
> "Another well known result from the theory of linear systems is that the order
> in which operations are performed is immaterial. We relied on this result when
> we decided to implement our ARMA filter in the form of a cascade of six second
> order pole-zero sections (cf. Fig. 1)."

**p. 1706, Figure 1 caption:** "Sx: sources; Fx: pole; Zx: zero; Nx nasal
resonators" — cascade as drawn:
`S1 → AV → ⊕ → F5 – Z5 – F4 – Z4 – F3 – Z3 – F2 – Z2 – F1 – Z1 – Np – Nz →`

## Kerkhoff — why section order stops being immaterial

**p. 1706:**
> "Since it was assumed that the order of the operations does not matter, we
> thought that we had complete freedom in the use of the zeros for any purpose
> for which they were needed. Specifically, we assumed that it was allowed to
> switch any zero in the cascade on or off, depending on the need determined by
> the frequency response for a given sound."

> "Operating our ARMA filter in this way led to occasional problems that were
> reminiscent of the local instabilities of all-pole filters when parameters
> change too abruptly, regardless of the fact that such abrupt changes in pole
> parameters did never occur. Analysis of the problem showed that once again we
> came across a situation where a structure that is linear as long as it is
> stationary behaves in unexpected ways when the parameters are time-varying.
> Even if manipulations of zeros do not upset the 'stationary' system response
> of the overall ARMA structure, they may still cause problems internally. When
> the zero parameters of a section in the chain are changed, the output of that
> section may change its amplitude abruptly. This may well upset the AR part of
> the succeeding section (simply because its input grows instantaneously), and
> this disturbance will last until the output of that section has damped out."

**p. 1707, their architectural fix:**
> "Fig. 2c shows the output obtained from the exact same sequence of synthesizer
> control parameters, after a simple change in the synthesizer architecture. In
> the new architecture all zeros are lumped in one large section, and the poles
> in another one. This rearrangement suffices to cope with local instabilities
> due to internal group delay in the time-varying filter."

**p. 1705–1706, the stability mechanism:**
> "resonators with time-varying parameters are not guaranteed to be stable if
> each successive set of parameters has its poles inside the unit circle... If
> large changes in the parameters {b, c} occur at a moment when the values of
> y_{n−1} and y_{n−2} are large, the resulting 5-tuple may lead to a very large
> output value, that affects the filter output for a number of future samples,
> because of the feedback structure implicit in the filter equation."

## Kerkhoff — the *follow* rule, in full

**p. 1707:**
> "In a rule based synthesis system that relies on articulatory knowledge the
> formant targets and transitions are most probably the first things to be
> specified. The main function of the zeros is to obtain the desired global
> spectral shape and slope in the presence of independently specified formant
> tracks. However, if both pole and zero parameters are interpolated between the
> target values of two neighbouring phonemes, it cannot be guaranteed that all
> intermediate frames automatically obtain the desired shape... Since the impact
> of poles and zeros on the spectrum interact, the combined pole and zero
> pattern can become rather awkward if poles and zeros are allowed to move
> independently from one target position to another. Moreover, the physical
> function of a zero in one sound may be different from its function in the next
> sound. For instance, a zero may be used to cancel a formant in one sound and
> to raise the overall spectral level in the high frequency region in another."

**p. 1707, the three strategies:**
> "• linear interpolation, independent of the simultaneous movements of poles
> and other zeros;
> • stepwise change at the phoneme boundary, i.e., the zero location is fixed at
> its target value during the final/initial transition out of/into the phoneme;
> • a zero follows two poles."

**p. 1707–1708, *follow* defined:**
> "The third interpolation strategy requires some explanation. It has been
> implemented **to prevent a zero from inadvertently cancelling a pole at the
> point where the two tracks cross.** In this strategy the relative distance
> between the frequency of a zero and its two neighbouring formants is kept
> constant during the transition. If the interpolation type *follow* is
> specified for a zero, its neighbouring formants are first determined. The
> relative distance of the zero to these formants in the stationary part of the
> phoneme are then computed, using
>
>     RelF_zero = (Zero_target − Form1_target) / (Form2_target − Zero_target)
>
> with Form1_target and Form2_target the target frequencies of the next lowest
> and next highest formant frequency and Zero_target the centre frequency of the
> zero under consideration.
>
> The bandwidth of the zero is controlled by demanding that the Q-factor of the
> 'following' zero equals
>
>     RelQ_zero = (Q_zero − Q_form) / Q_zero
>
> with Q_zero and Q_form the Q-factors of the zero and the next lower formant,
> respectively. **If the zero is below F1, Q_form is the Q-factor of F1.**
>
> RelF_zero and RelQ_zero are then used to compute the actual centre frequency
> and Q-factor of the zero during the transition, using
>
>     Zero_track = Form1_track + RelF_zero·(Form2_track − Form1_track)
>     Q_track    = Zero_track / (RelQ_zero·Q_form(track) + Q_form(track))
>
> where FormX_track is the value of the frequency of the formant to which the
> zero is tied in the frame under consideration, while Q_form(track) is the
> Q-factor of that formant."

**p. 1708, adoption and its residual problem:**
> "The rule developers working on our system display a clear preference for the
> option *follow* for zero interpolation. However, this option is not without
> problems of its own. Since zeros can change function at phoneme boundaries,
> the rule developer cannot always prevent a zero from making large jumps at a
> phoneme boundary. This can lead to abrupt changes in the spectral pattern and
> in the intensity of the resulting speech, which may give rise to the
> perception of spurious plosives. We countered this problem by adding a zero,
> that is used to smooth out the jump. The following algorithm is used:
> • compute the spectral level in the two adjacent steady states at two
> frequencies (1 kHz and 4 kHz);
> • compute target spectral levels at the same two frequencies for all frames
> during the transition by linear interpolation between the values in the steady
> states;
> • compute the spectral levels for each frame during the transition and adjust
> the parameters of the additional zero in such a way that the resulting
> spectral balance approaches the target."

**p. 1706, gain normalisation:**
> "In our system the normalization is effected by requiring that a + b + c = 1,
> i.e., by requiring that the DC-gain is unity."

## Fant & Mártony 1960 — how many poles and zeros a sound needs

**`1960_1_1_014-016.pdf`, PDF p.3:**
> "A match of fricatives in terms of two poles and one zero is generally
> sufficient for retaining a high standard of speech quality in a formant-coded
> synthesis (OVE 11)."

> "The pole at 2700 c/s and the zero at 2500 c/s of the fricative [f] constitute
> a bound pair with but small contribution to the spectrum."

> "The main peak of the [s]-spectrum of Fig. 1-7 is associated with the pole at
> 5800 c/s. The second pole at 8000 c/s contributes to build up a proper
> spectrum level at higher frequencies. The zero at 4500 c/s is placed higher
> than the corresponding zero in the measured spectrum in order to preserve a
> correct level ratio between the main formant and the low frequency part of the
> spectrum."

**PDF p.6:**
> "The essential feature of this particular palatal retroflex sound is a free
> zero at 1000 c/s and a free pole at 2800 c/s and one at 7000 c/s. A detailed
> match employing three additional pole-zero pairs associated with the
> relatively suppressed F2, F3, and F6 provides a match within a few dB from 300
> c/s to 12 kc/s... It is apparent that the resulting exaggeration of the
> relative level of the main peak is due to the absence of the high-frequency
> attenuation inherent in the two bound pole-zero pairs."

> "There is a similarity between [s] and [ʃ] in so far as the spectra of both
> possess a free zero of a frequency lower than that of the two free poles. This
> free zero contributes effectively to the high-pass structure of the spectrum
> above the zero."

**PDF p.5, Fig. 1-8 caption:** "Free poles are marked X and free zeros are
marked 0."

## Mártony & Fant 1961 — /l/, and dropping a bound pair

**`1961_2_1_001-002.pdf`, PDF p.3:**
> "Of special interest in the present investigation is the presence of one zero
> and one extra pole in the mode spectrum of [l] compared with the adjacent
> vowel. The zero is found slightly below the third formant in [l]-allophones
> connected with a front vowel, but the zero is found slightly above the third
> formant in [l]-allophones connected with a back vowel."

> "The spectral effect of the additional pole and zero is to decrease the level
> of the third or second formant but to raise the level of the fourth formant."

**PDF p.6:**
> "From our experience of speech synthesis it is quite feasible to remove the
> zero and its closest pole from the spectrum specification. A compensation for
> the lack of level in the fourth formant region does not appear to be
> important. Correct first and second formant transitions are of primary
> importance."

**PDF p.6, TABLE I-1 caption and bandwidth note:**
> "Formant frequencies (F₁ F₂ F₃ F₄ F₅) and anti-resonance frequencies (Z₁) in
> [l]-sounds and associated vowels."
> "Formant bandwidths were of the order of B₁ = 50 c/s, B₂ = 80 c/s, B₃ = 110
> c/s, B₄ = 180 c/s in vowels."

## Fujimura & Lindqvist-Gauffin 1964 — bandwidth vs frequency, and match tolerance

**`1964_5_3_001-007.pdf`, PDF p.3:**
> "The relation between the bandwidth and frequency appears to be comparatively
> simple for the first formant. There is a marked tendency that the bandwidth
> increases as the frequency decreases (towards the left of the figure). In
> other words, close vowels show much wider bandwidths, typically 70 c/s for
> [i], [y] and [u], than semi-open vowels, typically 35 c/s for a neutral vowel
> [ə]."

**PDF p.5:**
> "In this case, the bandwidth apparently depends more heavily on the particular
> vocal tract configurations, even for similar formant frequencies... close
> front vowels have very sharp resonances for this formant, a bandwidth value of
> 30 to 40 c/s for a frequency near 2000 c/s."

**PDF p.11, the stopping tolerance:**
> "When this was used in rematching the vowel samples, matches were good and
> generally within ±1 dB for most of the samples over the range from 100 to 2500
> c/s. Some samples had local deviations of up to 2 dB."

**PDF p.5, the ambiguity failure mode:**
> "The second and the third formants of [y] are located very close to each
> other, and this proximity of the formants results in an apparent single peak
> even in this continuous response curve. The analysis-by-synthesis matching
> naturally reveals that this peak consists of two formants, but an exact
> estimation of the frequencies and bandwidths is rather difficult."

## Kjellin & Kullander 1961 — automatic matching, and its drawback

**`1961_2_4_018-018.pdf`, PDF p.3:**
> "The system function describing the spectrum envelope of the synthetic version
> is expanded in a Taylor series leading to the specification of linear
> relations between the errors in all pole frequency and bandwidth data and the
> differential effects of these errors on any part of the spectrum envelope. The
> resulting set of linear equations is expressed in an orthogonalized matrix
> form and solved for optimal values of synthesis parameters. **This method has
> the drawback that gross errors in the preselected estimate of parameter data
> often lead to unnatural solutions.** The time necessary for a matching was 3
> minutes."

## EECS 206 Lab 9 — explicit placement procedure

**p. 10, §9.2.6:**
> "If we wish to design an IIR filter (with both poles and zeros), it usually
> makes sense to **start with the poles** since they typically affect the
> frequency response to a greater extent. If the frequency response that we are
> trying to match has peaks on it, this suggests that we should place a pole
> somewhere near that peak (inside the unit circle). Then, use zeros to try to
> pull down the frequency response where it is too high. As with zeros, poles
> near the origin have relatively little effect on the system's filter
> response."

> "A related idea is that of spectral slope. By having a pair of poles or zeros
> inside the unit circle and near the real axis, we can adjust the overall 'tilt'
> of the frequency response."

**p. 10, on why per-section optimisation fails:**
> "First, moving a pole or zero affects the frequency response of the entire
> system. This means that we cannot simply optimize the position of each
> pole-pair and zero-pair individually and expect to have a system which is
> optimized overall. Instead, after adjusting the position of any pole-pair or
> zero-pair, we generally need to move many of the remaining pairs to compensate
> for the changes. **This means that filter design using manual pole-zero
> placement is fundamentally an iterative design process.**"

**p. 9, §9.2.4, on near-cancellation:**
> "Notice the tendency of the poles and zeros to cancel the effects of one
> another. If a pole and a zero coincide exactly, they will completely cancel.
> If, however, a pole and a zero are very near one another but do not have
> exactly the same position, the z-plane surface must decrease in height from
> infinity to zero quite rapidly. This behavior allows the design of filters with
> rapid transitions between high gain and low gain."

**p. 21, all-pole versus pole-zero for vocal tract:**
> "Now, repeat the above for a filter with 10 poles nontrivial and no nontrivial
> zeros... You should be able to achieve a decibel matching error below 2.6...
> Note that the all-pole model produces less error with fewer total
> coefficients. **This suggests that all-pole filters are more appropriate for
> vocal tract modeling.**"

**p. 13, §9.2.8:** "Typically, our vocal tract filter can have relatively few
filter coefficients (i.e., approximately 10-20 coefficients)."

## Zhu et al. 1996 — what automation produces, what the human then edits

**p. 1, Abstract:**
> "One of the key features of the proposed method is that we have an algorithm
> to automatically measure the voicing source, unvoiced source and
> formant-antiformant parameters of the synthesizer directly from natural speech
> waveforms. After having automatically obtained estimates of the parameters
> from natural speech, one can manipulate the estimates using a flexible editing
> tool that has been developed as a part of the system."

**p. 3, §3.2:**
> "Formant and antiformant parameters are obtained by solving for the roots of
> the A(z) and B(z) polynomials, respectively."

**p. 3, §3.2, why per-period values are saved rather than continuous ones:**
> "(1) It takes immense computation time for the calculation and a large
> capacity for the storage of the parameters is needed, (2) **Estimated formant
> values are not always stable**, and (3) It is reasonable to assume the
> time-invariant nature of the vocal tract movement within one pitch period."

**p. 3, §4.1, the editing verbs:**
> "1. Modify a value: Move the mouse to the point which is to be modified, press
> the left button and drag the mouse to a new value.
> 2. Add a new value... 3. Delete a value... 4. Modify multiple values:
> Double-click at the start point and at the end point. The points between the
> start and the end are linearly changed. 5. Scale values...
> The right button is used to modify the bandwidth for the formant window."

**p. 4, §5.1, the section formula:**
> "H(z) = a / (1 − b z⁻¹ − c z⁻²),  b = 2 exp(−πB/f_s) cos(2πF/f_s),
> c = −exp(−2πB/f_s),  a = 1 − b − c, where F, B and f_s are formant frequency,
> bandwidth, sampling frequency, respectively."

**p. 4, §5.2, gain reconciliation:**
> "In the formant synthesizer, the gain of each second-order resonator at 0 Hz
> is always 0 dB. But the amplitude of the vocal tract transfer function
> B(z)/A(z) estimated by the Kalman filter at 0 Hz may not be equal to 0 dB...
> We have found that the RMS criterion yields a rather reasonable envelop of AV
> sequences as compared with the other methods."
