#pragma once

#include <span>
#include <utility>
#include <vector>

namespace trench::app {

struct AnalyzeProposal {
  std::vector<std::pair<double, double>> poles;
  std::vector<std::pair<double, double>> zeros;
};

[[nodiscard]] AnalyzeProposal analyzeSound(std::span<const float> samples,
                                           double sample_rate_hz);

}  // namespace trench::app
