#include "trench/core/transpose.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace trench::core {

namespace {

constexpr double kTau = 2.0 * std::numbers::pi;

bool root_travels(double hz, double radius, double ratio, double sample_rate_hz,
                  bool is_zero) {
  if (!is_zero) return hz >= kSubAnchorHz;
  return radius < kZeroWallRadius || hz * ratio < kZeroWallCeiling * sample_rate_hz;
}

std::optional<double> moved_lane_c1(double c1, double c2, double sample_rate_hz, double ratio,
                                    bool is_zero) {
  const double discriminant = c1 * c1 - 4.0 * c2;
  if (discriminant >= 0.0 || c2 <= 1.0e-18) return std::nullopt;
  const double radius = std::sqrt(c2);
  const double angle = std::acos(std::clamp(-c1 / (2.0 * radius), -1.0, 1.0));
  const double hz = angle * sample_rate_hz / kTau;
  if (!root_travels(hz, radius, ratio, sample_rate_hz, is_zero)) return std::nullopt;
  const double moved_hz = std::clamp(hz * ratio, 20.0, 0.49 * sample_rate_hz);
  return -2.0 * radius * std::cos(kTau * moved_hz / sample_rate_hz);
}

}

std::optional<double> conjugate_pair_hz(double c1, double c2, double sample_rate_hz) {
  const double discriminant = c1 * c1 - 4.0 * c2;
  if (discriminant >= 0.0 || c2 <= 1.0e-18) return std::nullopt;
  const double radius = std::sqrt(c2);
  const double angle = std::acos(std::clamp(-c1 / (2.0 * radius), -1.0, 1.0));
  return angle * sample_rate_hz / kTau;
}

double ratio_of_semitones(double semitones) { return std::pow(2.0, semitones / 12.0); }

double transposed_root_hz(double hz, double radius, double ratio, double sample_rate_hz,
                          bool is_zero) {
  if (ratio == 1.0) return hz;
  if (!root_travels(hz, radius, ratio, sample_rate_hz, is_zero)) return hz;
  return std::clamp(hz * ratio, 20.0, 0.49 * sample_rate_hz);
}

Biquad transpose_section(const Biquad& section, double ratio, double sample_rate_hz) {
  Biquad out = section;
  if (const auto pole = moved_lane_c1(section[3], section[4], sample_rate_hz, ratio, false)) {
    out[3] = *pole;
  }
  const double b0 = section[0];
  if (std::abs(b0) > 1.0e-12) {
    if (const auto zero = moved_lane_c1(section[1] / b0, section[2] / b0, sample_rate_hz, ratio, true)) {
      out[1] = *zero * b0;
    }
  }
  return out;
}

Cascade transpose_cascade(const Cascade& cascade, double ratio, double sample_rate_hz) {
  Cascade out = cascade;
  if (ratio == 1.0) return out;
  for (auto& section : out) section = transpose_section(section, ratio, sample_rate_hz);
  return out;
}

}
