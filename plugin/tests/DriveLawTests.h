#pragma once
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "dsp/DriveLaw.h"
#include <cmath>
#include <cstdio>

inline int driveLawTests()
{
    int failed = 0;
    const auto check = [&] (bool ok, const char* label)
    {
        std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", label);
        if (! ok) ++failed;
    };
    bool identity = true;
    for (int i = -4000; i <= 4000; ++i)
        identity = identity && trench::driveLaw ((float) i * 0.001f, 0.0f) == (float) i * 0.001f;
    check (identity, "DRIVE 0 is the identity at every level");
    bool ceiling = true, lift = true, monotone = true, smooth = true, slope = true;
    for (int k = 0; k <= 20; ++k)
    {
        const float a = (float) k / 20.0f;
        ceiling = ceiling && trench::driveLaw (1.0f, a) == 1.0f && trench::driveLaw (-1.0f, a) == -1.0f;
        float previous = trench::driveLaw (-3.0f, a);
        for (int i = -2999; i <= 3000; ++i)
        {
            const float x = (float) i * 0.001f;
            const float y = trench::driveLaw (x, a);
            monotone = monotone && y >= previous;
            previous = y;
            if (x >= 0.0f && x <= 1.0f)
                lift = lift && y >= x;
        }
        const double h = 1.0e-4;
        const double inside = (trench::driveLaw (1.0f, a) - trench::driveLaw ((float) (1.0 - h), a)) / h;
        const double outside = (trench::driveLaw ((float) (1.0 + h), a) - trench::driveLaw (1.0f, a)) / h;
        smooth = smooth && std::abs (inside - outside) < 2.0e-3;
        slope = slope && std::abs (trench::driveLaw (1.0e-4f, a) / 1.0e-4f - (1.0f + 0.5f * a)) < 1.0e-3f;
    }
    check (ceiling, "full scale maps to full scale at every DRIVE");
    check (lift, "below full scale DRIVE never lowers the level");
    check (monotone, "the law never turns back");
    check (smooth, "slope is continuous where overs begin");
    check (slope, "small-signal gain is 1 + DRIVE/2");
    bool stageIdentity = true;
    for (int i = -4000; i <= 4000; ++i)
        stageIdentity = stageIdentity && trench::driveMakeup ((float) i * 0.001f, 0.0f) == (float) i * 0.001f;
    check (stageIdentity && trench::drivePush (0.0f) == 1.0f, "DRIVE 0 adds no gain before the filter and none after");
    const auto thirdHarmonicDb = [] (float peak, float amount)
    {
        constexpr int n = 4096;
        double f1re = 0.0, f1im = 0.0, f3re = 0.0, f3im = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 2.0 * 3.14159265358979323846 * 8.0 * (double) i / (double) n;
            const double y = trench::driveMakeup (trench::drivePush (amount) * peak * (float) std::sin (w), amount);
            f1re += y * std::cos (w); f1im += y * std::sin (w);
            f3re += y * std::cos (3.0 * w); f3im += y * std::sin (3.0 * w);
        }
        return 20.0 * std::log10 (std::sqrt (f3re * f3re + f3im * f3im) / std::sqrt (f1re * f1re + f1im * f1im));
    };
    const double h3 = thirdHarmonicDb (0.25f, 1.0f);
    std::printf ("      H3 at -12 dBFS, DRIVE full: %.1f dB\n", h3);
    check (h3 > -24.0, "DRIVE at full reaches the knee from a -12 dBFS peak");
    check (trench::driveMakeup (trench::drivePush (1.0f) * 1.0e-4f, 1.0f) / 1.0e-4f >= 1.5f - 1.0e-3f, "DRIVE never lowers quiet material");
    return failed;
}
