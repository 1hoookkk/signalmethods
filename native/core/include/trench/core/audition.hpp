#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "trench/core/packed_body.hpp"

namespace trench::core {

inline constexpr std::size_t kApproachSamples = 256;

using EncodedSection = std::array<double, kCoefficientCount>;
using EncodedCascade = std::array<EncodedSection, kSectionCount>;

EncodedSection encode_section(const Biquad& section);
EncodedCascade encode_cascade(const Cascade& cascade);
Biquad decode_section(const EncodedSection& encoded);

class CascadeRunner {
 public:
  void set_target(const EncodedCascade& target);
  void reset();
  void process(std::span<float> block);
  [[nodiscard]] const Cascade& coefficients() const noexcept { return coefficients_; }
  [[nodiscard]] std::size_t remaining() const noexcept { return remaining_; }

 private:
  struct Section {
    double w1{};
    double w2{};
  };
  void decode();
  EncodedCascade current_{};
  EncodedCascade target_{};
  EncodedCascade step_{};
  Cascade coefficients_{};
  std::size_t remaining_{};
  std::array<Section, kSectionCount> state_{};
  bool primed_{};
};

class SawSource {
 public:
  SawSource(double hz, double sample_rate_hz, float level);
  float next();

 private:
  double phase_{};
  double step_{};
  float level_{};
};

}
