# Dana C. Massie: bibliography

Date order. Patents are placed by filing date.

## 1985

Massie, Dana C. "The Emulator II Computer Music Environment." ICMC 1985 Proceedings, pp. 111-114. E-Mu Systems, Santa Cruz CA. File: `icmc1985_emulator_ii.pdf`, `.txt`.
Massie's account of E-mu's own authoring environment: a real-time sampler joined to an off-line analysis and resynthesis system, "waveform display and editing, phase vocoder spectrum analysis and resynthesis, and synthesis using several well-known computer music algorithms", with each voice's spectrum shaped by "a 4-pole voltage-controlled lowpass filter (VCF)" under its own ADSR.

## 1987

Misek, Steven M.; Massie, Dana C. "Multitasking Operating System Design for Electronic Music." AES 5th International Conference: Music and Digital Technology, 6 May 1987. WaveFrame Corporation, Boulder CO. AES e-library 4671. Paywalled; abstract only.
A digital audio workstation operating system paper, not a filter paper: it covers "voice allocation routines, object-oriented MIDI-to-voice mapping routines, real-time event processing, voice garbage collection, signal processor scheduling, and distributed processing" in WaveFrame's ARTOS.

## 1991

Massie, Dana C. "An Engineering Study of the 4-Multiply Normalized Ladder Filter." AES 91st Convention, 6 October 1991, preprint 3187. E-mu Systems Inc., Scotts Valley CA. AES e-library 5529. Paywalled; abstract only.
The convention version of the 1993 journal paper: "An engineering example of an orthogonal second order digital allpass filter implementation is presented in a fully parametric audio equalizer application. Independent coefficients control frequency, Q, and boost/cut."

## 1992

Stonick, Virginia L.; Massie, Dana. "ARMA filter design for music analysis/synthesis." ICASSP 1992, San Francisco, pp. 253-256. Paywalled; not fetched.
Pole-zero (ARMA) fitting of instrument-body spectra using homotopy continuation, so that "the desired fixed-order optimal ARMA model will consistently be computed" rather than falling into a local minimum.

Massie, Dana C.; Rossum, David P. "Digital sampling instrument." US 5,248,845, filed 20 March 1992, E-mu Systems Inc. File: `patents/US5248845.pdf`.
Formant-preserving transposition: "An analysis stage separates and stores the formant and excitation components of sounds from an instrument. On playback, either the formant component or the excitation component may be manipulated" - the source/filter model behind E-mu's filter work, cited to LPC and singing-voice synthesis literature.

Massie, Dana C. "Digital signal processor for adding harmonic content to digital audio signals." US 5,524,074, filed 29 June 1992, E-mu Systems Inc. File: `patents/US5524074.pdf`.
"A digital audio signal processor for adding harmonic content to an input audio signal through a non-linear transfer function with discontinuities" - waveshaping as an effects stage, built on Le Brun's digital wave-shaping synthesis.

Massie, Dana C.; Stonick, Virginia L. "The Musical Intrigue of Pole-Zero Pairs." ICMC 1992 Proceedings, pp. 22-25. E-mu Systems Inc. / Carnegie Mellon. Files: `icmc1992_musical_intrigue_pole_zero_pairs.pdf`, `.txt`.
The clearest statement of the cascade-versus-parallel ruling that the Z-plane filter rests on: "For a small number of filter coefficients, the perceptually significant features of the spectral response of an instrument can be better resolved by including zeros as well as poles", and against parallel summing, "The poles are independently controlled by each separate denominator but the zeros are determined by how the second order sections add together."

## 1993

Massie, Dana C. "An Engineering Study of the Four-Multiply Normalized Ladder Filter." Journal of the Audio Engineering Society 41(7/8), July 1993, pp. 564-582. E-mu Systems Inc. AES e-library 6994. Paywalled; abstract only.
The filter structure paper: "An engineering example of an orthogonal second-order digital all-pass filter implementation is presented in a fully parametric audio equalizer application. Independent coefficients control frequency, Q, and boost/cut. The noise and scaling properties of the four-multiply normalized ladder structure are analyzed and shown graphically."

Stonick, Virginia L.; Massie, Dana. "Optimal LS IIR filter design for music analysis/synthesis." ICASSP 1993, IEEE Xplore document 230532. Paywalled; not fetched.
Least-squares design of "fixed, low-order infinite impulse response (IIR) filters for modeling the perceptually significant features of the spectra of string instrument bodies" - the analysis half of the authoring problem.

