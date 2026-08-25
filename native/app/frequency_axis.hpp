#pragma once

#include <algorithm>
#include <cmath>

namespace trench::app::frequency_axis {

inline double low_hz(double sample_rate_hz) { return sample_rate_hz / 2048.0; }

inline double high_hz(double sample_rate_hz) { return sample_rate_hz / 2.0; }

inline double fraction(double hz, double sample_rate_hz) {
  const double low = low_hz(sample_rate_hz);
  const double high = high_hz(sample_rate_hz);
  const double clamped = std::clamp(hz, low, high);
  return std::log2(clamped / low) / std::log2(high / low);
}

inline double hz(double fraction, double sample_rate_hz) {
  const double low = low_hz(sample_rate_hz);
  const double high = high_hz(sample_rate_hz);
  return low * std::pow(high / low, std::clamp(fraction, 0.0, 1.0));
}

}  // namespace trench::app::frequency_axis
