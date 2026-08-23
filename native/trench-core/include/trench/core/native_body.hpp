#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <variant>

#include "trench/core/packed_body.hpp"

namespace trench::core::native {

inline constexpr std::size_t kSections = 6;
inline constexpr std::size_t kCorners = 4;
inline constexpr double kPoleStabilityMargin = 1.0e-4;

struct Resonant {
  double hz{};
  double bw_hz{};
  bool operator==(const Resonant&) const = default;
};

struct RealRoots {
  double a_hz{};
  double b_hz{};
  bool operator==(const RealRoots&) const = default;
};

using Roots = std::variant<Resonant, RealRoots>;

struct Section {
  Roots pole{RealRoots{}};
  Roots zero{RealRoots{}};
  bool operator==(const Section&) const = default;
};

struct Corner {
  std::array<Section, kSections> sections{};
  double gain_db{};
  bool operator==(const Corner&) const = default;
};

struct Body {
  std::array<Corner, kCorners> corners{};
  bool operator==(const Body&) const = default;
};

struct Coefficients {
  double b1{};
  double b2{};
  double a1{};
  double a2{};
};

using Design = std::array<Coefficients, kSections>;

Roots roots_from_coefficients(double p, double q, double sample_rate_hz);
std::pair<double, double> coefficients_of(const Roots& roots, double sample_rate_hz);

Coefficients design(const Section& section, double sample_rate_hz);
Design design(const Corner& corner, double sample_rate_hz);
Design blend(const Body& body, double morph, double q, double sample_rate_hz);
double blend_gain_db(const Body& body, double morph, double q);

double dc_scale(const Coefficients& c);
Biquad biquad(const Coefficients& c);
Cascade cascade(const Design& d, double gain_db = 0.0);
bool is_stable(const Coefficients& c);
bool pole_is_conjugate(const Coefficients& c);

Section import_section(const PackedSection& words, double datum_hz);
double packed_corner_gain_db(std::span<const PackedSection> words);
Body import_p2k(std::span<const std::uint8_t> body, double datum_hz = kP2kDatumHz);

}  // namespace trench::core::native