## 1995

Massie, Dana C. "Method and apparatus for three dimensional audio spatialization." US 5,943,427, filed 21 April 1995, Creative Technology Ltd. Already held in `evidence/patents`; skipped here.

White, Paul. "Emu Systems: Sampling, The Future & CA." Sound on Sound, October 1995. File: `sos_1995_emu_sampling_the_future.txt`.
Massie claims the cube as his own invention and states its purpose: the H chip "could handle 14th-order filters, which meant there were 28 different parameters to control, and my contribution was to come up with a simple method of using them in a musical way by putting them into what we call the 'filter cube', as used in the Morpheus. The cube is just a way of mapping musical dimensions onto synthesizer dimensions." He also gives the reason the corners were hand-voiced rather than opened up: "if you have a filter with a 12dB peak and another with a 10dB peak and they cross, you end up with a 24dB peak - which makes it really easy to create something that overloads. By pre-packaging the filters, we could keep those intermediate peaks from getting out of control." He anticipates the authoring tool: "there were possibilities for multi-dimensional cubes... the four corners of a square could be full instrument families."

## 1996

Massie, Dana C.; Rossum, David P. "Digital sampling instrument." US 5,698,807, filed 5 March 1996, Creative Technology Ltd. File: `patents/US5698807.pdf`.
Continuation of US 5,248,845 with the same formant/excitation disclosure, 20 claims, assigned to Creative after the buy-out.

## 1997

Massie, Dana C. "Digital signal processor for adding harmonic content to digital audio signal." US 5,748,747, filed 9 April 1997, Creative Technology Ltd. File: `patents/US5748747.pdf`.
Continuation of US 5,524,074; same non-linear transfer function with discontinuities.

## 1998

Massie, Dana C. "Wavetable Sampling Synthesis." Chapter 8 in Kahrs, Mark; Brandenburg, Karlheinz (eds), Applications of Digital Signal Processing to Audio and Acoustics, Kluwer Academic, 1998, pp. 311-341. Closed access; abstract and section list only. File: `kluwer1998_wavetable_chapter_frontmatter.pdf`.
Massie's survey of the sampler signal path, in which filtering is one of the named expressive extensions: "Extensions to simple wavetable playback include looping of waveforms, enveloping of waveforms, and filtering of waveforms to provide for improved expressivity, i.e., spectral and time structure variation of the perfomed notes"; section 8.2.7 is Filtering, 8.2.3 Pitch Shifting Technologies, and the chapter compares "band limited sample rate conversion for pitch shifting, linear interpolation, and traditional computer music phase increment oscillator design."

## 1999

Massie, Dana. saol-users, 22 November 1999, message 0176. File: `saol_0176.txt`.
The cube stated in public as a filter-coefficient interpolation scheme: for a parametric equalizer built as a 5-multiply biquad, "all of the intermediate interpolated frames are all legal parametric equalizer shapes. This actually is a miracle", so "it is only necessary to store 8 initial frames, one for each corner of a cube, where the axes are the three user parameters of a traditional parametric equalizer. All intermediate setting for the PEQ can then be generated by simple bi-linear interpolation from these corner sets."

Massie, Dana. saol-users, 23 November 1999, message 0179. File: `saol_0179.txt`.
The engineering rules around that interpolation: stability under interpolation of direct-form biquads via Moorer's triangle of stability, "if you start with a coefficient set that is stable, and you end with a coefficient set that is stable, all linear interpolated values in between are also stable"; coefficients must be ramped at the sample rate to avoid zipper noise; poles and zeros of interpolated parametric EQs "follow arcs on the z-plane"; the 4-multiply biquad "does *NOT* interpolate properly" where the 5-multiply one does; high-order direct form and the parallel form both interpolate badly, since in parallel form "a variation in any coefficient causes *all* of the zeros to shift in unpredictable ways", so "outside of the lattice/ladder filters, I would suggest using cascaded second order sections... for interpolation."

## 2000

Massie, Dana. "Re: sound FX generator." saol-users, 27 January 2000, message 0203. File: `saol_0203.txt`.
Points a correspondent at Andrew Horner's JAES analysis work for deriving synthesis parameter sets from real instruments; no filter content beyond the tool recommendation.

## 2001

