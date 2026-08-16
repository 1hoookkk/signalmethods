Complex Serial Pole-Zero Speech Filter Design Manual: A Reverse-Engineering & Static Workflow Guide

1. Foundations of Pole-Zero Speech Modeling

The strategic transition from exploratory vocal tract studies to high-fidelity speech synthesis relies on the rigorous application of pole-zero matching. Unlike earlier methods that sought mere acoustic imitation, the pole-zero methodology facilitates a "structural comparison" of phonemes. This approach treats speech sounds as products of specific physical configurations of the vocal tract, modeled numerically to ensure that the synthetic output reflects the underlying acoustic architecture of human speech.

At the heart of this manual is the "analysis-by-synthesis" loop—a closed-loop process where human speech samples are measured and compared against synthetic spectra generated from tabulated data of elementary pole and zero curves. Historically, this work was shaped by significant hardware constraints. Forensic historians must note the specific implementation of the Kringlebotn feedback amplifier circuit, which achieved the continuous variation of an inductance to represent pole-zero pairs. This analog logic, alongside the OVE II synthesizer, established the blueprint for modern digital reconstruction. Moving from general theory to practical application requires a precise definition of the source characteristics that excite these complex serial filters.

2. The Source-Filter Interaction Model

To achieve accurate fricative and vowel synthesis, designers must establish a baseline "source" side of the equation. A relatively flat source spectrum is the essential starting point; without a standardized source, the contributions of the pole-zero filter cannot be isolated or accurately modeled.

Technical Specifications for Source Shaping

Based on the KTH literature, the following parameters define the Voice and Noise sources:

* Voice Source (A_0):
  * Average Slope: -12 dB/octave.
  * Radiation Transfer Correction: A differentiation factor of +6 dB/octave must be added to simulate acoustic radiation from the lips.
  * Circuit Realization: Variations in the voice source are modeled via an additional conjugate complex zero and a pole, supplemented by zeros and poles on the negative axis (implemented via simple RC circuitry).
* Noise Source (A_C and A_H):
  * A_C (Flat Noise): A baseline source with no spectral tilt.
  * A_H (Integrated Noise): Characterized by a -6 dB/octave slope.
  * Filtering Requirements: Both sources require high-pass filtering to suppress levels below 500 c/s. The A_H-gate requires an additional low-pass cutoff at 3000 c/s to prevent perceptual confusion between [h] and [sh] sounds.

The Significance of Source Matching Failing to correct for radiation and the relatively low sensitivity of the human ear in high-frequency regions fundamentally alters the perceived "center of gravity." For example, after correction, it becomes forensically clear that the [s] spectrum possesses a higher center of gravity than [f], identifying [f] as "grave" in comparison to the "sharp" quality of [s].

3. Phoneme-Specific Design Rules and Numerical Parameters

Achieving a "high standard of speech quality" necessitates the standardization of bandwidths and Q-factors. These static rules allow for the consistent reconstruction of phonemes across different synthesis platforms.

Static Design Rules for Complex Speech Filters

Phoneme	Component Type	Frequency (c/s)	Q-Factor / Bandwidth	Design Function
Fricative [s]	Pole (Main)	5800	Q = 10.0	Main spectral peak associated with constriction.
	Pole (High)	8000	Q = 3.0	Builds high-frequency level build-up.
	Zero	4500	Q = 2.5	Preserves level ratio vs low-frequency part.
Fricative [f]	Pole	2700	Q = 5.8	Bound pair component.
	Zero	2500	Q = 5.8	Bound pair; small spectral contribution.
	Pole	15000	Q = 3.3	High-damped auxiliary pole.
Lateral [l] (a)	F1	375	B1 = 50 c/s	Primary formant for [a]-allophone.
	F2	1250	B2 = 60 c/s	Secondary formant.
	Z1	2700	—	Mouth cavity zero.
Lateral [l] (i)	F1	235	B1 = 50 c/s	Primary formant for [i]-allophone.
	F2	1600	B2 = 60 c/s	Secondary formant.
	Z1	1900	—	Mouth cavity zero.
