#pragma once

#include <array>
#include <utility>
#include <vector>

namespace hs
{
std::array<double, 2> lpcFormants (const std::array<float, 13>& reflection, double sampleRateHz);
std::vector<std::pair<double, double>> lpcResonances (const std::array<float, 13>& reflection, double sampleRateHz, int most);
std::vector<std::pair<double, double>> polynomialResonances (const std::vector<double>& coefficients, double sampleRateHz, int most);
std::vector<std::pair<double, double>> spectralLpcPoles (const std::vector<float>& mono, double sampleRateHz, int most);
}
