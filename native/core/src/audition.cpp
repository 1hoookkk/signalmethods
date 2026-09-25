#include "trench/core/audition.hpp"

#include <algorithm>
#include <cmath>

namespace trench::core {

namespace {

constexpr double kDecodedFloor = 1.0e-30;
constexpr double kGritCeilingFloor = 1.0;
constexpr double kGritCeilingWide = 2.5;
constexpr double kGritThreshFraction = 0.6;
constexpr double kStageThresholdFloor = 0.05;
constexpr double kRingCeilingDb = 24.0;
constexpr double kRingFloor = 1.0e-4;
constexpr double kRingAttackSeconds = 0.001;
constexpr double kRingReleaseSeconds = 0.120;
constexpr double kRingDenominatorFloor = 1.0e-9;
constexpr double kRadiusSineFloor = 1.0e-6;
const double kRingCeilingLinear = std::pow(10.0, kRingCeilingDb / 20.0);

double one_pole_coefficient(double seconds, double sample_rate_hz) {
  if (!(seconds > 0.0) || !(sample_rate_hz > 0.0)) return 0.0;
  return std::exp(-1.0 / (seconds * sample_rate_hz));
}

double grit_state_ceiling(double grit) {
  const double g = std::clamp(grit, 0.0, 1.0);
  return kGritCeilingWide + (kGritCeilingFloor - kGritCeilingWide) * g;
}

double saturate_stage(double x, double threshold) {
  if (std::abs(x) <= threshold) return x;
  return (x > 0.0) ? (threshold + 0.5 * std::tanh(2.0 * (x - threshold)))
                   : (-threshold - 0.5 * std::tanh(2.0 * (-x - threshold)));
}

double logged(double decoded) { return std::log(std::max(decoded, kDecodedFloor)); }

}

EncodedSection encode_section(const Biquad& section) {
  const double b0 = section[0];
  const double gain = std::abs(b0) > 0.0 ? b0 : kDecodedFloor;
  const double zero_p = section[1] / gain;
  const double zero_q = section[2] / gain;
  const double pole_p = section[3];
  const double pole_q = section[4];
  const double zero_rsq = 1.0 - zero_q;
  const double pole_rsq = 1.0 - pole_q;
  return {logged((zero_p + 2.0 - zero_rsq) / 4.0), logged(zero_rsq),
          logged((pole_p + 2.0 - pole_rsq) / 4.0), logged(pole_rsq),
          logged(std::abs(gain) / 4.0)};
}

EncodedCascade encode_cascade(const Cascade& cascade) {
  EncodedCascade out{};
  for (std::size_t si = 0; si < kSectionCount; ++si) out[si] = encode_section(cascade[si]);
  return out;
}

Biquad decode_section(const EncodedSection& encoded) {
  const double d0 = std::exp(encoded[0]);
  const double d1 = std::exp(encoded[1]);
  const double d2 = std::exp(encoded[2]);
  const double d3 = std::exp(encoded[3]);
  const double d4 = std::exp(encoded[4]);
  const double c0 = 4.0 * d0 + d1;
  const double c2 = 4.0 * d2 + d3;
  const double c4 = 4.0 * d4;
  return {c4, (c0 - 2.0) * c4, (1.0 - d1) * c4, c2 - 2.0, 1.0 - d3};
}

CascadeRunner::CascadeRunner() { update_ring_coefficients(); }

void CascadeRunner::update_ring_coefficients() noexcept {
  ring_decay_ = one_pole_coefficient(ring_release_seconds_, sample_rate_hz_);
  ring_attack_ = one_pole_coefficient(ring_attack_seconds_, sample_rate_hz_);
  ring_release_ = ring_decay_;
}

void CascadeRunner::set_sample_rate(double sample_rate_hz) noexcept {
  sample_rate_hz_ = sample_rate_hz > 0.0 ? sample_rate_hz : 44100.0;
  update_ring_coefficients();
}

void CascadeRunner::set_ring_leveller(bool enabled) noexcept { ring_on_ = enabled; }

void CascadeRunner::set_feedback_ceiling(double linear) noexcept {
  feedback_ceiling_ = std::isfinite(linear) ? std::clamp(linear, 0.0, 256.0) : 0.0;
}

void CascadeRunner::set_ring_calibration(double allowance_db, double attack_ms, double release_ms, double floor_db) noexcept {
  ring_allowance_ = std::pow(10.0, std::clamp(allowance_db, 0.0, 48.0) / 20.0);
  ring_floor_ = std::pow(10.0, std::clamp(floor_db, -100.0, -30.0) / 20.0);
  ring_attack_seconds_ = std::clamp(attack_ms, 0.1, 100.0) / 1000.0;
  ring_release_seconds_ = std::clamp(release_ms, 5.0, 2000.0) / 1000.0;
  update_ring_coefficients();
}

double CascadeRunner::ring_level(Section& section, double in, double y) const noexcept {
  const double magnitude_in = std::abs(in);
  const double magnitude_out = std::abs(y);
  const double faded_in = section.e_in * ring_decay_;
  const double faded_out = section.e_out * ring_decay_;
  section.e_in = magnitude_in > faded_in ? magnitude_in : faded_in;
  section.e_out = magnitude_out > faded_out ? magnitude_out : faded_out;
  const double allowed = section.e_in * ring_allowance_ + ring_floor_;
  const double denominator = section.e_out + kRingDenominatorFloor;
  const double target = denominator > allowed ? allowed / denominator : 1.0;
  if (target >= 1.0 && section.ring_gain >= 1.0) return y;
  const double coefficient = target < section.ring_gain ? ring_attack_ : ring_release_;
  section.ring_gain = target + (section.ring_gain - target) * coefficient;
  return y * section.ring_gain;
}

Biquad CascadeRunner::kernel_row(const Biquad& b) {
  if (b[0] == 0.0) {
    return {2.0, 1.0, b[3] + 2.0, 1.0 - b[4], 0.0};
  }
  return {b[1] / b[0] + 2.0, 1.0 - b[2] / b[0], b[3] + 2.0, 1.0 - b[4], b[0]};
}

Biquad CascadeRunner::biquad_of_row(const Biquad& c) {
  return {c[4], (c[0] - 2.0) * c[4], (1.0 - c[1]) * c[4], c[2] - 2.0, 1.0 - c[3]};
}

void CascadeRunner::decode() {
  for (std::size_t si = 0; si < kSectionCount; ++si) coefficients_[si] = decode_section(current_[si]);
}

void CascadeRunner::set_target(const EncodedCascade& target, std::size_t samples) {
  kernel_ramp_ = false;
  kernel_valid_ = false;
  if (!primed_) {
    current_ = target;
    target_ = target;
    remaining_ = 0;
    primed_ = true;
    encoded_stale_ = false;
    decode();
    return;
  }
  if (remaining_ == 0) {
    glide_remaining_ = 0;
    if (encoded_stale_) {
      current_ = encode_cascade(coefficients_);
      encoded_stale_ = false;
    }
    if (target == current_) {
      target_ = target;
      return;
    }
    remaining_ = samples > 0 ? samples : kApproachSamples;
  } else if (target == target_) {
    return;
  }
  target_ = target;
  for (std::size_t si = 0; si < kSectionCount; ++si) {
    for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) {
      step_[si][ci] = (target_[si][ci] - current_[si][ci]) / static_cast<double>(remaining_);
    }
  }
}

