#include "trench/core/audition.hpp"

#include <cmath>

namespace trench::core {

void CascadeRunner::set_target(const Cascade& target) {
  target_ = target;
  if (!primed_) {
    current_ = target;
    primed_ = true;
  }
}

void CascadeRunner::reset() {
  for (auto& s : state_) s = {};
}

void CascadeRunner::process(std::span<float> block) {
  std::size_t done = 0;
  while (done < block.size()) {
    const std::size_t n = std::min(kAuditionTick, block.size() - done);
    Cascade delta{};
    for (std::size_t si = 0; si < kSectionCount; ++si) {
      for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) {
        delta[si][ci] = (target_[si][ci] - current_[si][ci]) / static_cast<double>(n);
      }
    }
    for (std::size_t i = 0; i < n; ++i) {
      double x = block[done + i];
      for (std::size_t si = 0; si < kSectionCount; ++si) {
        auto& c = current_[si];
        for (std::size_t ci = 0; ci < kCoefficientCount; ++ci) c[ci] += delta[si][ci];
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
      block[done + i] = static_cast<float>(x);
    }
    current_ = target_;
    done += n;
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

}  // namespace trench::core
