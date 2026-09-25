#pragma once

#include "parameters/Curves.h"
#include <atomic>

namespace trench::curves
{
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

inline float uncurveMap (Axis axis, float value) noexcept
{
    if (bypassAxis().load (std::memory_order_relaxed) == (int) axis)
        return value;
    const auto& table = *kTables[(std::size_t) axis];
    if (value <= table[0])
        return 0.0f;
    for (std::size_t i = 1; i < kTableSize; ++i)
        if (value <= table[i])
        {
            const float span = table[i] - table[i - 1];
            const float frac = span > 0.0f ? (value - table[i - 1]) / span : 0.0f;
            return ((float) (i - 1) + frac) / (float) (kTableSize - 1);
        }
    return 1.0f;
}
}
