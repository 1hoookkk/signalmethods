#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

enum class SectionType { kOff, kLowPass, kEq };

enum class SectionEdit { kType, kFc, kBw, kGain };

struct SectionParam {
  SectionType type{SectionType::kOff};
  double fc_hz{};
  double bw_oct{};
  double gain_db{};
  double trench_hz{};
};

enum class Shape { kOff, kLow, kHigh, kPeak, kLowShelf, kHighShelf };

struct ShapeParam {
  Shape shape{Shape::kOff};
  double fc_hz{};
  double q{};
  double gain_db{};
  double trench_hz{};
};

inline constexpr double kShapeQMin = 0.1;
inline constexpr double kShapeQMax = 40.0;

inline constexpr double kTrenchMinOct = 2.0;
inline constexpr double kTrenchMaxOct = 7.0;

double trench_floor_radius();

ShapeParam shape_of(const PackedSection& words, double sample_rate_hz = kP2kDatumHz);

PackedSection words_from_shape(const ShapeParam& param, const PackedSection& current,
                               std::size_t section,
                               double sample_rate_hz = kP2kDatumHz);

SectionParam param_of(const PackedSection& words, double sample_rate_hz = kP2kDatumHz);

std::array<std::uint16_t, 4> words_from_param(const SectionParam& param,
                                              const std::array<std::uint16_t, 4>& current,
                                              std::size_t section,
                                              double sample_rate_hz = kP2kDatumHz);

std::array<std::uint16_t, 4> words_from_param_keeping_offset(
    const SectionParam& param, SectionEdit edit,
    const std::array<std::uint16_t, 4>& current, std::size_t section,
    double sample_rate_hz = kP2kDatumHz);

}  // namespace trench::core::p2k
