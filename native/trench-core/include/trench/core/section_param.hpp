#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

enum class SectionType { kOff, kLowPass, kHighPass, kEq };

enum class SectionEdit { kType, kFc, kBw, kGain };

struct SectionParam {
  SectionType type{SectionType::kOff};
  double fc_hz{};
  double bw_oct{};
  double gain_db{};
};

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
