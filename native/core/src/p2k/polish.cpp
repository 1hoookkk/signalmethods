#include "trench/core/p2k.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

namespace {

void load_base(Scratch& s, const Corner& c, std::size_t si) {
  c.total_into(s.total);
  const auto n = c.num(si);
  const auto d = c.den(si);
  for (std::size_t i = 0; i < kNpts; ++i) {
    s.base[i] = s.total[i] - (n[i] - d[i]);
  }
}

}

bool intent_admits(const RoleIntent& intent, std::size_t si, const StageWords& candidate) {
  if (!intent[si]) {
    return true;
  }
  const PackedSection words{candidate[0], candidate[1], candidate[2], candidate[3], 0};
  return within_envelope(*intent[si], words, kSr);
}

double residual_var(std::span<const double> target, std::span<const double> model, Scratch& s) {
  for (std::size_t i = 0; i < kNpts; ++i) {
    s.resid[i] = target[i] - model[i];
  }
  return grid().weighted_var(s.resid, s.tmp);
}

double corner_var(const Corner& c, std::span<const double> target, Scratch& s) {
  return corner_cost(c, target, Cost::kWeightedVar, s);
}

double corner_cost(const Corner& c, std::span<const double> target, Cost cost, Scratch& s,
                   const LossFn* loss) {
  c.total_into(s.total);
  if (loss) {
    return (*loss)(target, s.total);
  }
  for (std::size_t i = 0; i < kNpts; ++i) {
    s.resid[i] = target[i] - s.total[i];
  }
  return c.grid().score(s.resid, cost, s.tmp);
}

std::pair<double, bool> sweep_axis(Corner& c, std::span<const double> target, std::size_t si,
                                   std::size_t wi, double best, Scratch& s) {
  return sweep_axis_cost(c, target, si, wi, best, Cost::kWeightedVar, s);
}

std::pair<double, bool> sweep_axis_cost(Corner& c, std::span<const double> target, std::size_t si,
                                        std::size_t wi, double best, Cost cost, Scratch& s,
                                        const LossFn* loss) {
  const Grid& g = c.grid();
  load_base(s, c, si);
  const bool sweep_is_mag = wi % 2 == 0;
  const bool is_pole = wi >= 2;
  const double d_partner = decode_word(c.w[si][wi ^ 1U]);
  const auto& lat = lattice_decoded();

  double best_v = std::numeric_limits<double>::infinity();
  std::size_t best_k = SIZE_MAX;
  for (std::size_t k = 0; k < lattice_len(); ++k) {
    const double d = lat[k];
    const double p = sweep_is_mag ? 4.0 * d + d_partner - 2.0 : 4.0 * d_partner + d - 2.0;
    const double q = sweep_is_mag ? 1.0 - d_partner : 1.0 - d;
    if (sweep_is_mag && !magnitude_admissible(k, is_pole)) {
      continue;
    }
    if (!is_legal(p, q, is_pole)) {
      continue;
    }
    if (c.intent[si]) {
      StageWords trial = c.w[si];
      trial[wi] = lattice_words()[k];
      if (!intent_admits(c.intent, si, trial)) {
        continue;
      }
    }
    g.factor_db(p, q, s.bank);
    const auto n = c.num(si);
    const auto d_stage = c.den(si);
    for (std::size_t i = 0; i < kNpts; ++i) {
      const double cand = wi < 2 ? s.base[i] + s.bank[i] - d_stage[i]
                                 : s.base[i] + n[i] - s.bank[i];
      if (loss) {
        s.cand[i] = cand;
      } else {
        s.resid[i] = target[i] - cand;
      }
    }
    const double v = loss ? (*loss)(target, s.cand) : g.score(s.resid, cost, s.tmp);
    if (v < best_v) {
      best_v = v;
      best_k = k;
    }
  }

  if (best_k == SIZE_MAX) {
    return {best, false};
  }
  if (best_v < best - 1e-12) {
    c.w[si][wi] = lattice_words()[best_k];
    c.refresh(si);
    return {best_v, true};
  }
  return {best, false};
}

std::pair<double, bool> sweep_radius(Corner& c, std::span<const double> target, std::size_t si,
                                     std::size_t root, double best, Scratch& s) {
  return sweep_radius_cost(c, target, si, root, best, Cost::kWeightedVar, s);
}

