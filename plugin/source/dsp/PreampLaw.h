#pragma once
#include <algorithm>
#include <cmath>
namespace trench
{
inline float driveTaper (float k) noexcept
{
    k = std::clamp (k, 0.0f, 1.0f);
    return k <= 0.0f ? 0.0f : (std::pow (10.0f, 2.0f * k) - 1.0f) * (1.0f / 99.0f);
}
inline float preampGain (float k) noexcept
{
    return 1.0f + 99.0f * driveTaper (k);
}
}
