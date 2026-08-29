#include "trench/core/native_body.hpp"

#include "trench/core/p2k.hpp"

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
const double kPackedScaleFloor = 4.0 * decode_word(1);

double capped(double decay_hz) { return std::min(std::abs(decay_hz), kDecayCapHz); }

double log_one_minus_radius(double decay_hz, double scale, double sample_rate_hz) {
  if (!std::isfinite(decay_hz)) return 0.0;
  const double exponent = -scale * std::abs(decay_hz) / sample_rate_hz;
  return std::log(-std::expm1(exponent));
}

double decay_from_log_one_minus_radius(double encoded, double scale,
                                       double sample_rate_hz) {
  const double radius = -std::expm1(std::min(encoded, 0.0));
  if (radius <= 0.0) return kInf;
  return -std::log(radius) * sample_rate_hz / scale;
}

struct OrderedRealRoot {
  double decay_hz{};
  bool negative{};
};

std::array<OrderedRealRoot, 2> ordered_real_roots(const RealRoots& real) {
  std::array<OrderedRealRoot, 2> out{{
      {capped(real.a_hz), std::signbit(real.a_hz)},
      {capped(real.b_hz), std::signbit(real.b_hz)},
  }};
  std::stable_sort(out.begin(), out.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.decay_hz < rhs.decay_hz;
  });
  return out;
}

Resonant as_resonant(const Roots& roots) {
  if (const auto* res = std::get_if<Resonant>(&roots)) {
    return *res;
  }
  const auto& real = std::get<RealRoots>(roots);
  const bool negative_angle = std::signbit(real.a_hz) && std::signbit(real.b_hz);
  return {negative_angle ? kAnglePiHz : kAngleZeroHz,
          capped(real.a_hz) + capped(real.b_hz)};
}

Roots blend_roots(const std::array<const Roots*, kCorners>& corner,
                  const std::array<double, kCorners>& weight,
                  double sample_rate_hz) {
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
    std::array<double, 2> encoded{};
    for (std::size_t ci = 0; ci < kCorners; ++ci) {
      if (weight[ci] == 0.0) continue;
      const auto ordered = ordered_real_roots(std::get<RealRoots>(*corner[ci]));
      for (std::size_t ri = 0; ri < ordered.size(); ++ri) {
        encoded[ri] += weight[ci] *
                       log_one_minus_radius(ordered[ri].decay_hz, kTau,
                                            sample_rate_hz);
      }
    }
    const auto sign_of = ordered_real_roots(std::get<RealRoots>(*corner[nearest]));
    const auto decoded = [&](std::size_t ri) {
      const double decay =
          decay_from_log_one_minus_radius(encoded[ri], kTau, sample_rate_hz);
      return sign_of[ri].negative ? -decay : decay;
    };
    return RealRoots{decoded(0), decoded(1)};
  }
  double log_hz = 0.0;
  double encoded_radius = 0.0;
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    if (weight[ci] == 0.0) continue;
    const Resonant res = as_resonant(*corner[ci]);
    log_hz += weight[ci] * std::log(std::max(res.hz, kAngleZeroHz));
    encoded_radius += weight[ci] *
                      log_one_minus_radius(res.bw_hz, std::numbers::pi,
                                           sample_rate_hz);
  }
  return Resonant{
      std::exp(log_hz),
      decay_from_log_one_minus_radius(encoded_radius, std::numbers::pi,
                                      sample_rate_hz)};
}

}  // namespace

Design blend_roots_log_2019(const Body& body, double morph, double q, double sample_rate_hz) {
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
    out[si] = design(Section{blend_roots(poles, weight, sample_rate_hz),
                             blend_roots(zeros, weight, sample_rate_hz), stabilised},
                     sample_rate_hz);
  }
  return out;
}

