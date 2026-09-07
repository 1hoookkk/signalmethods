#pragma once

#include <array>

namespace hs
{
std::array<double, 2> lpcFormants (const std::array<float, 13>& reflection, double sampleRateHz);
}
