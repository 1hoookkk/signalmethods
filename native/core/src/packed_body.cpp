#include "trench/core/packed_body.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace trench::core {
namespace {

constexpr double kTau = 2.0 * std::numbers::pi;

std::uint16_t read_u16_le(std::span<const std::uint8_t> bytes, std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

void write_u16_le(std::uint16_t value, std::span<std::uint8_t> bytes,
                  std::size_t offset) {
  bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

std::pair<double, double> pair_coefficients(const RootPair& pair,
                                            double sample_rate_hz) {
  if (const auto* conjugate = std::get_if<ConjugatePair>(&pair)) {
    const auto angle = kTau * conjugate->hz / sample_rate_hz;
    return {-2.0 * conjugate->radius * std::cos(angle),
            conjugate->radius * conjugate->radius};
  }
  if (const auto* real = std::get_if<RealPair>(&pair)) {
    return {-(real->root_a + real->root_b), real->root_a * real->root_b};
  }
  return {0.0, 0.0};
}

RootPair pair_geometry(double encoded_magnitude, double encoded_radius_squared,
                       double sample_rate_hz) {
  const double q = 1.0 - encoded_radius_squared;
  const double c = 4.0 * encoded_magnitude + encoded_radius_squared;
  const double p = c - 2.0;
  if (p == 0.0 && q == 0.0) {
    return DegeneratePair{};
  }
  const double discriminant = p * p - 4.0 * q;
  if (discriminant < 0.0) {
    const double radius = std::sqrt(q);
    const double cosine = -p / (2.0 * radius);
    return ConjugatePair{std::acos(cosine) / kTau * sample_rate_hz, radius};
  }
  const double root = std::sqrt(discriminant);
  return RealPair{(-p + root) / 2.0, (-p - root) / 2.0};
}

}

double decode_word(std::uint16_t word) {
  const std::uint32_t u = static_cast<std::uint32_t>(word) + 1U;
  if (u == 65'536U) return 1.0;
  if (u == 1U) return 0.0;
  const auto exponent = static_cast<int>((u >> 12U) & 0xFU);
  const auto mantissa = static_cast<double>(u & 0xFFFU);
  const double x = exponent == 0 ? mantissa / 4096.0
                                 : (mantissa + 4096.0) / 8192.0;
  return std::ldexp(x, exponent - 15);
}

std::uint16_t encode_word(double value) {
  if (value >= 1.0) return 0xFFFF;
  if (value <= 0.0) return 0x0000;
  const auto denormal_mantissa = static_cast<std::int64_t>(std::round(value * 134'217'728.0));
  if (denormal_mantissa > 0 && denormal_mantissa <= 0xFFF) {
    return static_cast<std::uint16_t>(denormal_mantissa - 1);
  }
  int stored_exponent = std::min(static_cast<int>(std::floor(std::log2(value))) + 1, 0);
  if (stored_exponent < -14) return 0x0000;
  int biased_exponent = stored_exponent + 15;
  auto mantissa_with_hidden = static_cast<std::int64_t>(
      std::round(value / std::ldexp(1.0, stored_exponent - 13)));
  if (mantissa_with_hidden >= 0x2000) {
    if (stored_exponent < 0) {
      ++stored_exponent;
      ++biased_exponent;
      mantissa_with_hidden = static_cast<std::int64_t>(
          std::round(value / std::ldexp(1.0, stored_exponent - 13)));
      const auto mantissa = std::min<std::int64_t>(mantissa_with_hidden & 0xFFF, 0xFFF);
      return static_cast<std::uint16_t>((biased_exponent << 12) | mantissa) - 1U;
    }
    return 0xFFFF;
  }
  const auto mantissa = std::clamp<std::int64_t>(mantissa_with_hidden - 0x1000, 0, 0xFFF);
  return static_cast<std::uint16_t>((biased_exponent << 12) | mantissa) - 1U;
}

std::uint16_t interpolate_word(std::uint16_t a, std::uint16_t b, float fraction) {
  const float difference = static_cast<float>(static_cast<std::int32_t>(b) -
                                               static_cast<std::int32_t>(a));
  const auto delta = static_cast<std::int32_t>(difference * fraction);
  return static_cast<std::uint16_t>(static_cast<std::int32_t>(a) + delta);
}

Biquad section_words_to_biquad(const PackedSection& words) {
  const double d0 = decode_word(words[0]);
  const double d1 = decode_word(words[1]);
  const double d2 = decode_word(words[2]);
  const double d3 = decode_word(words[3]);
  const double d4 = decode_word(words[4]);
  const double c0 = 4.0 * d0 + d1;
  const double c1 = d1;
  const double c2 = 4.0 * d2 + d3;
  const double c3 = d3;
  const double c4 = 4.0 * d4;
  return {c4, (c0 - 2.0) * c4, (1.0 - c1) * c4,
          c2 - 2.0, 1.0 - c3};
}

SectionGeometry geometry_from_words(const PackedSection& words,
                                    double sample_rate_hz) {
  return {
      pair_geometry(decode_word(words[2]), decode_word(words[3]), sample_rate_hz),
      pair_geometry(decode_word(words[0]), decode_word(words[1]), sample_rate_hz),
      4.0 * decode_word(words[4]),
  };
}

PackedSection words_from_geometry(const SectionGeometry& geometry,
                                  double sample_rate_hz) {
  const auto [zero_p, zero_q] = pair_coefficients(geometry.zero, sample_rate_hz);
  const auto [pole_p, pole_q] = pair_coefficients(geometry.pole, sample_rate_hz);
  return {
      encode_word((zero_p + 1.0 + zero_q) / 4.0),
      encode_word(1.0 - zero_q),
      encode_word((pole_p + 1.0 + pole_q) / 4.0),
      encode_word(1.0 - pole_q),
      encode_word(geometry.scale / 4.0),
  };
}

PackedBody PackedBody::from_native_bytes(std::span<const std::uint8_t> bytes) {
  if (bytes.size() != kNativeBodyBytes) {
    throw std::invalid_argument("native body must be exactly 560 bytes");
  }
  PackedBody body;
  std::size_t offset = 0;
  for (auto& corner : body.words) {
    for (auto& section : corner) {
      for (auto& word : section) {
        word = read_u16_le(bytes, offset);
        offset += 2;
      }
    }
  }
  return body;
}

PackedBody PackedBody::from_legacy_bytes(std::span<const std::uint8_t> bytes) {
  if (bytes.size() != kLegacyBodyBytes) {
    throw std::invalid_argument("legacy body must be exactly 240 bytes");
  }
  PackedBody body;
  for (auto& corner : body.words) corner.fill(kIdentitySection);
  std::size_t offset = 0;
  for (std::size_t corner = 0; corner < kLegacyCornerCount; ++corner) {
    for (std::size_t section = 0; section < kLegacySectionCount; ++section) {
      for (auto& word : body.words[corner][section]) {
        word = read_u16_le(bytes, offset);
        offset += 2;
      }
      body.words[corner + kLegacyCornerCount][section] = body.words[corner][section];
    }
  }
  return body;
}

PackedBody PackedBody::from_body_bytes(std::span<const std::uint8_t> bytes) {
  if (bytes.size() == kNativeBodyBytes) return from_native_bytes(bytes);
  if (bytes.size() == kLegacyBodyBytes) return from_legacy_bytes(bytes);
  throw std::invalid_argument("body must be 240 legacy bytes or 560 native bytes");
}

std::array<std::uint8_t, kNativeBodyBytes> PackedBody::native_bytes() const {
  std::array<std::uint8_t, kNativeBodyBytes> bytes{};
  std::size_t offset = 0;
  for (const auto& corner : words) {
    for (const auto& section : corner) {
      for (const auto word : section) {
        write_u16_le(word, bytes, offset);
        offset += 2;
      }
    }
  }
  return bytes;
}

bool PackedBody::is_legacy_representable() const {
  for (std::size_t corner = 0; corner < kLegacyCornerCount; ++corner) {
    if (words[corner] != words[corner + kLegacyCornerCount]) return false;
    if (words[corner][kLegacySectionCount] != kIdentitySection) return false;
  }
  return true;
}

std::array<std::uint8_t, kLegacyBodyBytes> PackedBody::legacy_bytes() const {
  if (!is_legacy_representable()) {
    throw std::logic_error("body uses section seven or the third axis");
  }
  std::array<std::uint8_t, kLegacyBodyBytes> bytes{};
  std::size_t offset = 0;
  for (std::size_t corner = 0; corner < kLegacyCornerCount; ++corner) {
    for (std::size_t section = 0; section < kLegacySectionCount; ++section) {
      for (const auto word : words[corner][section]) {
        write_u16_le(word, bytes, offset);
        offset += 2;
      }
    }
  }
  return bytes;
}

CornerWords PackedBody::interpolate_words(float morph, float q, float z) const {
  CornerWords result{};
  for (std::size_t section = 0; section < kSectionCount; ++section) {
    for (std::size_t word = 0; word < kCoefficientCount; ++word) {
      std::array<std::uint16_t, 2> plane{};
      for (std::size_t zi = 0; zi < 2; ++zi) {
        const std::size_t base = zi * kLegacyCornerCount;
        const auto edge0 = interpolate_word(words[base][section][word],
                                            words[base + 1][section][word], morph);
        const auto edge1 = interpolate_word(words[base + 2][section][word],
                                            words[base + 3][section][word], morph);
        plane[zi] = interpolate_word(edge0, edge1, q);
      }
      result[section][word] = interpolate_word(plane[0], plane[1], z);
    }
  }
  return result;
}

Cascade PackedBody::interpolate_biquads(float morph, float q, float z) const {
  const auto packed = interpolate_words(morph, q, z);
  Cascade result{};
  for (std::size_t i = 0; i < kSectionCount; ++i) {
    result[i] = section_words_to_biquad(packed[i]);
  }
  return result;
}

}