double blend_gain_db_2019(const Body& body, double morph, double q) {
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

Corner import_p2k_corner(const P2kCorner& words, double datum_hz) {
  Corner out{};
  for (std::size_t si = 0; si < kSections; ++si) {
    out.sections[si] = import_section(words[si], datum_hz);
  }
  double gain_db = 0.0;
  for (std::size_t si = 0; si < kSections; ++si) {
    const auto b = section_words_to_biquad(words[si]);
    const double k = dc_scale(design(out.sections[si], datum_hz));
    gain_db += 20.0 * (std::log10(std::max(std::abs(b[0]), kPackedScaleFloor)) -
                       std::log10(std::abs(k)));
  }
  out.gain_db = gain_db;
  return out;
}

Body import_p2k(std::span<const std::uint8_t> body, double datum_hz) {
  const auto packed = PackedBody::from_legacy_bytes(body);
  Body out{};
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    P2kCorner words{};
    std::copy_n(packed.words[ci].begin(), kSections, words.begin());
    out.corners[ci] = import_p2k_corner(words, datum_hz);
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
  return out;
}

P2kCorner export_p2k_corner(const Corner& corner, double datum_hz) {
  p2k::CornerWords roots{};
  for (std::size_t si = 0; si < kSections; ++si) {
    const auto quantise = [datum_hz](const Roots& value) {
      if (const auto* real = std::get_if<RealRoots>(&value);
          real != nullptr && !std::isfinite(real->a_hz) &&
          !std::isfinite(real->b_hz)) {
        return std::pair{kIdentitySection[0], kIdentitySection[1]};
      }
      const auto [p, q] = coefficients_of(value, datum_hz);
      const auto& lattice = p2k::lattice_decoded();
      const auto& words = p2k::lattice_words();
      const auto radius_index = p2k::nearest_lattice_word(
          encode_word(std::clamp(1.0 - q, 0.0, 1.0)));
      const double radius_word = lattice[radius_index];
      const auto magnitude_index = p2k::nearest_lattice_word(
          encode_word(std::clamp((p + 2.0 - radius_word) / 4.0, 0.0, 1.0)));
      return std::pair{words[magnitude_index], words[radius_index]};
    };
    auto [zero_mag, zero_radius] = quantise(corner.sections[si].zero);
    const auto* real_zero = std::get_if<RealRoots>(&corner.sections[si].zero);
    const bool zero_is_parked = real_zero != nullptr && !std::isfinite(real_zero->a_hz) &&
                                !std::isfinite(real_zero->b_hz);
    if (si == kSections - 1 && !zero_is_parked) {
      zero_radius = p2k::kS6ZeroRsqWord;
      if (const auto* res = std::get_if<Resonant>(&corner.sections[si].zero)) {
        const double d_rsq = decode_word(zero_radius);
        const double p = -2.0 * std::sqrt(1.0 - d_rsq) *
                         std::cos(kTau * res->hz / datum_hz);
        zero_mag = p2k::lattice_words()[p2k::nearest_lattice_word(
            encode_word(std::clamp((p + 2.0 - d_rsq) / 4.0, 0.0, 1.0)))];
      }
    }
    const auto [pole_mag, pole_radius] = quantise(corner.sections[si].pole);
    roots[si] = {zero_mag, zero_radius, pole_mag, pole_radius};
  }
  roots = p2k::enter(roots);

  double dc_ratio = 1.0;
  for (const auto& row : roots) {
    auto [numerator, denominator] = p2k::dc_terms(row);
    if (std::abs(numerator) < 1.0e-15) {
      numerator = std::copysign(1.0e-15, numerator == 0.0 ? 1.0 : numerator);
    }
    dc_ratio *= denominator / numerator;
  }
  const double corner_gain = std::pow(10.0, corner.gain_db / 20.0);
  const double stage_scale =
      std::pow(std::abs(dc_ratio * corner_gain), 1.0 / static_cast<double>(kSections));
  const auto scale_word = p2k::nearest_gain_word(stage_scale);

  P2kCorner out{};
  for (std::size_t si = 0; si < kSections; ++si) {
    std::copy(roots[si].begin(), roots[si].end(), out[si].begin());
    out[si][4] = scale_word;
  }
  return out;
}

Corner packed_interior_corner(const PackedBody& packed, double morph, double q,
                              double datum_hz) {
  const auto words = packed.interpolate_words(static_cast<float>(morph),
                                              static_cast<float>(q), 0.0F);
  P2kCorner corner{};
  for (std::size_t si = 0; si < kSections; ++si) corner[si] = words[si];
  return import_p2k_corner(corner, datum_hz);
}

PackedBody export_p2k_body(const Body& body, double datum_hz) {
  PackedBody out{};
  for (auto& corner : out.words) corner.fill(kIdentitySection);
  for (std::size_t ci = 0; ci < kCorners; ++ci) {
    const auto words = export_p2k_corner(body.corners[ci], datum_hz);
    std::copy(words.begin(), words.end(), out.words[ci].begin());
    std::copy(words.begin(), words.end(), out.words[ci + kLegacyCornerCount].begin());
  }
  return out;
}

std::array<std::uint8_t, kLegacyBodyBytes> export_p2k(const Body& body,
                                                       double datum_hz) {
  return export_p2k_body(body, datum_hz).legacy_bytes();
}

}  // namespace trench::core::native
