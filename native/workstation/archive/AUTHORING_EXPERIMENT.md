# The authoring operation: experiment

Supporting `AUTHORING_DECISION.md`. Script `evidence/research-results/authoring-operation/
experiment.py`, output `results.txt` beside it. Reproduce with `python experiment.py`; under
three minutes.

## Experiment

`experiment.py` reimplements `decode_word`, `encode_word`, `interpolate_word`,
`section_values_to_biquad` and `geometry_from_words` and is checked bit-exact against a
verifier linked to `trench_native_core.lib` (15 decode values, 10 encode values, one Klatt
row's biquad, geometry, round trip and three interpolations all agree). Inputs: the Klatt 1980
bank and `plugin/presets/p2k/talking_hedz.body240`. Full numbers in `results.txt`.

Part A, separation. With zeros live, a 12th-order all-pole fit from the signal reaches 4 to
5 dB rms envelope error within 40 dB of the peak and the residual's spectral flatness stays
below the source's (0.49 against 0.56 for noise, Klatt /æ/; 0.33 against 0.56, Talking Hedz
M0Q0). For the poles-only versions of the same corners, time-domain autocorrelation LPC-12 on
30k samples does not recover the cascade (20 to 26 dB error, residual flatness 0.000), and
0.97 pre-emphasis makes the dB error worse; the same order fitted from the exact magnitude
spectrum, the patent's frequency-domain path, recovers them to 4.6 and 6.8 dB, and a 60 dB
floor on that spectrum, which conditions the Toeplitz matrix from 1e17 to 1e5, moves the
target away from all-pole and the error back to 21 to 25 dB. Averaging five pitches before
LPC, as the patent does, lands at 4.85 dB on the factory corner: the zeros remain. Nothing in
this part recovers e; the flatness column is the measurement of that.

Part B, coupling. A radius edit by word 3 alone moves the pole: −0.1, −3.3 and −7.2 cents on
Klatt /æ/ rows 1 to 3 for halving 1 − R, and −15.2 cents at 2,430 Hz for r 0.9775 → 0.999,
with r itself landing as intended. Re-encoding words 2 and 3 together holds the angle to
0.0 cents at the same radius. An angle edit by word 2 alone leaves r unchanged exactly.
Quantisation of word 2 near 620 Hz: one code is 0.1 cents, 256 codes 50 cents. The chip's
word lerp from Talking Hedz M0Q0 to M1Q0 on row 2 carries the pole 1,006 → 852 → 695 → 479 →
329 → 227 Hz at t = 0, 0.1, 0.25, 0.5, 0.75, 1 with radius rising monotonically: a smooth,
monotone, non-linear path in Hz.

Part C, the running filter, switch A → B mid-note on a 110 Hz impulse train, Talking Hedz
M0Q0 → M1Q0 (metrics: output step at the switch relative to the pre-switch peak; rms
deviation from the steady-state B output over the next 2,048 samples, relative):
hard switch with state retained 0.127 / 1.77; reset 0.045 / 0.18; word lerp over 256
samples with state retained 0.048 / 0.18; word lerp over 2,048 samples 0.048 / 1.68 (the path
itself differs from B for that long); output crossfade 0.048 / 0.08; derived state map that
preserves the next two zero-input outputs 0.253 / 2.34. Two stable sections at r 0.949
alternated every sample diverge to infinity; the two factory corners alternated every eight
samples reach 2.6e193 against a steady maximum of 0.088.

Reading: the chip's own operation, a slow lerp of words with the state left alone, is as
continuous as a reset on this metric without the reset's step; the derived state map is
worse than doing nothing; frozen-time stability says nothing about fast switching, and the
runner's 256-sample approach is the rate limit that keeps the plugin inside the safe region.

