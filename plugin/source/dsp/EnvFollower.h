#pragma once
#include <algorithm>
#include <cmath>
namespace trench
{
class EnvFollower
{
public:
    static constexpr int kHop = 32;
    void prepare (double sampleRate)
    {
        sr = std::max (1.0, sampleRate);
        const double hopS = (double) kHop / sr;
        attackAlpha  = 1.0 - std::exp (-hopS / (kAttackMs / 1000.0));
        releaseAlpha = 1.0 - std::exp (-hopS / (kReleaseMs / 1000.0));
        reset();
    }
    void reset()
    {
        peak = 0.0; offset = 0.0;
    }
    void setAmount (float a)
    {
        amount = std::clamp ((double) a, 0.0, 1.0);
        if (amount <= 0.0) offset = 0.0;
    }
    bool armed() const noexcept { return amount > 0.0; }
    void advance (float blockPeak) noexcept
    {
        if (! armed()) { offset = 0.0; return; }
        const double p = std::isfinite (blockPeak) ? std::abs ((double) blockPeak) : 0.0;
        peak += (p - peak) * (p > peak ? attackAlpha : releaseAlpha);
        const double db = std::max (kFloorDb, 20.0 * std::log10 (std::max (peak, 1.0e-9)));
        const double calibrated = std::clamp ((db - kFloorDb) / (kCeilingDb - kFloorDb), 0.0, 1.0);
        offset = calibrated * amount;
    }
    float currentOffset() const noexcept { return amount > 0.0 ? (float) offset : 0.0f; }
private:
    static constexpr double kAttackMs = 1.0, kReleaseMs = 80.0;
    static constexpr double kFloorDb = -60.0, kCeilingDb = -6.0;
    double sr = 48000.0, attackAlpha = 0.0, releaseAlpha = 0.0;
    double amount = 0.0, peak = 0.0, offset = 0.0;
};
}
