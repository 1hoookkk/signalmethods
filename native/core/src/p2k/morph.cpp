#include "trench/core/morph.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "trench/core/native_body.hpp"
#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

namespace {

double percentile_of(std::vector<double>& values, double fraction) {
  if (values.empty()) {
    return 0.0;
  }
  std::sort(values.begin(), values.end());
  const auto n = static_cast<double>(values.size());
  const auto rank = static_cast<std::size_t>(std::ceil(fraction * n));
  const std::size_t index = rank == 0 ? 0 : std::min(values.size() - 1, rank - 1);
  return values[index];
}

double pink_power_db(std::span<const double> weight, std::span<const double> response) {
  double acc = 0.0;
  for (std::size_t i = 0; i < weight.size(); ++i) {
    acc += weight[i] * std::pow(10.0, response[i] / 10.0);
  }
  return 10.0 * std::log10(std::max(acc, 1e-30));
}

}  // namespace

std::vector<double> morph_response_db(std::span<const std::uint8_t> body, float morph, float q,
                                      const Grid& g) {
  return corner_response_db(interpolate_body(body, morph, q), g);
}

std::vector<double> response_db(const SectionBiquads& biquads, const Grid& g) {
  std::vector<double> out;
  out.reserve(g.hz.size());
  for (const double hz : g.hz) {
    double acc = 0.0;
    for (const auto& b : biquads) {
      acc += stage_db(b, hz);
    }
    out.push_back(acc);
  }
  return out;
}

InteriorAudit interior_audit(std::span<const std::uint8_t> body, const Grid& g,
                             std::size_t morph_steps, std::size_t q_steps) {
  const auto corners = body_corners(body);
  return interior_audit(
      [&](float m, float q) {
        const auto words = interpolate_plane(corners, m, q);
        SectionBiquads out{};
        for (std::size_t si = 0; si < kStageCount; ++si) {
          out[si] = section_words_to_biquad(words[si]);
        }
        return out;
      },
      g, morph_steps, q_steps);
}

InteriorAudit interior_audit(const native::Body& body, const Grid& g, std::size_t morph_steps,
                             std::size_t q_steps) {
  return interior_audit(
      [&](float m, float q) {
        const auto cascade =
            native::cascade(native::blend_roots_log_2019(body, m, q, kSr),
                            native::blend_gain_db_2019(body, m, q));
        SectionBiquads out{};
        std::copy_n(cascade.begin(), kStageCount, out.begin());
        return out;
      },
      g, morph_steps, q_steps);
}

