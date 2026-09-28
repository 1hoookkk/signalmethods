# 5D

The 5D button, last in the GAIN row after OUTPUT, enables a fixed stereo QSound-derived effect.
The automatable `fiveD` parameter defaults to off and is stored in project state.
Projects without this parameter restore it to off. The switch crossfades over
20 ms. Mono instances retain their mono signal.

The effect renders the left input at the mirrored -90 degree position and the
right input at +90 degrees, then sums their two-channel contributions. This
stereo composition is a local product choice; it is not a recovered vendor
stereo-enhancer mode.

The +90 degree anchor comes from the existing behavioral capture
`evidence/authoring/captures/qsound/qcreator_qright90_impulse_11025.wav`.
Its SHA-256 is `43a28384e29538a738a3b1c88604623500fe0cc3ea47fb589e7905e022759a42`.
The impulse input was 0.5, so the measured PCM16 output at samples 30000-30022
is divided by 16384 to obtain the response. The leading channel is 16382/16384;
the opposite-channel response has 23 taps including its initial zero.

At other host rates, the shadow response is linearly resampled with the sample
period correction. The leading channel remains immediate. The stage is the
last in the chain, after the manual OUTPUT gain. It adds no
normalization, automatic gain, limiter, or latency compensation.

The negative cross-channel response changes the level and tone of correlated
stereo material. Equal left/right input remains equal left/right. Listening
acceptance of the combined stereo presentation is separate from matching the
measured anchor.
