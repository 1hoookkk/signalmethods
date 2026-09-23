#pragma once
#include <cmath>
namespace trench
{
inline float driveLaw (float x, float amount) noexcept
{
    const double a = amount < 0.0f ? 0.0 : (amount > 1.0f ? 1.0 : (double) amount);
    const double v = (double) x;
    const double m = std::abs (v);
    if (m <= 1.0)
        return (float) (v + a * 0.5 * v * (1.0 - v * v));
    return (float) std::copysign (m - a * (m - 1.0), v);
}
inline constexpr double kDrivePushDb = 12.0;
inline float drivePush (float amount) noexcept
{
    const double a = amount < 0.0f ? 0.0 : (amount > 1.0f ? 1.0 : (double) amount);
    return (float) std::pow (10.0, kDrivePushDb * a / 20.0);
}
inline float driveMakeup (float filtered, float amount) noexcept
{
    return driveLaw (filtered, amount) / drivePush (amount);
}
}
