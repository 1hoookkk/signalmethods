#pragma once

#include "parameters/Curves.h"
#include <atomic>

namespace trench::curves
{
// The dev bisection host drives one axis in its INTERNAL units, so that axis
// must not see the table it is being used to measure. Every other shipping
// axis remains mapped, including slamTrim while slam is bypassed.
inline std::atomic<int>& bypassAxis() noexcept
{
    static std::atomic<int> axis { -1 };
    return axis;
}

inline void setBypassAxis (Axis axis) noexcept
{
    bypassAxis().store ((int) axis, std::memory_order_relaxed);
}

inline void clearBypassAxis() noexcept
{
    bypassAxis().store (-1, std::memory_order_relaxed);
}

inline float curveMap (const Table& table, float knob) noexcept
{
    knob = knob < 0.0f ? 0.0f : (knob > 1.0f ? 1.0f : knob);
    const float scaled = knob * (float) (kTableSize - 1);
    const auto lo = (std::size_t) scaled;
    if (lo >= kTableSize - 1)
        return table[kTableSize - 1];
    const float frac = scaled - (float) lo;
    return table[lo] + frac * (table[lo + 1] - table[lo]);
}

inline float curveMap (Axis axis, float knob) noexcept
{
    if (bypassAxis().load (std::memory_order_relaxed) == (int) axis)
        return knob;
    return curveMap (*kTables[(std::size_t) axis], knob);
}
}
