#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "trench/core/native_body.hpp"
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
  double excursion_up_db{};
  double excursion_down_db{};
  double bilinear_dev_max_db{};
  double bilinear_dev_p95_db{};
  double detour_max{};
  double loudness_swing_db{};
  double loudness_beyond_corners_db{};
  double prefix_headroom_db{};
  double prefix_floor_db{};
};

using SectionBiquads = std::array<std::array<double, 5>, kStageCount>;
using CascadeAt = std::function<SectionBiquads(float morph, float q)>;

std::vector<double> response_db(const SectionBiquads& biquads, const Grid& g = grid());

InteriorAudit interior_audit(const CascadeAt& at, const Grid& g = grid(),
                             std::size_t morph_steps = 21, std::size_t q_steps = 5);
InteriorAudit interior_audit(std::span<const std::uint8_t> body, const Grid& g = grid(),
                             std::size_t morph_steps = 21, std::size_t q_steps = 5);
InteriorAudit interior_audit(const native::Body& body, const Grid& g = grid(),
                             std::size_t morph_steps = 21, std::size_t q_steps = 5);

}  // namespace trench::core::p2k
