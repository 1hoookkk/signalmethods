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
  CascadeRunner();
  void set_target(const EncodedCascade& target);
  void set_immediate(const Cascade& coefficients);
  void set_glide(const Cascade& coefficients, std::size_t samples);
  void reset();
  void process(std::span<float> block);
  void set_pole_distortion(double grit) noexcept;
  void set_sample_rate(double sample_rate_hz) noexcept;
  void set_ring_leveller(bool enabled) noexcept;
  [[nodiscard]] double grit_activity() const noexcept;
  [[nodiscard]] const Cascade& coefficients() const noexcept { return coefficients_; }
  [[nodiscard]] std::size_t remaining() const noexcept { return remaining_; }

 private:
  struct Section {
    double w1{};
    double w2{};
    double y_prev{};
    double e_in{};
    double e_out{};
    double ring_gain{1.0};
  };
  void decode();
  void update_ring_coefficients() noexcept;
  double ring_level(Section& section, double in, double y) const noexcept;
  static Biquad kernel_row(const Biquad& b);
  static Biquad biquad_of_row(const Biquad& c);
  EncodedCascade current_{};
  EncodedCascade target_{};
  EncodedCascade step_{};
  Cascade coefficients_{};
  std::size_t remaining_{};
  std::array<Section, kSectionCount> state_{};
  double grit_{};
  double activity_{};
  Cascade glide_target_{};
  Cascade glide_step_{};
  Cascade glide_row_{};
  std::size_t glide_remaining_{};
  bool primed_{};
  bool encoded_stale_{};
  bool ring_on_{true};
  double sample_rate_hz_{44100.0};
  double ring_decay_{};
  double ring_attack_{};
  double ring_release_{};
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