Nasalized [3]	Pole 1	517	(56)	Primary nasalized resonance.
	Pole 2	1230	(55)	Secondary resonance.
	Zero	312	(60)	Low-frequency anti-resonance.

Standard Bandwidths for Vowels To maintain spectral consistency, the following standard vowel bandwidths are utilized:

* B1: 50 c/s | B2: 60 c/s | B3: 110 c/s | B4: 180 c/s

Design Note: The Mouth Cavity Zero (Z_1) In laterals and nasals, the mouth cavity zero decreases the level of adjacent formants while raising the level of higher-order formants. However, from a synthesis standpoint, it is often feasible to remove the zero and its closest pole from the specification during transitions, as correct first and second formant transitions are of primary importance.

4. The Spectrum-Matching Workflow: Step-by-Step Reconstruction

Reconstructing the spectral envelope of a target human sample requires a disciplined forensic workflow.

1. Initial Sampling: Use a wave analyzer with a 125-c/s bandwidth to establish the measured spectra (the "broken lines" in historical diagrams).
2. Primary Parameter Placement: Locate the main peak (e.g., the 5800 c/s pole for [s]) and the primary high-pass zero.
3. Bound Pair Integration: Add suppressed/bound poles (F2, F3, F6) to provide a match within a few dB up to 12 kc/s. These weak formants prevent the exaggeration of the main peak.
4. Automatic Fine-Tuning: Utilize a Taylor series expansion of the system function to establish linear relations between pole/bandwidth errors and spectral envelope effects. The resulting set of linear equations must be expressed in an orthogonalized matrix form and solved for optimal synthesis values.

The Risk of "Unnatural Solutions" Automatic programs are highly sensitive to the "first guess." A gross error in the preselected estimate of formant frequencies or bandwidths will lead the mathematical optimization to converge on unnatural solutions that do not represent human speech.

5. Forensic Analysis: Numerical Fingerprints Checklist

These "fingerprints" identify historical KTH or MIT design logic in unknown implementations.

* [ ] The "Bound Pair" Signature: A pole and zero within 200 c/s of each other (e.g., [f] at 2700/2500) used to suppress specific spectral regions.
* [ ] The 2000 c/s Offset: A free zero located approximately 2000 c/s lower in [sh] than in [s], defining the high-pass structure.
* [ ] Labiodental Rise: In [f] spectra, the level rises continuously all the way up to 12 kc/s, distinguishing labiodental constriction from other fricatives.
* [ ] High-Dissipation Low F1: Bandwidths of ~70 c/s for close vowels ([i], [y], [u]) vs. ~35 c/s for neutral vowels ([3]), reflecting wall vibrations in the vocal tract.
* [ ] Quantal Step Signatures: Parameters sampled at 40 Hz and quantized to 4 bits (16 levels), specifically using 50 c/s steps for F1 and 150 c/s steps for F2.
* [ ] The Buzz Bar: A pronounced low-frequency energy band (around 170-200 c/s) identifying voiced stops ([b], [d]).

6. Implementation in the Z-Plane

The evolution of speech synthesis represents a transition from analog RC networks and mechanical "painted plexiglass discs" to discrete-time pole-zero placement. Historically, these painted discs served as mechanical lookup tables, with unpainted portions incrementally defining frequency curves with a 16 dB dynamic range. Today, these are superseded by the z-plane, where we model the vocal tract as a natural filter.

In the z-plane, there is a direct graphical relationship to the frequency response: poles create resonance peaks, and zeros create anti-resonance valleys. The shift toward "automatic modeling methods" for vowel classification provides a significant performance gain over the manual trial-and-error of the past. By automatically calculating the best set of pole frequencies and bandwidths for an optimal fit, modern systems achieve the precision once reserved for human experts. Yet, the enduring value of Fant’s complex serial filter architecture remains the foundational logic for modern acoustic modeling.
