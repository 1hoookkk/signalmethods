#pragma once
#include <algorithm>
#include <cmath>
namespace trench
{
class TransientDetector
{
public:
    static constexpr int kHop = 32;
    void prepare (double sampleRate)
    {
        const double hopS = (double) kHop / std::max (1.0, sampleRate);
        fastAttackAlpha  = 1.0 - std::exp (-hopS / (kFastAttackMs / 1000.0));
        fastReleaseAlpha = 1.0 - std::exp (-hopS / (kFastReleaseMs / 1000.0));
        slowAttackAlpha  = 1.0 - std::exp (-hopS / (kSlowAttackMs / 1000.0));
        slowReleaseAlpha = 1.0 - std::exp (-hopS / (kSlowReleaseMs / 1000.0));
        reset();
    }
    void reset()
    {
        fast = 0.0; slow = 0.0;
    }
    void advance (float blockPeak) noexcept
    {
        const double p = std::isfinite (blockPeak) ? std::abs ((double) blockPeak) : 0.0;
        fast += (p - fast) * (p > fast ? fastAttackAlpha : fastReleaseAlpha);
        slow += (p - slow) * (p > slow ? slowAttackAlpha : slowReleaseAlpha);
    }
    float current() const noexcept
    {
        return (float) std::clamp ((fast - slow) / (slow + 1.0e-6), 0.0, 1.0);
    }
private:
    static constexpr double kFastAttackMs = 2.0, kFastReleaseMs = 40.0;
    static constexpr double kSlowAttackMs = 10.0, kSlowReleaseMs = 400.0;
    double fastAttackAlpha = 0.0, fastReleaseAlpha = 0.0;
    double slowAttackAlpha = 0.0, slowReleaseAlpha = 0.0;
    double fast = 0.0, slow = 0.0;
};
}