std::pair<double, bool> sweep_radius_cost(Corner& c, std::span<const double> target, std::size_t si,
                                          std::size_t root, double best, Cost cost, Scratch& s,
                                          const LossFn* loss) {
  const Grid& g = c.grid();
  const std::size_t wm = root == 0 ? 0 : 2;
  const std::size_t wr = root == 0 ? 1 : 3;
  const auto [p0, q0] = pq(c.w[si][wm], c.w[si][wr]);
  if (q0 <= 1e-12) {
    return {best, false};
  }
  const double cos_w = -p0 / (2.0 * std::sqrt(q0));
  if (!(cos_w >= -1.0 && cos_w <= 1.0)) {
    return {best, false};
  }
  load_base(s, c, si);
  const auto& lat = lattice_decoded();

  double best_v = std::numeric_limits<double>::infinity();
  std::size_t best_k = SIZE_MAX;
  std::size_t best_mag = 0;
  for (std::size_t k = 0; k < lattice_len(); ++k) {
    const double d_rsq = lat[k];
    const double q = 1.0 - d_rsq;
    if (q <= 1e-12) {
      continue;
    }
    const double p = -2.0 * std::sqrt(std::max(q, 0.0)) * cos_w;
    std::size_t mag_byte = nearest_lattice((p + 2.0 - d_rsq) / 4.0);
    while (mag_byte > 0 && !magnitude_admissible(mag_byte, root == 1)) {
      --mag_byte;
    }
    const double p_q = 4.0 * lat[mag_byte] + d_rsq - 2.0;
    if (!is_legal(p_q, q, root == 1)) {
      continue;
    }
    if (c.intent[si]) {
      StageWords trial = c.w[si];
      trial[wm] = lattice_words()[mag_byte];
      trial[wr] = si == 5 && wr == 1 ? kS6ZeroRsqWord : lattice_words()[k];
      if (!intent_admits(c.intent, si, trial)) {
        continue;
      }
    }
    g.factor_db(p_q, q, s.bank);
    const auto n = c.num(si);
    const auto d_stage = c.den(si);
    for (std::size_t i = 0; i < kNpts; ++i) {
      const double cand = root == 0 ? s.base[i] + (s.bank[i] - d_stage[i])
                                    : s.base[i] + (n[i] - s.bank[i]);
      if (loss) {
        s.cand[i] = cand;
      } else {
        s.resid[i] = target[i] - cand;
      }
    }
    const double v = loss ? (*loss)(target, s.cand) : g.score(s.resid, cost, s.tmp);
    if (v < best_v) {
      best_v = v;
      best_k = k;
      best_mag = mag_byte;
    }
  }

  if (best_k == SIZE_MAX) {
    return {best, false};
  }
  if (best_v < best - 1e-12) {
    c.w[si][wm] = lattice_words()[best_mag];
    c.w[si][wr] = lattice_words()[best_k];
    if (si == 5 && wr == 1) {
      c.w[si][wr] = kS6ZeroRsqWord;
    }
    c.refresh(si);
    return {best_v, true};
  }
  return {best, false};
}

std::pair<double, bool> stage_moves(Corner& c, std::span<const double> target, std::size_t si,
                                    double best, Scratch& s) {
  return stage_moves_cost(c, target, si, best, Cost::kWeightedVar, s);
}

std::pair<double, bool> stage_moves_cost(Corner& c, std::span<const double> target, std::size_t si,
                                         double best, Cost cost, Scratch& s, const LossFn* loss) {
  bool moved = false;
  for (std::size_t wi = 0; wi < 4; ++wi) {
    if (si == 5 && wi == 1) {
      continue;
    }
    const auto [v, ch] = sweep_axis_cost(c, target, si, wi, best, cost, s, loss);
    best = v;
    moved |= ch;
  }
  for (std::size_t root = 0; root < 2; ++root) {
    if (si == 5 && root == 0) {
      continue;
    }
    const auto [v, ch] = sweep_radius_cost(c, target, si, root, best, cost, s, loss);
    best = v;
    moved |= ch;
  }
  return {best, moved};
}

