#include "trench/core/audition.hpp"

#include <algorithm>
#include <cmath>

namespace trench::core {

namespace {

constexpr double kDecodedFloor = 1.0e-30;

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

void CascadeRunner::decode() {
  for (std::size_t si = 0; si < kSectionCount; ++si) coefficients_[si] = decode_section(current_[si]);
}

void CascadeRunner::set_target(const EncodedCascade& target) {
  target_ = target;
  if (!primed_) {
    current_ = target;
    remaining_ = 0;
    primed_ = true;
    decode();
    return;
  }
  for (std::size_t si = 0; si < kSectionCount; ++si) {
    for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) {
      step_[si][ci] = (target_[si][ci] - current_[si][ci]) / static_cast<double>(kApproachSamples);
    }
  }
  remaining_ = kApproachSamples;
}

void CascadeRunner::reset() {
  for (auto& s : state_) s = {};
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
    }
    double x = sample;
    for (std::size_t si = 0; si < kSectionCount; ++si) {
      const auto& c = coefficients_[si];
      auto& s = state_[si];
      const double y = c[0] * x + s.w1;
      s.w1 = c[1] * x - c[3] * y + s.w2;
      s.w2 = c[2] * x - c[4] * y;
      x = y;
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
