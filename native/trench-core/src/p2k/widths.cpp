#include "trench/core/p2k.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace trench::core::p2k {

namespace {

double angle_hz(double p, double q) {
  const double denom = 2.0 * std::sqrt(std::max(q, 0.0));
  if (denom <= 1e-12) {
    return -1.0;
  }
  const double cos_w = -p / denom;
  if (!(cos_w >= -1.0 && cos_w <= 1.0)) {
    return -1.0;
  }
  return std::acos(cos_w) * kSr / (2.0 * std::numbers::pi);
}

double word_hz(std::uint16_t mag, std::uint16_t rsq) {
  const auto [p, q] = pq(mag, rsq);
  return angle_hz(p, q);
}

double hz_quantum(std::size_t i_mag, std::uint16_t rsq) {
  const auto& words = lattice_words();
  const double here = word_hz(words[i_mag], rsq);
  if (here < 0.0) {
    return -1.0;
  }
  double span = 0.0;
  if (i_mag > 0) {
    const double lo = word_hz(words[i_mag - 1], rsq);
    if (lo >= 0.0) {
      span = std::max(span, std::abs(lo - here));
    }
  }
  if (i_mag + 1 < words.size()) {
    const double hi = word_hz(words[i_mag + 1], rsq);
    if (hi >= 0.0) {
      span = std::max(span, std::abs(hi - here));
    }
  }
  return 0.5 * span;
}

std::pair<double, bool> sweep_width(Corner& c, std::span<const double> target, std::size_t si,
                                    double held_hz, double best, Scratch& s) {
  const auto& lat = lattice_decoded();
  const auto& words = lattice_words();
  std::uint16_t best_mag = c.w[si][2];
  std::uint16_t best_rsq = c.w[si][3];
  double best_v = best;
  bool moved = false;

  for (std::size_t k = 0; k < lattice_len(); ++k) {
    const double q_rung = 1.0 - lat[k];
    if (q_rung <= 1e-12) {
      continue;
    }
    const auto [mag, rsq] = words_from_root(held_hz, std::sqrt(q_rung));
    if (rsq != words[k]) {
      continue;
    }
    const std::size_t i_mag = nearest_lattice_word(mag);
    if (!magnitude_admissible(i_mag, true)) {
      continue;
    }
    const auto [p, q] = pq(mag, rsq);
    if (!is_legal(p, q, true)) {
      continue;
    }
    const double got = angle_hz(p, q);
    const double quantum = hz_quantum(i_mag, rsq);
    if (got < 0.0 || quantum < 0.0) {
      continue;
    }
    const double drift = std::abs(got - held_hz);
    if (drift > quantum + 1e-9 || drift > kHeldHzTolerance * held_hz) {
      continue;
    }
    c.w[si][2] = mag;
    c.w[si][3] = rsq;
    c.refresh(si);
    const double v = corner_cost(c, target, Cost::kWeightedVar, s);
    if (v < best_v - 1e-12) {
      best_v = v;
      best_mag = mag;
      best_rsq = rsq;
      moved = true;
    }
  }

  c.w[si][2] = best_mag;
  c.w[si][3] = best_rsq;
  c.refresh(si);
  return {best_v, moved};
}

}  // namespace

std::optional<WidthFit> fit_pole_widths(std::span<const double> target, const CornerWords& words,
                                        std::uint32_t live, std::size_t max_passes,
                                        const Grid& g) {
  if (target.size() != kNpts) {
    return std::nullopt;
  }
  Corner c = Corner::from_words(words, g);
  Scratch s;

  std::array<double, kStageCount> held{};
  std::array<bool, kStageCount> solve{};
  for (std::size_t si = 0; si < kStageCount; ++si) {
    held[si] = word_hz(c.w[si][2], c.w[si][3]);
    solve[si] = pole_free(live, si) && held[si] > 0.0;
  }

  double best = corner_var(c, target, s);
  for (std::size_t pass = 0; pass < max_passes; ++pass) {
    bool changed = false;
    for (std::size_t si = 0; si < kStageCount; ++si) {
      if (!solve[si]) {
        continue;
      }
      const auto [v, ch] = sweep_width(c, target, si, held[si], best, s);
      best = v;
      changed |= ch;
    }
    if (!changed) {
      break;
    }
  }

  WidthFit fit;
  fit.words = c.w;
  fit.scales = stage_gain_pass(c);
  fit.packed = pack_corner(c, fit.scales);
  fit.shape_rms_db = std::sqrt(std::max(best, 0.0));
  for (std::size_t si = 0; si < kStageCount; ++si) {
    const auto [p, q] = pq(c.w[si][2], c.w[si][3]);
    const double r = pair_radius(p, q);
    fit.solved[si] = solve[si];
    fit.pole_hz[si] = angle_hz(p, q);
    fit.pole_radius[si] = r;
    fit.pole_bw_hz[si] = -std::log(std::max(r, 1e-9)) * kSr / std::numbers::pi;
  }
  return fit;
}

}  // namespace trench::core::p2k
