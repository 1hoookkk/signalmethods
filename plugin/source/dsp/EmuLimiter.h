#pragma once
#include <algorithm>
#include <cmath>
namespace trench
{
class EmuLimiter
{
public:
    void prepare (double sampleRate) noexcept
    {
        const double perSample = 44'100.0 / std::max (8'000.0, sampleRate);
        attack = std::pow (kAttack, perSample);
        release = std::pow (kRelease, perSample);
        reset();
    }
    void reset() noexcept { gain = 1.0; }
    void process (float& left, float* right) noexcept
    {
        const double peak = std::max (std::abs ((double) left), right != nullptr ? std::abs ((double) *right) : 0.0);
        gain = peak * gain > kThreshold ? gain * attack : std::min (1.0, gain * release);
        left = (float) (left * gain);
        if (right != nullptr)
            *right = (float) (*right * gain);
    }
    double currentGain() const noexcept { return gain; }
    static constexpr double kThreshold = 0.63;
    static constexpr double kAttack = 0.999;
    static constexpr double kRelease = 1.0 / 0.9999;
private:
    double attack = kAttack, release = kRelease, gain = 1.0;
};
}
