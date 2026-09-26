# Parked: X3-style output limiter

Found uncommitted on 26 Sep 2026 (EmuLimiter.h, 12:00; wired into TrenchDspBridge.h, 12:03). The user said nobody owns it and has ruled out levellers, so it was unwired before install. It reproduced X3's CStereoLimiter law: threshold 0.63 (-4 dBFS), gain x0.99 per sample over, x1/0.99999 back. It broke 11 gain and soft-clip checks. To restore: copy EmuLimiter.h back into plugin/source/dsp and apply emu-limiter-wiring.patch.

26 Sep 2026, later: the user wants it ("no one is actively working on it, not that we don't need it"; "the limiter should just catch the crossings"). Restored into the bridge with X3's fastest-release row (attack 0.999, release 1/0.9999) instead of the default row: on Talking Hedz sweeping Morph at Q100 over the D Rich loop, the default row ducked the music more than 1 dB for 60 % of the time; the fastest row 1.4 %. Threshold stays 0.63.

26 Sep 2026, final: user said "X3 everything". Back to X3's default row (attack 0.99, release 1/0.99999, threshold 0.63), the values read from EmulatorX.dll at release setting 50.
