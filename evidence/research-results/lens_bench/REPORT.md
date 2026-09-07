# LENS fitter benchmark on cab impulse responses

Ground truth: FFT magnitude of each IR on the 128 log bins 40 Hz to 5 kHz, floored 30 dB under the peak, mean removed. Estimate: white noise convolved with the IR, decimated by 4, Peevers's LPC-12 envelope per 128-sample hop, the same bins and normalisation, averaged over hops with memory m (SMOOTH). Errors are RMS dB over the bins after the first second. Jitter is the mean frame-to-frame RMS change. Pole jitter is the standard deviation of the first resonance in cents and Hz of bandwidth. Convergence is the time after a switch from the previous IR until the error is within 1.5 dB of the settled error.

IRs: 98

| memory | SMOOTH ms | mean RMSE dB | frame jitter dB | F1 jitter cents | B1 jitter Hz | converge after switch s |
|---|---|---|---|---|---|---|
| 0 | 0 | 11.70 | 0.817 | 670.6 | 200.1 | 0.013 |
| 0.5 | 16.7497 | 11.68 | 0.434 | 670.6 | 200.1 | 0.013 |
| 0.8 | 52.0292 | 11.67 | 0.196 | 670.6 | 200.1 | 0.018 |
| 0.9 | 110.193 | 11.66 | 0.103 | 670.6 | 200.1 | 0.021 |
| 0.95 | 226.345 | 11.65 | 0.053 | 670.6 | 200.1 | 0.026 |
| 0.98 | 574.675 | 11.65 | 0.022 | 670.6 | 200.1 | 0.012 |

Best five at memory 0.8:
- American Twin 2x12 Dark 160 8.97 dB
- American Twin 2x12 Medium 160 9.50 dB
- American Twin 2x12 Bright 160 9.71 dB
- American Twin 2x12 Dark Mix 9.79 dB
- British Alnico 2x12 Medium 160 9.83 dB

Worst five at memory 0.8:
- LuxOVibe 2x10 Dark 421 14.38 dB
- Brown Deluxe 1x12 Dark 57 13.99 dB
- LuxOVibe 2x10 Dark Mix 13.76 dB
- Brown Deluxe 1x12 Dark Mix 13.73 dB
- Brown Deluxe 1x12 Bright 57 13.72 dB
