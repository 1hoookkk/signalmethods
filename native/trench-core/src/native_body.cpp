#include "trench/core/native_body.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace trench::core::native {

namespace {

constexpr double kTau = 2.0 * std::numbers::pi;
constexpr double kInf = std::numeric_limits<double>::infinity();

double real_decay_hz(double root, double sample_rate_hz) {
  if (root == 0.0) {
    return kInf;
  }
  const double decay = -std::log(std::abs(root)) * sample_rate_hz / kTau;
  return root < 0.0 ? -decay : decay;
}

double real_root(double decay_hz, double sample_rate_hz) {
  const double magnitude = std::exp(-kTau * std::abs(decay_hz) / sample_rate_hz);
  return decay_hz < 0.0 ? -magnitude : magnitude;
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
  return {b1, b2, a1, a2};
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

}  // namespace

Design blend(const Body& body, double morph, double q, double sample_rate_hz) {
  const auto weight = corner_weights(morph, q);
  std::array<Design, kCorners> designs{};
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    designs[ci] = design(body.corners[ci], sample_rate_hz);
  }
  Design out{};
  for (std::size_t si = 0; si < kSections; ++si) {
    Coefficients acc{};
    for (std::size_t ci = 0; ci < kCorners; ++ci) {
      const auto& c = designs[ci][si];
      acc.b1 += weight[ci] * c.b1;
      acc.b2 += weight[ci] * c.b2;
      acc.a1 += weight[ci] * c.a1;
      acc.a2 += weight[ci] * c.a2;
    }
    out[si] = acc;
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

double dc_scale(const Coefficients& c) { return (1.0 + c.a1 + c.a2) / (1.0 + c.b1 + c.b2); }

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

Section import_section(const PackedSection& words, double datum_hz) {
  const auto pair = [&](std::size_t mag, std::size_t rsq) {
    const double d_mag = decode_word(words[mag]);
    const double d_rsq = decode_word(words[rsq]);
    return roots_from_coefficients(4.0 * d_mag + d_rsq - 2.0, 1.0 - d_rsq, datum_hz);
  };
  return {pair(2, 3), pair(0, 1)};
}

double packed_corner_gain_db(std::span<const PackedSection> words) {
  double h = 1.0;
  for (const auto& section : words) {
    const auto b = section_words_to_biquad(section);
    h *= (b[0] + b[1] + b[2]) / (1.0 + b[3] + b[4]);
  }
  return 20.0 * std::log10(std::abs(h));
}

Body import_p2k(std::span<const std::uint8_t> body, double datum_hz) {
  const auto packed = PackedBody::from_legacy_bytes(body);
  Body out{};
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    for (std::size_t si = 0; si < kSections; ++si) {
      out.corners[ci].sections[si] = import_section(packed.words[ci][si], datum_hz);
    }
    out.corners[ci].gain_db = packed_corner_gain_db(
        std::span<const PackedSection>(packed.words[ci]).first(kSections));
  }
  return out;
}

}  // namespace trench::core::native
