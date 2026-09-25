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
  void set_target(const EncodedCascade& target, std::size_t samples = 0);
  void set_immediate(const Cascade& coefficients);
  void set_glide(const Cascade& coefficients, std::size_t samples);
  void set_kernel_targets(std::span<const Biquad> targets, std::size_t ramp_samples);
  void zero_kernel_deltas() noexcept;
  void reset();
  void process(std::span<float> block);
  void set_pole_distortion(double grit) noexcept;
  void set_radius_distortion(double threshold) noexcept;
  void set_stage_saturation(bool enabled, double threshold) noexcept;
  void set_sample_rate(double sample_rate_hz) noexcept;
  void set_ring_leveller(bool enabled) noexcept;
  void set_feedback_ceiling(double linear) noexcept;
  void set_ring_calibration(double allowance_db, double attack_ms, double release_ms, double floor_db) noexcept;
  [[nodiscard]] double grit_activity() const noexcept;
  [[nodiscard]] double take_peak_state() noexcept { const double p = peak_state_; peak_state_ = 0.0; return p; }
  [[nodiscard]] const Cascade& coefficients() const noexcept { return coefficients_; }
  [[nodiscard]] std::size_t remaining() const noexcept { return remaining_; }

 private:
  struct Section {
    Biquad kernel{2.0, 1.0, 2.0, 1.0, 1.0};
    Biquad kdeltas{};
    Biquad ktarget{2.0, 1.0, 2.0, 1.0, 1.0};
    double x1{};
    double x2{};
    double y1{};
    double y2{};
    double w1{};
    double w2{};
    double p1{};
    double p2{};
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
  double peak_state_{};
  Cascade glide_target_{};
  Cascade glide_step_{};
  Cascade glide_row_{};
  std::size_t glide_remaining_{};
  bool primed_{};
  bool encoded_stale_{};
  bool ring_on_{true};
  bool stage_saturation_{false};
  double stage_threshold_{1.0};
  bool kernel_ramp_{false};
  bool kernel_valid_{false};
  double sample_rate_hz_{44100.0};
  double ring_decay_{};
  double ring_attack_{};
  double ring_release_{};
  double feedback_ceiling_{};
  double radius_threshold_{};
  double ring_allowance_{15.848931924611133};
  double ring_floor_{1.0e-4};
  double ring_attack_seconds_{0.001};
  double ring_release_seconds_{0.120};
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
