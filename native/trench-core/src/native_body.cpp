#include "trench/core/native_body.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace trench::core::native {

namespace {

constexpr double kTau = 2.0 * std::numbers::pi;
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kDcZeroEpsilon = 1.0e-9;

double real_decay_hz(double root, double sample_rate_hz) {
  if (root == 0.0) {
    return kInf;
  }
  const double decay = std::abs(std::log(std::abs(root))) * sample_rate_hz / kTau;
  return root < 0.0 ? -decay : decay;
}

double real_root(double decay_hz, double sample_rate_hz) {
  const double magnitude = std::exp(-kTau * std::abs(decay_hz) / sample_rate_hz);
  return std::signbit(decay_hz) ? -magnitude : magnitude;
}

std::pair<double, double> pole_coefficients(const Roots& roots, double sample_rate_hz) {
  const double limit = 1.0 - kPoleStabilityMargin;
  if (const auto* res = std::get_if<Resonant>(&roots)) {
    const double r = std::min(std::exp(-std::numbers::pi * res->bw_hz / sample_rate_hz), limit);
    return {-2.0 * r * std::cos(kTau * res->hz / sample_rate_hz), r * r};
  }
  const auto& real = std::get<RealRoots>(roots);
  const double a = std::clamp(real_root(real.a_hz, sample_rate_hz), -limit, limit);
  const double b = std::clamp(real_root(real.b_hz, sample_rate_hz), -limit, limit);
  return {-(a + b), a * b};
}

}  // namespace

Roots roots_from_coefficients(double p, double q, double sample_rate_hz) {
  const double discriminant = p * p - 4.0 * q;
  if (discriminant < 0.0) {
    const double r = std::sqrt(q);
    const double cosine = std::clamp(-p / (2.0 * r), -1.0, 1.0);
    return Resonant{std::acos(cosine) / kTau * sample_rate_hz,
                    -std::log(r) * sample_rate_hz / std::numbers::pi};
  }
  const double root = std::sqrt(discriminant);
  return RealRoots{real_decay_hz((-p + root) / 2.0, sample_rate_hz),
                   real_decay_hz((-p - root) / 2.0, sample_rate_hz)};
}

std::pair<double, double> coefficients_of(const Roots& roots, double sample_rate_hz) {
  if (const auto* res = std::get_if<Resonant>(&roots)) {
    const double r = std::exp(-std::numbers::pi * res->bw_hz / sample_rate_hz);
    return {-2.0 * r * std::cos(kTau * res->hz / sample_rate_hz), r * r};
  }
  const auto& real = std::get<RealRoots>(roots);
  const double a = real_root(real.a_hz, sample_rate_hz);
  const double b = real_root(real.b_hz, sample_rate_hz);
  return {-(a + b), a * b};
}

Coefficients design(const Section& section, double sample_rate_hz) {
  const auto [b1, b2] = coefficients_of(section.zero, sample_rate_hz);
  const auto [a1, a2] = pole_coefficients(section.pole, sample_rate_hz);
  return {b1, b2, a1, a2, section.dc_stabilised};
}

Design design(const Corner& corner, double sample_rate_hz) {
  Design out{};
  for (std::size_t si = 0; si < kSections; ++si) {
    out[si] = design(corner.sections[si], sample_rate_hz);
  }
  return out;
}

namespace {

std::array<double, kCorners> corner_weights(double morph, double q) {
  const double m = std::clamp(morph, 0.0, 1.0);
  const double qq = std::clamp(q, 0.0, 1.0);
  return {(1.0 - m) * (1.0 - qq), m * (1.0 - qq), (1.0 - m) * qq, m * qq};
}


constexpr double kAngleZeroHz = 20.0;
constexpr double kAnglePiHz = 20'000.0;
constexpr double kDecayCapHz = 1.0e9;
constexpr double kBwFloorHz = 1.0e-9;
const double kPackedScaleFloor = 4.0 * decode_word(1);

double capped(double decay_hz) { return std::min(std::abs(decay_hz), kDecayCapHz); }

Resonant as_resonant(const Roots& roots) {
  if (const auto* res = std::get_if<Resonant>(&roots)) {
    return *res;
  }
  const auto& real = std::get<RealRoots>(roots);
  return {std::signbit(real.a_hz) ? kAnglePiHz : kAngleZeroHz,
          capped(real.a_hz) + capped(real.b_hz)};
}

Roots blend_roots(const std::array<const Roots*, kCorners>& corner,
                  const std::array<double, kCorners>& weight) {
  std::size_t nearest = 0;
  for (std::size_t ci = 1; ci < kCorners; ++ci) {
    if (weight[ci] > weight[nearest]) nearest = ci;
  }
  if (weight[nearest] == 1.0) {
    return *corner[nearest];
  }
  bool all_real = true;
  for (const Roots* r : corner) {
    all_real = all_real && std::holds_alternative<RealRoots>(*r);
  }
  if (all_real) {
    double lo = 0.0;
    double hi = 0.0;
    for (std::size_t ci = 0; ci < kCorners; ++ci) {
      const auto& real = std::get<RealRoots>(*corner[ci]);
      const double a = capped(real.a_hz);
      const double b = capped(real.b_hz);
      lo += weight[ci] * std::log(std::max(std::min(a, b), kBwFloorHz));
      hi += weight[ci] * std::log(std::max(std::max(a, b), kBwFloorHz));
    }
    const auto& sign_of = std::get<RealRoots>(*corner[nearest]);
    const double sa = std::signbit(std::min(sign_of.a_hz, sign_of.b_hz)) ? -1.0 : 1.0;
    const double sb = std::signbit(std::max(sign_of.a_hz, sign_of.b_hz)) ? -1.0 : 1.0;
    return RealRoots{sa * std::exp(lo), sb * std::exp(hi)};
  }
  double log_hz = 0.0;
  double log_bw = 0.0;
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    const Resonant res = as_resonant(*corner[ci]);
    log_hz += weight[ci] * std::log(std::max(res.hz, kAngleZeroHz));
    log_bw += weight[ci] * std::log(std::max(res.bw_hz, kBwFloorHz));
  }
  return Resonant{std::exp(log_hz), std::exp(log_bw)};
}

}  // namespace

