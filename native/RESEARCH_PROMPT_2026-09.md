# TRENCH — research brief (September 2026)

Context, fixed: TRENCH authors morphing filters as four corners of six 2-pole/2-zero sections at 44.1 kHz. Authoring is by hand on a fixed display (octave lines from 20 Hz, 10 dB lines from −30 to +30): a reference sound is drawn as an RMS spectrum line, poles are placed at its peaks, widths fitted, zeros drawn to sand the residual, done at ±1 dB. Do not research filter theory, fitting algorithms, E-mu history or interpolation. Everything must be current as of August 2026; date every source.

## Question 1 — The reference line

What is the best-practice way to turn a recording into a magnitude line a designer matches by eye? Answer with one recommended recipe and cite it: analysis window (length, placement for a held note vs. an attack), Welch averaging parameters, fractional-octave smoothing width (1/3, 1/6, 1/12) and why, log-frequency resampling, level normalisation, and how REW and Praat do it by default. Cover held instrument notes, sustained vowels, impulse responses, and cabinet/room measurements separately if the recipe differs. Give the SciPy and MATLAB calls that implement it.

## Question 2 — Where the dry source material is

For each class below, list the best freely available dry recordings, formant tables and impulse-response libraries as of 2026: URL, licence, date, sample rate, dry/anechoic yes/no, one line on why. Newest first. Say what to avoid for matching (room, processing or mic colouration baked in).

Classes: sustained vowels and diphthongs with measured F1–F4 (Hillenbrand 1995, Peterson & Barney 1952, VocalSet, anything newer); solo brass with mutes; clarinet, oboe, flute breath and overblow; acoustic guitar body close-miked at the sound hole; electric guitar pick position; bass drum, snare, hi-hat, cymbals; marimba, tambourine, udu, bells/gamelan, wine glass, kalimba/steel drum; piano soundboard, Rhodes, clavinet; cello and string section; speaker cabinets and rooms (impulse responses); sawtooth/square/FM oscillators for resonance demos.

Start from and update: University of Iowa Musical Instrument Samples, Philharmonia Orchestra samples, Good-Sounds, TinySOL/OrchideaSOL, NSynth, OpenAIR, EchoThief, free cabinet IR packs, Klatt 1980 Table II.

## Question 3 — Test this first-guess fitter

TRENCH's candidate fitter (MATLAB, `tools/matlab/match_body.m`) does: RMS line as in Question 1 → minimum-phase response from the magnitude (cepstral method) → `invfreqz(H, w, 12, 12, uniform weights, 30 iterations)` → reflect unstable poles to r = 0.999 and non-minimum-phase zeros inside → conjugate pairs to (Hz, bandwidth) → keep the six lowest pole pairs → give each the zero pair nearest in octaves → report max |body − line| over 40 Hz–16 kHz, target ±1 dB. A second candidate is Burg LPC order 12 on the waveform (poles only, zeros drawn by hand). 

Evidence from its first run (2026-09-01, a known six-row body excited by a 110 Hz sawtooth): the RMS line carried the sawtooth's harmonics and −6 dB/oct tilt, and the unweighted least-squares fit collapsed onto the single loudest peak (max residual 68.6 dB). So the research must state how the excitation is removed or avoided for each input kind, and which error weighting (relative / log-magnitude / iterative reweighting) keeps a 12/12 fit from collapsing. Treat this recipe as unproven.

Judge both against the strongest alternatives as of 2026 — warped LPC, Steiglitz–McBride, vector fitting / stabilised rational approximation, and any recent robust method — on three inputs: a sustained vowel, a held cello note, a guitar-cabinet impulse response. Report, per method and input, the max residual in dB at six sections, stability, and failure modes (poles on partials, zero/pole cancellation, order collapse). Name one winner per input kind and the change to make to `match_body.m` if it is not the winner. Give the SciPy equivalent of the winner so MATLAB can be dropped.

## Deliverables

1. The reference-line recipe on one page, with the SciPy and MATLAB calls.
2. The source table, one row per library, newest first.
3. The fitter verdict: one table (method × input → max residual, stability, failure mode), one winner per input, the concrete change to `match_body.m`.
4. Source list with links and dates.
