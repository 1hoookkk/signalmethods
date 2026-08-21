#pragma once

#include "trench/core/p2k.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace trench::core::p2k {

struct PushDirection {
  std::array<double, kNpts> curve{};
  double sigma{};
};

struct Push {
  std::vector<PushDirection> directions;
  std::size_t candidate_count{};
};

Push compute_push(const StoredCorner& corner, std::uint32_t freedom_mask,
                  std::size_t max_directions = 4);

}  // namespace trench::core::p2k
