#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

namespace {

double shape_dc_db(const CornerWords& w) {
  double db = 0.0;
  for (std::size_t si = 0; si < kStageCount; ++si) {
    const auto [n, d] = dc_terms(w[si]);
    db += 20.0 * std::log10(std::max(std::abs(n), 1e-15) /
                            std::max(std::abs(d), 1e-15));
  }
  return db;
}

}  // namespace

std::optional<ZeroFit> fit_zeros_under(std::span<const double> ceiling_db,
                                       const CornerWords& start, std::uint32_t live,
                                       std::size_t max_passes, const Grid& g) {
  if (ceiling_db.size() != kNpts) return std::nullopt;
  auto c = Corner::from_words(enter(start), g);
  std::vector<double> model(kNpts);
  const auto cost_of = [&]() {
    c.total_into(model);
    const auto dc = shape_dc_db(c.w);
    double acc = 0.0;
    for (std::size_t i = 0; i < kNpts; ++i) {
      const auto over = model[i] - dc - ceiling_db[i];
      if (over > 0.0) acc += over * over;
    }
    return acc / static_cast<double>(kNpts);
  };
  double best = cost_of();
  const auto& lat = lattice_words();
  for (std::size_t pass = 0; pass < max_passes && best > 0.0; ++pass) {
    bool moved = false;
    for (std::size_t si = 0; si < kStageCount; ++si) {
      if ((live & (1U << si)) == 0U) continue;
      for (std::size_t wr = 0; wr < 2; ++wr) {
        auto keep = c.w[si][wr];
        for (std::size_t k = 0; k < lat.size(); ++k) {
          if (wr == 0 && !magnitude_admissible(k, false)) continue;
          const auto word = lat[k];
          if (word == keep) continue;
          c.w[si][wr] = word;
          const auto [p, q] = pq(c.w[si][0], c.w[si][1]);
          if (!is_legal(p, q, false)) continue;
          c.refresh(si);
          const auto cost = cost_of();
          if (cost < best - 1e-12) {
            best = cost;
            keep = word;
            moved = true;
          }
        }
        c.w[si][wr] = keep;
        c.refresh(si);
      }
      c.total_into(model);
      const auto dc = shape_dc_db(c.w);
      std::size_t worst = 0;
      double over = 0.0;
      for (std::size_t i = 0; i < kNpts; ++i) {
        const auto o = model[i] - dc - ceiling_db[i];
        if (o > over) {
          over = o;
          worst = i;
        }
      }
      if (over <= 0.0) continue;
      const auto peak_hz = g.hz[worst];
      auto keep0 = c.w[si][0];
      auto keep1 = c.w[si][1];
      for (int step = 0; step < 20; ++step) {
        const auto bw = 20.0 * std::pow(2.0, step * 0.45);
        const auto radius = std::exp(-std::numbers::pi * bw / kSr);
        const auto [zm, zr] = words_from_root(peak_hz, radius);
        c.w[si][0] = zm;
        c.w[si][1] = zr;
        const auto [p, q] = pq(zm, zr);
        if (!is_legal(p, q, false)) continue;
        c.refresh(si);
        const auto cost = cost_of();
        if (cost < best - 1e-12) {
          best = cost;
          keep0 = zm;
          keep1 = zr;
          moved = true;
        }
      }
      c.w[si][0] = keep0;
      c.w[si][1] = keep1;
      c.refresh(si);
    }
    if (!moved) break;
  }
  ZeroFit fit;
  fit.words = c.w;
  fit.scales = stage_gain_pass(c);
  fit.packed = pack_corner(c, fit.scales);
  c.total_into(model);
  const auto dc = shape_dc_db(c.w);
  fit.worst_over_db = 0.0;
  for (std::size_t i = 0; i < kNpts; ++i) {
    fit.worst_over_db = std::max(fit.worst_over_db, model[i] - dc - ceiling_db[i]);
  }
  return fit;
}

}  // namespace trench::core::p2k
