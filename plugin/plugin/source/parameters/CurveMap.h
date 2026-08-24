#pragma once

#include "parameters/Curves.h"
#include <atomic>

namespace trench::curves
{
// The dev bisection host drives an axis in its INTERNAL units, so it must not
// see the table it is being used to measure.
inline std::atomic<bool>& bypass() noexcept
{
    static std::atomic<bool> flag { false };
    return flag;
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
    if (bypass().load (std::memory_order_relaxed))
        return knob;
    return curveMap (*kTables[(std::size_t) axis], knob);
}
}
