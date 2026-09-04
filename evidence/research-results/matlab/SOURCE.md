# Tyson's MATLAB scripts (copied 2026-09-04 from C:/Users/hooki/OneDrive/Documents/MATLAB)

corner.m (1 Sep): one 50 ms Hamming slice, pre-emphasis [1, -0.96], Burg LPC order 12 at the file
rate, roots -> frequency, radius, bandwidth per lane, sorted ascending, printed for a Python compiler.
sound_to_skeleton.m (31 Aug): loudest 120 ms frame, order 12, up to six pole pairs, spectrum vs LPC
envelope plot, CSV. voice_body.m, first_pole_zero.m (31 Aug), fit_body.m, match_body.m (1 Sep).
Same job as FROM AUDIO in the workstation. Differences from native/core/body_from_audio: pre-emphasis
and Burg's method (stable poles from short windows); ours runs autocorrelation Levinson, SPEECH mode
at 11,025 Hz because order 12 at 44.1 kHz loses F1/F2.
