#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace trench::core {

inline constexpr std::size_t kCoefficientCount = 5;
inline constexpr std::size_t kSectionCount = 7;
inline constexpr std::size_t kCornerCount = 8;
inline constexpr std::size_t kLegacySectionCount = 6;
inline constexpr std::size_t kLegacyCornerCount = 4;
inline constexpr std::size_t kNativeBodyBytes = 560;
inline constexpr std::size_t kLegacyBodyBytes = 240;
inline constexpr double kMorpheusDatumHz = 39'062.5;
inline constexpr double kP2kDatumHz = 44'100.0;

using PackedSection = std::array<std::uint16_t, kCoefficientCount>;
using Biquad = std::array<double, kCoefficientCount>;
using CornerWords = std::array<PackedSection, kSectionCount>;
using Cascade = std::array<Biquad, kSectionCount>;

inline constexpr PackedSection kIdentitySection{
    0xDFFF, 0xFFFF, 0xDFFF, 0xFFFF, 0xDFFF};

struct ConjugatePair {
  double hz{};
  double radius{};
};

struct RealPair {
  double root_a{};
  double root_b{};
};

struct DegeneratePair {};

using RootPair = std::variant<ConjugatePair, RealPair, DegeneratePair>;

struct SectionGeometry {
  RootPair pole;
  RootPair zero;
  double scale{1.0};
};

double decode_word(std::uint16_t word);
double decode_fractional(double word);
std::uint16_t encode_word(double value);
std::uint16_t interpolate_word(std::uint16_t a, std::uint16_t b, float fraction);

Biquad section_values_to_biquad(const std::array<double, kCoefficientCount>& decoded);
Biquad section_words_to_biquad(const PackedSection& words);
SectionGeometry geometry_from_words(
    const PackedSection& words,
    double sample_rate_hz = kMorpheusDatumHz);
PackedSection words_from_geometry(
    const SectionGeometry& geometry,
    double sample_rate_hz = kMorpheusDatumHz);

class PackedBody {
 public:
  static PackedBody from_native_bytes(std::span<const std::uint8_t> bytes);
  static PackedBody from_legacy_bytes(std::span<const std::uint8_t> bytes);
  static PackedBody from_body_bytes(std::span<const std::uint8_t> bytes);

  [[nodiscard]] std::array<std::uint8_t, kNativeBodyBytes> native_bytes() const;
  [[nodiscard]] bool is_legacy_representable() const;
  [[nodiscard]] std::array<std::uint8_t, kLegacyBodyBytes> legacy_bytes() const;
  [[nodiscard]] CornerWords interpolate_words(float morph, float q, float z) const;
  [[nodiscard]] Cascade interpolate_biquads(float morph, float q, float z) const;
  [[nodiscard]] Cascade interpolate_biquads_float(float morph, float q, float z) const;

  std::array<CornerWords, kCornerCount> words{};
};

double section_response_db(const Biquad& section, double frequency_hz,
                           double sample_rate_hz);
double cascade_response_db(std::span<const Biquad> sections, double frequency_hz,
                           double sample_rate_hz);

Cascade unity_dc(const Cascade& cascade);

std::vector<double> marginal_contribution_db(
    std::span<const Biquad> sections,
    std::size_t section_index,
    std::span<const double> frequencies_hz,
    double sample_rate_hz);

std::vector<std::vector<double>> marginal_contributions_db(
    std::span<const Biquad> sections,
    std::span<const double> frequencies_hz,
    double sample_rate_hz);

std::vector<double> logarithmic_frequency_grid(double low_hz, double high_hz,
                                               std::size_t point_count);

}
