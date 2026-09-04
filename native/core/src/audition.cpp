#include "trench/core/audition.hpp"

#include <algorithm>
#include <cmath>

namespace trench::core {

namespace {

constexpr double kDecodedFloor = 1.0e-30;
constexpr double kGritCeilingFloor = 0.35;
constexpr double kGritCeilingWide = 4.0;
constexpr double kGritThreshFraction = 0.6;
constexpr double kRingCeilingDb = 24.0;
constexpr double kRingFloor = 1.0e-4;
constexpr double kRingAttackSeconds = 0.001;
constexpr double kRingReleaseSeconds = 0.120;
constexpr double kRingDenominatorFloor = 1.0e-9;
const double kRingCeilingLinear = std::pow(10.0, kRingCeilingDb / 20.0);

double one_pole_coefficient(double seconds, double sample_rate_hz) {
  if (!(seconds > 0.0) || !(sample_rate_hz > 0.0)) return 0.0;
  return std::exp(-1.0 / (seconds * sample_rate_hz));
}

double grit_state_ceiling(double grit) {
  return kGritCeilingWide * std::pow(kGritCeilingFloor / kGritCeilingWide, grit);
}

double grit_distort_threshold(double grit) {
  return kGritThreshFraction * grit_state_ceiling(grit);
}

double soft_clamp_ceiling(double x, double ceiling) {
  return ceiling * std::tanh(x / ceiling);
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
  ring_decay_ = one_pole_coefficient(kRingReleaseSeconds, sample_rate_hz_);
  ring_attack_ = one_pole_coefficient(kRingAttackSeconds, sample_rate_hz_);
  ring_release_ = ring_decay_;
}

void CascadeRunner::set_sample_rate(double sample_rate_hz) noexcept {
  sample_rate_hz_ = sample_rate_hz > 0.0 ? sample_rate_hz : 44100.0;
  update_ring_coefficients();
}

void CascadeRunner::set_ring_leveller(bool enabled) noexcept { ring_on_ = enabled; }

double CascadeRunner::ring_level(Section& section, double in, double y) const noexcept {
  const double magnitude_in = std::abs(in);
  const double magnitude_out = std::abs(y);
  const double faded_in = section.e_in * ring_decay_;
  const double faded_out = section.e_out * ring_decay_;
  section.e_in = magnitude_in > faded_in ? magnitude_in : faded_in;
  section.e_out = magnitude_out > faded_out ? magnitude_out : faded_out;
  const double allowed = section.e_in * kRingCeilingLinear + kRingFloor;
  const double denominator = section.e_out + kRingDenominatorFloor;
  const double target = denominator > allowed ? allowed / denominator : 1.0;
  if (target >= 1.0 && section.ring_gain >= 1.0) return y;
  const double coefficient = target < section.ring_gain ? ring_attack_ : ring_release_;
  section.ring_gain = target + (section.ring_gain - target) * coefficient;
  return y * section.ring_gain;
}

Biquad CascadeRunner::kernel_row(const Biquad& b) {
  const double c4 = std::abs(b[0]) > 0.0 ? b[0] : kDecodedFloor;
  return {b[1] / c4 + 2.0, 1.0 - b[2] / c4, b[3] + 2.0, 1.0 - b[4], c4};
}

Biquad CascadeRunner::biquad_of_row(const Biquad& c) {
  return {c[4], (c[0] - 2.0) * c[4], (1.0 - c[1]) * c[4], c[2] - 2.0, 1.0 - c[3]};
}

void CascadeRunner::decode() {
  for (std::size_t si = 0; si < kSectionCount; ++si) coefficients_[si] = decode_section(current_[si]);
}

void CascadeRunner::set_target(const EncodedCascade& target) {
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
    remaining_ = kApproachSamples;
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

void CascadeRunner::reset() {
  for (auto& s : state_) s = {};
}

void CascadeRunner::set_pole_distortion(double grit) noexcept {
  grit_ = std::clamp(grit, 0.0, 1.0);
}

double CascadeRunner::grit_activity() const noexcept {
  return std::min(activity_, 1.0);
}

void CascadeRunner::process(std::span<float> block) {
  for (float& sample : block) {
    if (remaining_ > 0) {
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
    if (grit_ <= 0.0) {
      for (std::size_t si = 0; si < kSectionCount; ++si) {
        const auto& c = coefficients_[si];
        auto& s = state_[si];
        const double y = c[0] * x + s.w1;
        s.w1 = c[1] * x - c[3] * y + s.w2;
        s.w2 = c[2] * x - c[4] * y;
        x = ring_on_ ? ring_level(s, x, y) : y;
      }
    } else {
      const double ceiling = grit_state_ceiling(grit_);
      const double vt = grit_distort_threshold(grit_);
      activity_ *= 0.999;
      for (std::size_t si = 0; si < kSectionCount; ++si) {
        const auto& c = coefficients_[si];
        auto& s = state_[si];
        double a1 = c[3];
        double a2 = c[4];
        const double vg = std::abs(s.y_prev);
        if (vg > vt && a2 > 1.0e-9) {
          const double r = std::sqrt(a2);
          if (r > 1.0e-6 && r < 1.0) {
            const double cos_theta = std::clamp(-a1 / (2.0 * r), -1.0, 1.0);
            const double r_new = std::clamp(r - r * (1.0 - r) * (vg - vt) / vg, 0.0, 0.9999);
            a1 = -2.0 * r_new * cos_theta;
            a2 = r_new * r_new;
          }
        }
        const double y = c[0] * x + s.w1;
        s.w1 = soft_clamp_ceiling(c[1] * x - a1 * y + s.w2, ceiling);
        s.w2 = soft_clamp_ceiling(c[2] * x - a2 * y, ceiling);
        s.y_prev = y;
        x = ring_on_ ? ring_level(s, x, y) : y;
      }
      if (grit_ > activity_) activity_ = grit_;
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