void CascadeRunner::set_immediate(const Cascade& coefficients) {
  if (primed_ && remaining_ > 0) {
    set_target(encode_cascade(coefficients));
    return;
  }
  glide_remaining_ = 0;
  kernel_ramp_ = false;
  kernel_valid_ = false;
  coefficients_ = coefficients;
  primed_ = true;
  encoded_stale_ = true;
}

void CascadeRunner::set_glide(const Cascade& coefficients, std::size_t samples) {
  if (!primed_ || samples == 0) {
    set_immediate(coefficients);
    return;
  }
  if (remaining_ > 0) {
    set_target(encode_cascade(coefficients));
    return;
  }
  glide_target_ = coefficients;
  kernel_ramp_ = false;
  kernel_valid_ = false;
  for (std::size_t si = 0; si < kSectionCount; ++si) {
    const auto from = kernel_row(coefficients_[si]);
    const auto to = kernel_row(coefficients[si]);
    glide_row_[si] = from;
    for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) {
      glide_step_[si][ci] = (to[ci] - from[ci]) / static_cast<double>(samples);
    }
  }
  glide_remaining_ = samples;
  encoded_stale_ = true;
}

void CascadeRunner::set_kernel_targets(std::span<const Biquad> targets, std::size_t ramp_samples) {
  if (remaining_ > 0) {
    return;
  }
  const double n = static_cast<double>(std::max<std::size_t>(1, ramp_samples));
  if (!kernel_valid_) {
    for (std::size_t si = 0; si < kSectionCount; ++si) {
      state_[si].kernel = kernel_row(coefficients_[si]);
      state_[si].ktarget = state_[si].kernel;
    }
    kernel_valid_ = true;
  }
  for (std::size_t si = 0; si < kSectionCount; ++si) {
    const auto target = si < targets.size() ? kernel_row(targets[si]) : Biquad{2.0, 1.0, 2.0, 1.0, 1.0};
    state_[si].kernel = state_[si].ktarget;
    for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) {
      state_[si].kdeltas[ci] = (target[ci] - state_[si].kernel[ci]) / n;
    }
    state_[si].ktarget = target;
  }
  kernel_ramp_ = true;
  glide_remaining_ = 0;
}