double polish(Corner& c, std::span<const double> target, std::size_t max_passes, Scratch& s) {
  double best = corner_var(c, target, s);
  for (std::size_t si = 0; si < kStageCount; ++si) {
    for (std::size_t pass = 0; pass < 4; ++pass) {
      const auto [v, moved] = stage_moves(c, target, si, best, s);
      best = v;
      if (!moved) {
        break;
      }
    }
  }
  for (std::size_t pass = 0; pass < max_passes; ++pass) {
    bool changed = false;
    for (std::size_t si = 0; si < kStageCount; ++si) {
      const auto [v, ch] = stage_moves(c, target, si, best, s);
      best = v;
      changed |= ch;
    }
    if (!changed) {
      break;
    }
  }
  return best;
}

std::pair<double, bool> sweep_axis_fine(Corner& c, std::span<const double> target, std::size_t si,
                                        std::size_t wi, double best, Scratch& s) {
  return sweep_axis_fine_cost(c, target, si, wi, best, Cost::kWeightedVar, s);
}

std::pair<double, bool> sweep_axis_fine_cost(Corner& c, std::span<const double> target,
                                             std::size_t si, std::size_t wi, double best, Cost cost,
                                             Scratch& s, const LossFn* loss) {
  const Grid& g = c.grid();
  load_base(s, c, si);
  const bool is_pole = wi >= 2;
  const double d_partner = decode_word(c.w[si][wi ^ 1U]);
  const bool sweep_is_mag = wi % 2 == 0;
  const std::int32_t centre = c.w[si][wi];

  double best_v = std::numeric_limits<double>::infinity();
  std::uint16_t best_w = 0;
  for (std::int32_t cand = std::max(centre - kFineSpan, 0);
       cand <= std::min(centre + kFineSpan, 0xFFFF); ++cand) {
    const auto word = static_cast<std::uint16_t>(cand);
    if (sweep_is_mag && is_pole && static_cast<std::size_t>(word >> 8U) > kMaxMagByte) {
      continue;
    }
    const double d = decode_word(word);
    const double p = sweep_is_mag ? 4.0 * d + d_partner - 2.0 : 4.0 * d_partner + d - 2.0;
    const double q = sweep_is_mag ? 1.0 - d_partner : 1.0 - d;
    if (!is_legal(p, q, is_pole)) {
      continue;
    }
    if (c.intent[si]) {
      StageWords trial = c.w[si];
      trial[wi] = word;
      if (!intent_admits(c.intent, si, trial)) {
        continue;
      }
    }
    g.factor_db(p, q, s.bank);
    const auto n = c.num(si);
    const auto d_stage = c.den(si);
    for (std::size_t i = 0; i < kNpts; ++i) {
      const double cand_db = wi < 2 ? s.base[i] + s.bank[i] - d_stage[i]
                                    : s.base[i] + n[i] - s.bank[i];
      if (loss) {
        s.cand[i] = cand_db;
      } else {
        s.resid[i] = target[i] - cand_db;
      }
    }
    const double v = loss ? (*loss)(target, s.cand) : g.score(s.resid, cost, s.tmp);
    if (v < best_v) {
      best_v = v;
      best_w = word;
    }
  }

  if (std::isinf(best_v)) {
    return {best, false};
  }
  if (best_v < best - 1e-12) {
    c.w[si][wi] = best_w;
    c.refresh(si);
    return {best_v, true};
  }
  return {best, false};
}

double polish_fine(Corner& c, std::span<const double> target, std::size_t max_passes, Scratch& s) {
  double best = corner_var(c, target, s);
  for (std::size_t pass = 0; pass < max_passes; ++pass) {
    bool changed = false;
    for (std::size_t si = 0; si < kStageCount; ++si) {
      for (std::size_t wi = 0; wi < 4; ++wi) {
        if (si == 5 && wi == 1) {
          continue;
        }
        const auto [v, ch] = sweep_axis_fine(c, target, si, wi, best, s);
        best = v;
        changed |= ch;
      }
      for (std::size_t root = 0; root < 2; ++root) {
        if (si == 5 && root == 0) {
          continue;
        }
        const auto [v, ch] = sweep_radius(c, target, si, root, best, s);
        best = v;
        changed |= ch;
      }
    }
    if (!changed) {
      break;
    }
  }
  return best;
}

}
