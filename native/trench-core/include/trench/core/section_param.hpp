#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

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

struct PoleReading {
  double hz{};
  double bw_hz{};
};

struct MaskParam {
  double offset_oct{};
  double zero_bw_hz{};
};

inline constexpr double kTrenchMinOct = 2.0;
inline constexpr double kTrenchMaxOct = 7.0;

inline constexpr double kMaskOffsetMinOct = -3.0;
inline constexpr double kMaskOffsetMaxOct = 6.0;
inline constexpr double kMaskWidthMaxHz = 2000.0;
inline constexpr double kPlacedPoleBwHz = 120.0;

constexpr double mask_offset_min_oct(std::size_t section) {
  return section == 5 ? kTrenchMinOct : kMaskOffsetMinOct;
}

constexpr double mask_offset_max_oct(std::size_t section) {
  return section == 5 ? kTrenchMaxOct : kMaskOffsetMaxOct;
}

double trench_floor_radius();

double mask_width_floor_hz(double sample_rate_hz = kP2kDatumHz);

std::optional<PoleReading> pole_of(const PackedSection& words,
                                   double sample_rate_hz = kP2kDatumHz);

MaskParam mask_of(const PackedSection& words, std::size_t section,
                  double sample_rate_hz = kP2kDatumHz);

std::array<std::uint16_t, 4> words_from_pole(double hz, double bw_hz,
                                             const std::array<std::uint16_t, 4>& current,
                                             std::size_t section,
                                             double sample_rate_hz = kP2kDatumHz);

std::array<std::uint16_t, 4> words_from_mask(const MaskParam& mask,
                                             const std::array<std::uint16_t, 4>& current,
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