InteriorAudit interior_audit(const CascadeAt& at, const Grid& g, std::size_t morph_steps,
                             std::size_t q_steps) {
  InteriorAudit out;
  const std::size_t npts = g.hz.size();
  std::vector<double> scratch(npts, 0.0);

  const float coords[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
  std::array<std::vector<double>, 4> corner_db{};
  for (std::size_t ci = 0; ci < 4; ++ci) {
    corner_db[ci] = response_db(at(coords[ci][0], coords[ci][1]), g);
  }
  std::vector<double> corner_ceiling(npts, 0.0);
  std::vector<double> corner_floor(npts, 0.0);
  for (std::size_t i = 0; i < npts; ++i) {
    corner_ceiling[i] = corner_db[0][i];
    corner_floor[i] = corner_db[0][i];
    for (std::size_t ci = 1; ci < 4; ++ci) {
      corner_ceiling[i] = std::max(corner_ceiling[i], corner_db[ci][i]);
      corner_floor[i] = std::min(corner_floor[i], corner_db[ci][i]);
    }
  }

  const std::vector<double> pink(npts, 1.0 / static_cast<double>(npts));
  double corner_power_hi = -std::numeric_limits<double>::infinity();
  double corner_power_lo = std::numeric_limits<double>::infinity();
  for (std::size_t ci = 0; ci < 4; ++ci) {
    const double p = pink_power_db(pink, corner_db[ci]);
    corner_power_hi = std::max(corner_power_hi, p);
    corner_power_lo = std::min(corner_power_lo, p);
  }

  const std::size_t cells = morph_steps * q_steps;
  std::vector<std::vector<double>> response(cells);
  std::vector<char> usable(cells, 0);
  std::vector<double> deviations;
  deviations.reserve(cells);
  std::vector<double> blend(npts, 0.0);
  std::vector<double> prefix(npts, 0.0);
  double power_hi = -std::numeric_limits<double>::infinity();
  double power_lo = std::numeric_limits<double>::infinity();
  double prefix_hi = -std::numeric_limits<double>::infinity();
  double prefix_lo = std::numeric_limits<double>::infinity();
  double step_sum = 0.0;
  std::size_t steps = 0;

  for (std::size_t qi = 0; qi < q_steps; ++qi) {
    const float qq = q_steps > 1 ? static_cast<float>(qi) / static_cast<float>(q_steps - 1) : 0.0F;
    std::vector<double> previous;
    for (std::size_t mi = 0; mi < morph_steps; ++mi) {
      const float m =
          morph_steps > 1 ? static_cast<float>(mi) / static_cast<float>(morph_steps - 1) : 0.0F;
      const auto biquads = at(m, qq);
      auto current = response_db(biquads, g);
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

      const double wm = static_cast<double>(m);
      const double wq = static_cast<double>(qq);
      const std::array<double, 4> weights{(1.0 - wm) * (1.0 - wq), wm * (1.0 - wq),
                                          (1.0 - wm) * wq, wm * wq};
      for (std::size_t i = 0; i < npts; ++i) {
        blend[i] = weights[0] * corner_db[0][i] + weights[1] * corner_db[1][i] +
                   weights[2] * corner_db[2][i] + weights[3] * corner_db[3][i];
        out.excursion_up_db = std::max(out.excursion_up_db, current[i] - corner_ceiling[i]);
        out.excursion_down_db = std::max(out.excursion_down_db, corner_floor[i] - current[i]);
      }
      const double deviation = std::sqrt(g.residual_var(blend, current, scratch));
      out.bilinear_dev_max_db = std::max(out.bilinear_dev_max_db, deviation);
      deviations.push_back(deviation);

      const double power = pink_power_db(pink, current);
      power_hi = std::max(power_hi, power);
      power_lo = std::min(power_lo, power);

      std::fill(prefix.begin(), prefix.end(), 0.0);
      for (const auto& biquad : biquads) {
        for (std::size_t i = 0; i < npts; ++i) {
          prefix[i] += stage_db(biquad, g.hz[i]);
          prefix_hi = std::max(prefix_hi, prefix[i]);
          prefix_lo = std::min(prefix_lo, prefix[i]);
        }
      }

      previous = current;
      const std::size_t cell = qi * morph_steps + mi;
      usable[cell] = 1;
      response[cell] = std::move(current);
    }
  }

  const auto walk = [&](const std::vector<std::size_t>& line) {
    std::size_t start = 0;
    while (start < line.size()) {
      if (usable[line[start]] == 0) {
        ++start;
        continue;
      }
      std::size_t stop = start;
      while (stop + 1 < line.size() && usable[line[stop + 1]] != 0) {
        ++stop;
      }
      if (stop > start) {
        double arc = 0.0;
        for (std::size_t k = start; k < stop; ++k) {
          arc += std::sqrt(g.residual_var(response[line[k]], response[line[k + 1]], scratch));
        }
        const double direct =
            std::sqrt(g.residual_var(response[line[start]], response[line[stop]], scratch));
        out.detour_max = std::max(out.detour_max, arc / (direct + 1e-3));
      }
      start = stop + 1;
    }
  };
  for (std::size_t qi = 0; qi < q_steps; ++qi) {
    std::vector<std::size_t> row(morph_steps);
    for (std::size_t mi = 0; mi < morph_steps; ++mi) {
      row[mi] = qi * morph_steps + mi;
    }
    walk(row);
  }
  for (std::size_t mi = 0; mi < morph_steps; ++mi) {
    std::vector<std::size_t> column(q_steps);
    for (std::size_t qi = 0; qi < q_steps; ++qi) {
      column[qi] = qi * morph_steps + mi;
    }
    walk(column);
  }

  out.mean_step_db = steps > 0 ? step_sum / static_cast<double>(steps) : 0.0;
  out.bilinear_dev_p95_db = percentile_of(deviations, 0.95);
  if (!deviations.empty()) {
    out.loudness_swing_db = power_hi - power_lo;
    out.loudness_beyond_corners_db = std::max(0.0, power_hi - corner_power_hi) +
                                     std::max(0.0, corner_power_lo - power_lo);
    out.prefix_headroom_db = prefix_hi;
    out.prefix_floor_db = prefix_lo;
  }
  return out;
}

}  // namespace trench::core::p2k
