#pragma once
#include <array>
#include <cmath>
#include <cstddef>
namespace trench
{
inline constexpr std::array<float, 33> kDeskCompensationDb {
    0.000f, 0.194f, 0.142f, 0.054f, -0.068f, -0.222f, -0.409f, -0.626f,
    -0.871f, -1.139f, -1.425f, -1.723f, -2.020f, -2.301f, -2.536f, -2.740f,
    -2.923f, -3.090f, -3.242f, -3.381f, -3.508f, -3.625f, -3.731f, -3.829f,
    -3.917f, -3.998f, -4.071f, -4.138f, -4.197f, -4.251f, -4.299f, -4.342f,
    -4.380f
};
inline float deskCompensationDb (float knob) noexcept
{
    knob = knob < 0.0f ? 0.0f : (knob > 1.0f ? 1.0f : knob);
    const float scaled = knob * (float) (kDeskCompensationDb.size() - 1);
    const auto lo = (std::size_t) scaled;
    if (lo >= kDeskCompensationDb.size() - 1)
        return kDeskCompensationDb.back();
    const float frac = scaled - (float) lo;
    return kDeskCompensationDb[lo] + frac * (kDeskCompensationDb[lo + 1] - kDeskCompensationDb[lo]);
}
inline float deskCompensationGain (float knob) noexcept
{
    return std::pow (10.0f, deskCompensationDb (knob) / 20.0f);
}
}
