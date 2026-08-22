#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "trench/core/packed_body.hpp"

namespace trench::core {

inline constexpr std::size_t kAuditionTick = 32;

class CascadeRunner {
 public:
  void set_target(const Cascade& target);
  void reset();
  void process(std::span<float> block);

 private:
  struct Section {
    double w1{};
    double w2{};
  };
  Cascade current_{};
  Cascade target_{};
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

}  // namespace trench::core
