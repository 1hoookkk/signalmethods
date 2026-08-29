#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

enum class Role { kParked, kTilt, kPeak, kNotch, kPeakNotch, kRealAxis };

struct RoleEnvelope {
  double pole_lo_hz{};
  double pole_hi_hz{};
  double pole_r_lo{};
  double pole_r_hi{};
  double zero_offset_oct_lo{};
  double zero_offset_oct_hi{};
  double zero_r_lo{};
  double zero_r_hi{};
};

Role role_of(const SectionGeometry& geometry);
Role role_of(const PackedSection& words, double sample_rate_hz = kP2kDatumHz);
const RoleEnvelope& envelope(Role role);
bool within_envelope(Role role, const SectionGeometry& geometry);
bool within_envelope(Role role, const PackedSection& words,
                     double sample_rate_hz = kP2kDatumHz);

using RoleIntent = std::array<std::optional<Role>, 6>;

std::array<std::uint16_t, 4> seat_words(Role role, const std::array<std::uint16_t, 4>& words,
                                        std::size_t section);

}
