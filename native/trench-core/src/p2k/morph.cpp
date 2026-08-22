#include "trench/core/morph.hpp"

#include <cmath>

namespace trench::core::p2k {

std::vector<double> morph_response_db(std::span<const std::uint8_t> body, float morph, float q,
                                      const Grid& g) {
  return corner_response_db(interpolate_body(body, morph, q), g);
}

InteriorAudit interior_audit(std::span<const std::uint8_t> body, const Grid& g,
                             std::size_t morph_steps, std::size_t q_steps) {
  InteriorAudit out;
  const auto corners = body_corners(body);
  std::vector<double> scratch(kNpts, 0.0);
  double step_sum = 0.0;
  std::size_t steps = 0;
  for (std::size_t qi = 0; qi < q_steps; ++qi) {
    const float qq = q_steps > 1 ? static_cast<float>(qi) / static_cast<float>(q_steps - 1) : 0.0F;
    std::vector<double> previous;
    for (std::size_t mi = 0; mi < morph_steps; ++mi) {
      const float m =
          morph_steps > 1 ? static_cast<float>(mi) / static_cast<float>(morph_steps - 1) : 0.0F;
      auto current = corner_response_db(interpolate_plane(corners, m, qq), g);
      bool finite = true;
      for (const double v : current) {
        finite = finite && std::isfinite(v);
      }
      if (!finite) {
        ++out.refused;
        previous.clear();
        continue;
      }
      if (!previous.empty()) {
        const double step = std::sqrt(g.residual_var(previous, current, scratch));
        step_sum += step;
        ++steps;
        if (step > out.max_step_db) {
          out.max_step_db = step;
          out.worst_morph = m;
          out.worst_q = qq;
        }
      }
      previous = std::move(current);
    }
  }
  out.mean_step_db = steps > 0 ? step_sum / static_cast<double>(steps) : 0.0;
  return out;
}

}  // namespace trench::core::p2k
