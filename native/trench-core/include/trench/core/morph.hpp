#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "trench/core/p2k.hpp"

namespace trench::core::p2k {

std::vector<double> morph_response_db(std::span<const std::uint8_t> body, float morph, float q,
                                      const Grid& g = grid());

struct InteriorAudit {
  double max_step_db{};
  double mean_step_db{};
  float worst_morph{};
  float worst_q{};
  std::size_t refused{};
};

InteriorAudit interior_audit(std::span<const std::uint8_t> body, const Grid& g = grid(),
                             std::size_t morph_steps = 21, std::size_t q_steps = 5);

}  // namespace trench::core::p2k