Design blend(const Body& body, double morph, double q, double sample_rate_hz) {
  const auto weight = corner_weights(morph, q);
  Design out{};
  for (std::size_t si = 0; si < kSections; ++si) {
    std::array<const Roots*, kCorners> poles{};
    std::array<const Roots*, kCorners> zeros{};
    for (std::size_t ci = 0; ci < kCorners; ++ci) {
      poles[ci] = &body.corners[ci].sections[si].pole;
      zeros[ci] = &body.corners[ci].sections[si].zero;
    }
    bool stabilised = true;
    for (std::size_t ci = 0; ci < kCorners; ++ci) {
      stabilised = stabilised && body.corners[ci].sections[si].dc_stabilised;
    }
    out[si] = design(Section{blend_roots(poles, weight), blend_roots(zeros, weight), stabilised},
                     sample_rate_hz);
  }
  return out;
}

double blend_gain_db(const Body& body, double morph, double q) {
  const auto weight = corner_weights(morph, q);
  double acc = 0.0;
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    acc += weight[ci] * body.corners[ci].gain_db;
  }
  return acc;
}

double dc_scale(const Coefficients& c) {
  return c.dc_stabilised ? (1.0 + c.a1 + c.a2) / (1.0 + c.b1 + c.b2) : 1.0;
}

Biquad biquad(const Coefficients& c) {
  const double g = dc_scale(c);
  return {g, g * c.b1, g * c.b2, c.a1, c.a2};
}

Cascade cascade(const Design& d, double gain_db) {
  Cascade out{};
  for (auto& s : out) {
    s = {1.0, 0.0, 0.0, 0.0, 0.0};
  }
  for (std::size_t si = 0; si < kSections; ++si) {
    out[si] = biquad(d[si]);
  }
  const double gain = std::pow(10.0, gain_db / 20.0);
  for (std::size_t k = 0; k < 3; ++k) {
    out[0][k] *= gain;
  }
  return out;
}

bool is_stable(const Coefficients& c) {
  return std::abs(c.a2) < 1.0 && std::abs(c.a1) < 1.0 + c.a2;
}

bool pole_is_conjugate(const Coefficients& c) { return c.a1 * c.a1 < 4.0 * c.a2; }

bool pole_is_marginal(const Coefficients& c) {
  const double limit = 1.0 - kPoleStabilityMargin;
  return std::abs(c.a2) >= limit * limit * (1.0 - 1.0e-12);
}

Section import_section(const PackedSection& words, double datum_hz) {
  const auto pair = [&](std::size_t mag, std::size_t rsq) {
    const double d_mag = decode_word(words[mag]);
    const double d_rsq = decode_word(words[rsq]);
    return roots_from_coefficients(4.0 * d_mag + d_rsq - 2.0, 1.0 - d_rsq, datum_hz);
  };
  const auto b = section_words_to_biquad(words);
  const bool zero_at_dc = std::abs(b[0] + b[1] + b[2]) < kDcZeroEpsilon * std::abs(b[0]);
  return {pair(2, 3), pair(0, 1), !zero_at_dc};
}

Body import_p2k(std::span<const std::uint8_t> body, double datum_hz) {
  const auto packed = PackedBody::from_legacy_bytes(body);
  Body out{};
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    for (std::size_t si = 0; si < kSections; ++si) {
      out.corners[ci].sections[si] = import_section(packed.words[ci][si], datum_hz);
    }
  }
  for (std::size_t si = 0; si < kSections; ++si) {
    bool stabilised = true;
    for (const auto& corner : out.corners) {
      stabilised = stabilised && corner.sections[si].dc_stabilised;
    }
    for (auto& corner : out.corners) {
      corner.sections[si].dc_stabilised = stabilised;
    }
  }
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    double gain_db = 0.0;
    for (std::size_t si = 0; si < kSections; ++si) {
      const auto b = section_words_to_biquad(packed.words[ci][si]);
      const double k = dc_scale(design(out.corners[ci].sections[si], datum_hz));
      gain_db += 20.0 * (std::log10(std::max(std::abs(b[0]), kPackedScaleFloor)) -
                         std::log10(std::abs(k)));
    }
    out.corners[ci].gain_db = gain_db;
  }
  return out;
}

}  // namespace trench::core::native
