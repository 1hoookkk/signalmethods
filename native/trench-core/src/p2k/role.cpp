#include "trench/core/role.hpp"

#include <cmath>
#include <variant>

#include "trench/core/p2k.hpp"

namespace trench::core::p2k {

namespace {

struct Roots {
  double pole_hz{};
  double pole_r{};
  double zero_hz{};
  double zero_r{};
  bool conjugate{};
};

Roots roots_of(const SectionGeometry& g) {
  Roots out;
  const auto* pole = std::get_if<ConjugatePair>(&g.pole);
  if (pole == nullptr) {
    return out;
  }
  out.pole_hz = pole->hz;
  out.pole_r = pole->radius;
  if (const auto* zero = std::get_if<ConjugatePair>(&g.zero)) {
    out.zero_hz = zero->hz;
    out.zero_r = zero->radius;
  } else if (std::holds_alternative<DegeneratePair>(g.zero)) {
    out.zero_hz = pole->hz;
    out.zero_r = 0.0;
  } else {
    return out;
  }
  out.conjugate = true;
  return out;
}

double octaves_apart(const Roots& r) {
  return std::abs(std::log2(std::max(r.zero_hz, 20.0) / std::max(r.pole_hz, 20.0)));
}

constexpr RoleEnvelope kEnvelopes[] = {
    {15000.0, 22050.0, 0.0, 1.0, 0.0, 10.0, 0.0, 1.0},
    {80.0, 19500.0, 0.75, kPoleRMax, 2.0, 8.0, 0.4, 1.0},
    {300.0, 18000.0, 0.70, kPoleRMax, 0.0, 2.0, 0.0, 0.97},
    {20.0, 20000.0, 0.0, 0.5, 0.0, 10.0, 0.85, 1.0},
    {400.0, 18500.0, 0.55, kPoleRMax, 0.0, 2.0, 0.97, 1.0},
    {0.0, 22050.0, 0.0, 1.0, 0.0, 10.0, 0.0, 1.0},
};

}  // namespace

Role role_of(const SectionGeometry& geometry) {
  const auto r = roots_of(geometry);
  if (!r.conjugate) {
    return Role::kRealAxis;
  }
  if (r.pole_hz > 15000.0 && r.zero_hz > 15000.0) {
    return Role::kParked;
  }
  if (r.zero_r < 0.5) {
    return Role::kPeak;
  }
  if (r.pole_r < 0.5) {
    return Role::kNotch;
  }
  if (octaves_apart(r) > 2.0) {
    return Role::kTilt;
  }
  return r.zero_r > 0.97 ? Role::kPeakNotch : Role::kPeak;
}

Role role_of(const PackedSection& words, double sample_rate_hz) {
  return role_of(geometry_from_words(words, sample_rate_hz));
}

const RoleEnvelope& envelope(Role role) {
  return kEnvelopes[static_cast<std::size_t>(role)];
}

bool within_envelope(Role role, const SectionGeometry& geometry) {
  const auto r = roots_of(geometry);
  if (role == Role::kRealAxis) {
    return !r.conjugate;
  }
  if (!r.conjugate) {
    return false;
  }
  const auto& e = envelope(role);
  const double oct = octaves_apart(r);
  return r.pole_hz >= e.pole_lo_hz && r.pole_hz <= e.pole_hi_hz && r.pole_r >= e.pole_r_lo &&
         r.pole_r <= e.pole_r_hi && oct >= e.zero_offset_oct_lo && oct <= e.zero_offset_oct_hi &&
         r.zero_r >= e.zero_r_lo && r.zero_r <= e.zero_r_hi;
}

bool within_envelope(Role role, const PackedSection& words, double sample_rate_hz) {
  return within_envelope(role, geometry_from_words(words, sample_rate_hz));
}

}  // namespace trench::core::p2k