Massie, Dana. "Soft vs. Hard (was RE: Saol acceptance.)" saol-users, 20 January 2001, message 0377. File: `saol_0377.txt`.
On authoring tools rather than filters: software synthesis promises "excellent displays, generalized mapping networks, 'Global Editing', Timbre Spaces, Neural Network training algorithms", but is "hobbled by 'the complexity barrier' - that invisible wall that users and programmers hit", the same objection he raises against exposing the raw filter chip in the 1995 interview.

## 2004

Dahl, Luke; Jot, Jean-Marc; Vu, Vincent; Massie, Dana. "Reverberation processor for interactive audio applications." US 2004/0213416 A1, filed 24 May 2004. File: `patents/US20040213416A1.pdf`.
A delay-line reverberator with a single continuous control over echo salience: "an increase and a decrease in the salience of the echo effect is dependent upon the control parameter", the same one-knob-over-many-parameters posture as the cube.

## 2009

Watts, Lloyd; Massie, Dana; Sansano, Allen; Huey, Jim. "Voice Processors Based on the Human Hearing System." IEEE Micro 29(2), pp. 54-63. Paywalled; not fetched.
Audience's cochlea-modelled filterbank voice processor.

## 2010

Massie, Dana. "Multi-function floating point unit." US 9,104,510 B1, filed 30 April 2010, Audience Inc. File: `patents/US9104510.pdf`.
An audio arithmetic unit rather than a filter: "a high-performance flexible vector floating point arithmetic unit... which can perform a single-cycle throughput complex multiply-and-accumulate operation, as well as a Fast Fourier Transform (radix-2 decimation-in-time) Butterfly operation."

Clark, Brian; Massie, Dana. "Asynchronous sample rate converter." US 8,405,532 B1, filed 28 May 2010, Audience Inc. File: `patents/US8405532.pdf`.
Coefficient interpolation applied to rate conversion: "Coefficients of a (polyphase) finite impulse response filter are interpolated based on a current time register value."

Avendano, Carlos; Murgia, Carlo; Massie, Dana. "Low complexity bandwidth expansion of speech." US 8,700,391 B1, filed 30 September 2010, Audience Inc. File: `patents/US8700391.pdf`.
Spectral folding plus a feature-selected shelf filter: "The modification may be performed by a shelf filter selected based on the feature."

Massie, Dana; Laroche, Jean. "Low latency active noise cancellation system." US 8,848,935 B1, filed 19 November 2010, Audience Inc. File: `patents/US8848935.pdf`.
Digital filter circuitry replacing analog ANC filters, "which is not subject to the inaccuracies and drift of analog filter components."

## 2013

Massie, Dana; Rossum, David P.; Clark, Brian; Rub, Leonardo; Laroche, Jean. "Sample rate conversion using infinite impulse response filters." US 8,618,961 B1, filed 12 March 2013, Audience Inc. File: `patents/US8618961.pdf`.
Massie and Rossum together again on time-varying IIR filtering for rate conversion; the largest of the collected patents at 47 pages.

## 2017

Meyer, Chris. "Modular NAMM 2017.3: Expert Sleepers, Five12, & Rossum Electro-Music." Learning Modular. Secondary source, not saved.
Reports that "synthesis pioneers Max Mathews and Dana Massie had since created a 'phasor filter' design, which uses a polar plot of complex numbers to ensure the amplitude does not run away", which Rossum built on for the Eurorack Morpheus after the original Morpheus filter had a coefficient that "would sometimes go over the value of 1".

## 2024

NAMM Oral History Program, Dana Massie, interviewed 7 October 2024. File: `namm_2024_oral_history_dana_massie.txt`.
Biography page: Massie was hired by Dave Rossum "at E-mu to develop innovative programmable computer systems and a cutting-edge sound library for the Emulator II", later Director of the Creative Advanced Technology Center; the interview itself is video only.

## 2025

Massie, Dana C. et al. "Warped filter architecture with reduced processing rate systems and methods." WO 2026/072381 A1, filed 16 September 2025, Apple Inc. Outside the 1985-2010 window; PDF not available from patentimages. Listed for completeness because the title is a filter architecture.

## Other Massie patents found, outside the filter/sampling scope

US 8,611,551 B1 (2013, ANC, Audience); US 8,787,587 B1 (2012, non-acoustic sensor parameter selection, Audience); US 9,532,155 B1 (2014, ultrasound acoustic environment monitoring, Knowles); US 11,166,099 B2 (2020, headphone ANC and speaker protection, Apple).
