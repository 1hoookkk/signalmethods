# LENS offline fit on the 303 at five octaves, open and closed

Reference: the whole note's time-averaged power spectrum at a quarter of the rate, 4096-point Hann frames, smoothed across frequency by 150 Hz to remove the harmonic comb, on the 128 corpus bins, floored 30 dB under the peak, level removed. This reference is a new experiment, not recovered behaviour. Fit A: LPC-12 from that smoothed average by autocorrelation and Levinson, offline, a new experiment. Fit B: the recovered Peevers per-frame LPC-12 envelope averaged over every frame of the note. Errors are RMS dB over the bins.

| note | RMSE fit A dB | RMSE fit B dB | fit A poles Hz / bandwidth Hz |
|---|---|---|---|
| 303 closed C1 | 1.43 | 6.17 | 250/67 2169/647 3163/711 4112/750 5047/767  |
| 303 closed C2 | 1.41 | 7.07 | 253/68 2170/646 3163/711 4112/749 5047/766  |
| 303 closed C3 | 1.51 | 6.47 | 256/76 2171/658 3164/721 4113/759 5047/776  |
| 303 closed C4 | 1.83 | 6.06 | 233/58 2169/699 3163/766 4112/804 5047/822  |
| 303 closed C5 | 9.28 | 9.97 | 232/178 362/121 2258/1202 3194/1241 4124/1255 5050/1262  |
| 303 open C1 | 0.45 | 2.36 | 812/1317 1603/369 1938/429 3546/549 4736/1295  |
| 303 open C2 | 0.40 | 3.03 | 939/1166 1631/330 1974/413 3558/537 4761/1285  |
| 303 open C3 | 0.51 | 3.18 | 1025/1034 1658/288 2020/385 3592/560 4746/1266  |
| 303 open C4 | 0.84 | 3.51 | 200/526 1071/897 1691/266 2053/377 3613/608 4775/1373  |
| 303 open C5 | 9.77 | 11.99 | 379/372 1270/862 1672/327 2073/400 3681/771  |
