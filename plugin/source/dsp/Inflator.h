#pragma once
#include <algorithm>
#include <cmath>
namespace trench
{
inline float inflatorCurve (float y) noexcept
{
    const float y2 = y * y;
    return 1.5f * y - 0.5f * y2 * y - 0.0625f * (y2 - 2.0f * y2 * y + y2 * y2);
}
inline float inflate (float x, float effect, float ceiling) noexcept
{
    const float e = std::clamp (effect, 0.0f, 1.0f);
    const float u = std::clamp (x / ceiling, -1.0f, 1.0f);
    const float shaped = std::copysign (inflatorCurve (std::abs (u)), u);
    return ceiling * (e * shaped + (1.0f - e) * u);
}
}