void CascadeRunner::zero_kernel_deltas() noexcept {
  if (kernel_valid_) {
    for (auto& s : state_) {
      s.kernel = s.ktarget;
      s.kdeltas = {};
    }
  }
}

void CascadeRunner::reset() {
  for (auto& s : state_) s = {};
  activity_ = 0.0;
  kernel_ramp_ = false;
  kernel_valid_ = false;
}

void CascadeRunner::set_stage_saturation(bool enabled, double threshold) noexcept {
  stage_saturation_ = enabled;
  stage_threshold_ = std::clamp(threshold, kStageThresholdFloor, 1.0);
}

void CascadeRunner::set_pole_distortion(double grit) noexcept {
  grit_ = std::clamp(grit, 0.0, 1.0);
}

void CascadeRunner::set_radius_distortion(double threshold) noexcept {
  radius_threshold_ = std::isfinite(threshold) && threshold > 0.0 ? threshold : 0.0;
}

double CascadeRunner::grit_activity() const noexcept {
  return std::min(activity_, 1.0);
}

void CascadeRunner::process(std::span<float> block) {
  for (float& sample : block) {
    if (kernel_ramp_) {
      for (std::size_t si = 0; si < kSectionCount; ++si) {
        coefficients_[si] = biquad_of_row(state_[si].kernel);
      }
    } else if (remaining_ > 0) {
      --remaining_;
      if (remaining_ == 0) {
        current_ = target_;
      } else {
        for (std::size_t si = 0; si < kSectionCount; ++si) {
          for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) current_[si][ci] += step_[si][ci];
        }
      }
      decode();
    } else if (glide_remaining_ > 0) {
      --glide_remaining_;
      if (glide_remaining_ == 0) {
        coefficients_ = glide_target_;
      } else {
        for (std::size_t si = 0; si < kSectionCount; ++si) {
          for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) glide_row_[si][ci] += glide_step_[si][ci];
          coefficients_[si] = biquad_of_row(glide_row_[si]);
        }
      }
    }
    double x = sample;
    if (radius_threshold_ > 0.0) {
      activity_ *= 0.999;
      for (std::size_t si = 0; si < kSectionCount; ++si) {
        const auto& c = coefficients_[si];
        auto& s = state_[si];
        const double radius_squared = c[4];
        const double radius = radius_squared > 0.0 ? std::sqrt(radius_squared) : 0.0;
        const double cosine = radius > 0.0 ? -c[3] / (2.0 * radius) : 0.0;
        const double sine_squared = 1.0 - cosine * cosine;
        double w;
        if (radius > 0.0 && radius < 1.0 && sine_squared > kRadiusSineFloor * kRadiusSineFloor) {
          const double sine = std::sqrt(sine_squared);
          const double dc = 1.0 + c[3] + c[4];
          const double u = s.w1 - radius * cosine * s.w2;
          const double v = radius * sine * s.w2;
          const double level = std::abs(s.p1);
          double lifted = radius;
          double gain = 1.0;
          if (level >= radius_threshold_) {
            const double m = (level - radius_threshold_) / level;
            activity_ = std::max(activity_, m);
            lifted = radius + m * radius * (1.0 - radius);
            gain = (1.0 - 2.0 * lifted * cosine + lifted * lifted) / dc;
          }
          const double ceiling = 1000.0 * radius_threshold_;
          const double next_u = std::clamp(x * gain + lifted * cosine * u - lifted * sine * v, -ceiling, ceiling);
          const double next_v = std::clamp(lifted * sine * u + lifted * cosine * v, -ceiling, ceiling);
          w = next_u + (cosine / sine) * next_v;
          double y = c[0] * w + c[1] * s.p1 + c[2] * s.p2;
          if (stage_saturation_) y = saturate_stage(y, stage_threshold_);
          s.p2 = s.p1;
          s.p1 = w;
          s.w2 = next_v / (radius * sine);
          s.w1 = next_u + radius * cosine * s.w2;
          peak_state_ = std::max(peak_state_, std::abs(w) * std::clamp(dc, 1.0e-9, 1.0));
          s.y_prev = y;
          x = y;
        } else {
          const double out = c[0] * x + c[1] * s.x1 + c[2] * s.x2 - c[3] * s.y1 - c[4] * s.y2;
          s.x2 = s.x1;
          s.x1 = x;
          s.y2 = s.y1;
          s.y1 = out;
          double y = stage_saturation_ ? saturate_stage(out, stage_threshold_) : out;
          s.y_prev = y;
          x = y;
        }
      }
    } else if (grit_ <= 0.0) {
      activity_ = 0.0;
      for (std::size_t si = 0; si < kSectionCount; ++si) {
        const auto& c = coefficients_[si];
        auto& s = state_[si];
        const double out = c[0] * x + c[1] * s.x1 + c[2] * s.x2 - c[3] * s.y1 - c[4] * s.y2;
        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = out;
        if (c[3] != 0.0 || c[4] != 0.0) peak_state_ = std::max(peak_state_, std::abs(out));
        double y = stage_saturation_ ? saturate_stage(out, stage_threshold_) : out;
        s.y_prev = y;
        x = ring_on_ ? ring_level(s, x, y) : y;
      }
    } else {
      const double ceiling = feedback_ceiling_ > 0.0 ? feedback_ceiling_ : grit_state_ceiling(grit_);
      activity_ *= 0.999;
      for (std::size_t si = 0; si < kSectionCount; ++si) {
        const auto& c = coefficients_[si];
        auto& s = state_[si];
        const double limit = ceiling;
        const double knee = kGritThreshFraction * limit;
        double out = c[0] * x + c[1] * s.x1 + c[2] * s.x2 - c[3] * s.y1 - c[4] * s.y2;
        if (std::abs(out) > knee && (c[3] != 0.0 || c[4] != 0.0)) {
          activity_ = std::max(activity_, std::min(1.0, (std::abs(out) - knee) / knee));
          out = std::copysign(knee + (limit - knee) * std::tanh((std::abs(out) - knee) / (limit - knee)), out);
        }
        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = out;
        if (c[3] != 0.0 || c[4] != 0.0) peak_state_ = std::max(peak_state_, std::abs(out));
        double y = stage_saturation_ ? saturate_stage(out, stage_threshold_) : out;
        s.y_prev = y;
        x = ring_on_ ? ring_level(s, x, y) : y;
      }

    }
    if (kernel_ramp_) {
      for (std::size_t si = 0; si < kSectionCount; ++si) {
        for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) {
          state_[si].kernel[ci] += state_[si].kdeltas[ci];
        }
      }
    }
    if (!std::isfinite(x)) {
      x = 0.0;
      reset();
    }
    sample = static_cast<float>(x);
  }
}

SawSource::SawSource(double hz, double sample_rate_hz, float level)
    : step_(hz / sample_rate_hz), level_(level) {}

float SawSource::next() {
  const float out = static_cast<float>(2.0 * phase_ - 1.0) * level_;
  phase_ += step_;
  if (phase_ >= 1.0) phase_ -= 1.0;
  return out;
}

}
