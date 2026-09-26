# Parked: X3-style output limiter

Found uncommitted on 26 Sep 2026 (EmuLimiter.h, 12:00; wired into TrenchDspBridge.h, 12:03). The user said nobody owns it and has ruled out levellers, so it was unwired before install. It reproduced X3's CStereoLimiter law: threshold 0.63 (-4 dBFS), gain x0.99 per sample over, x1/0.99999 back. It broke 11 gain and soft-clip checks. To restore: copy EmuLimiter.h back into plugin/source/dsp and apply emu-limiter-wiring.patch.
