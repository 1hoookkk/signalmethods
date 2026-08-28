#pragma once

#include <optional>

#include "trench/core/packed_body.hpp"

namespace trench::core {

inline constexpr double kSubAnchorHz = 70.0;
inline constexpr double kZeroWallRadius = 0.8;
inline constexpr double kZeroWallCeiling = 0.45;

std::optional<double> conjugate_pair_hz(double c1, double c2, double sample_rate_hz);

double ratio_of_semitones(double semitones);

double transposed_root_hz(double hz, double radius, double ratio, double sample_rate_hz,
                          bool is_zero);

Biquad transpose_section(const Biquad& section, double ratio, double sample_rate_hz);

Cascade transpose_cascade(const Cascade& cascade, double ratio, double sample_rate_hz);

}  // namespace trench::core
