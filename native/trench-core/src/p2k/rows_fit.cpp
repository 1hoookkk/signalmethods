#include "trench/core/rows_fit.hpp"

#include <algorithm>
#include <cmath>
#include <vector>


namespace trench::core::p2k {

namespace {

double cost_of(const Rows& rows, const CornerWords& held, std::uint32_t mask,
               std::span<const double> target, const Grid& g, Scratch& s) {
  const Corner c = Corner::from_words(words_from_rows(rows, held, mask, kSr), g);
  return corner_cost(c, target, Cost::kWeightedVar, s);
}

double clamp_fc(double hz) { return std::clamp(hz, 20.0, kRootHiHz); }

}  // namespace

bool row_held(std::size_t section, std::uint32_t mask) {
  return (mask & pole_bit(section)) == 0U || (mask & zero_bit(section)) == 0U;
}

Rows rows_of_corner(const CornerWords& words, double sample_rate_hz) {
  Rows rows{};
  for (std::size_t si = 0; si < 6; ++si) {
    rows[si] = param_of({words[si][0], words[si][1], words[si][2], words[si][3], 0},
                        sample_rate_hz);
  }
  return rows;
}

CornerWords words_from_rows(const Rows& rows, const CornerWords& held, std::uint32_t mask,
                            double sample_rate_hz) {
  CornerWords out = held;
  for (std::size_t si = 0; si < 6; ++si) {
    if (rows[si].type == SectionType::kOff || row_held(si, mask)) continue;
    out[si] = words_from_param(rows[si], held[si], si, sample_rate_hz);
  }
  return out;
}

CornerWords words_from_rows(const Rows& rows, double sample_rate_hz) {
  return words_from_rows(rows, identity_words(), kAllFree, sample_rate_hz);
}

double rows_rms_db(const Rows& rows, std::span<const double> target, const Grid& g) {
  Scratch s;
  return std::sqrt(std::max(cost_of(rows, identity_words(), kAllFree, target, g, s), 0.0));
}

Rows seed_rows_from_target(std::span<const double> target, const Grid& g) {
  Rows rows{};
  for (auto& r : rows) r = {SectionType::kOff, 18000.0, 1.0, 0.0};
  struct Feature {
    double hz;
    double bw_oct;
    double prominence;
  };
  const std::size_t n = target.size();
  const auto extremum_at = [&](std::size_t i, int sign) {
    const double v = sign * target[i];
    return v > sign * target[i - 1] && v >= sign * target[i + 1] && v >= sign * target[i - 2] &&
           v >= sign * target[i + 2];
  };
  std::vector<Feature> features;
  for (int sign : {1, -1}) {
    for (std::size_t i = 2; i + 2 < n; ++i) {
      if (!extremum_at(i, sign)) continue;
      double left = sign * target[i];
      for (std::size_t k = i; k-- > 2;) {
        left = std::min(left, sign * target[k]);
        if (extremum_at(k, sign)) break;
      }
      double right = sign * target[i];
      for (std::size_t k = i + 1; k + 2 < n; ++k) {
        right = std::min(right, sign * target[k]);
        if (extremum_at(k, sign)) break;
      }
      const double prominence = sign * target[i] - std::max(left, right);
      std::size_t lo = i;
      while (lo > 0 && sign * target[lo] > sign * target[i] - 3.0) --lo;
      std::size_t hi = i;
      while (hi + 1 < n && sign * target[hi] > sign * target[i] - 3.0) ++hi;
      const double bw_hz = std::max(g.hz[hi] - g.hz[lo], 1.0);
      const double bw_oct =
          std::clamp(2.0 * std::asinh(bw_hz / (2.0 * g.hz[i])) / std::log(2.0), 0.05, 2.0);
      features.push_back({g.hz[i], bw_oct, sign * prominence});
    }
  }
  std::erase_if(features, [](const Feature& f) {
    return f.hz < 250.0 || f.hz > 8000.0 || std::abs(f.prominence) < 3.0;
  });
  std::sort(features.begin(), features.end(), [](const Feature& a, const Feature& b) {
    if ((a.prominence > 0.0) != (b.prominence > 0.0)) return a.prominence > 0.0;
    return std::abs(a.prominence) > std::abs(b.prominence);
  });
  if (features.size() > 4) features.resize(4);
  std::sort(features.begin(), features.end(),
            [](const Feature& a, const Feature& b) { return a.hz < b.hz; });
  std::size_t row = 1;
  for (const auto& f : features) {
    rows[row++] = {SectionType::kEq, clamp_fc(f.hz), f.bw_oct, f.prominence < 0.0 ? -12.0 : 12.0};
  }
  rows[0] = {SectionType::kHighPass, 10500.0, 0.05, 0.0};
  rows[5] = {SectionType::kLowPass, 225.0, 0.8, 0.0};
  return rows;
}

std::optional<RowsFit> fit_rows_watched(std::span<const double> target, Rows seed,
                                        const CornerWords& held, std::uint32_t mask,
                                        const RowsFitOptions& opts, const Grid& g,
                                        const std::function<bool()>& stop_requested,
                                        const std::function<void(const RowsStep&)>& on_step) {
  if (target.size() != kNpts) return std::nullopt;
  Scratch s;
  Rows rows = seed;
  double best = cost_of(rows, held, mask, target, g, s);
  bool stopped = false;

  const auto try_values = [&](std::size_t si, auto&& set, const std::vector<double>& values) {
    bool moved = false;
    for (const double v : values) {
      Rows trial = rows;
      set(trial[si], v);
      const double c = cost_of(trial, held, mask, target, g, s);
      if (c < best - 1e-9) {
        best = c;
        rows = trial;
        moved = true;
      }
    }
    return moved;
  };

  for (std::size_t pass = 0; pass < opts.max_passes && !stopped; ++pass) {
    bool changed = false;
    const double shrink = std::pow(0.5, static_cast<double>(pass));
    for (std::size_t si = 0; si < 6 && !stopped; ++si) {
      if (stop_requested && stop_requested()) {
        stopped = true;
        break;
      }
      if (rows[si].type == SectionType::kOff || row_held(si, mask)) continue;
      bool moved = false;
      {
        std::vector<double> v;
        const double span = opts.fc_span_oct * shrink;
        for (std::size_t k = 0; k < opts.steps; ++k) {
          const double t = -span + 2.0 * span * static_cast<double>(k) /
                                       static_cast<double>(opts.steps - 1);
          v.push_back(clamp_fc(rows[si].fc_hz * std::pow(2.0, t)));
        }
        moved |= try_values(si, [](SectionParam& p, double x) { p.fc_hz = x; }, v);
      }
      {
        std::vector<double> v;
        const double lo = std::log(opts.bw_lo_oct);
        const double hi = std::log(opts.bw_hi_oct);
        const double centre = std::log(std::clamp(rows[si].bw_oct, opts.bw_lo_oct, opts.bw_hi_oct));
        const double span = (hi - lo) * 0.5 * shrink;
        for (std::size_t k = 0; k < opts.steps; ++k) {
          const double t = centre - span + 2.0 * span * static_cast<double>(k) /
                                               static_cast<double>(opts.steps - 1);
          v.push_back(std::exp(std::clamp(t, lo, hi)));
        }
        moved |= try_values(si, [](SectionParam& p, double x) { p.bw_oct = x; }, v);
      }
      if (rows[si].type == SectionType::kEq) {
        std::vector<double> v;
        const double span = (opts.gain_hi_db - opts.gain_lo_db) * 0.5 * shrink;
        for (std::size_t k = 0; k < opts.steps; ++k) {
          const double t = rows[si].gain_db - span + 2.0 * span * static_cast<double>(k) /
                                                        static_cast<double>(opts.steps - 1);
          v.push_back(std::clamp(t, opts.gain_lo_db, opts.gain_hi_db));
        }
        moved |= try_values(si, [](SectionParam& p, double x) { p.gain_db = x; }, v);
      }
      if (moved && on_step) {
        on_step(RowsStep{si, rows, words_from_rows(rows, held, mask, kSr),
                         std::sqrt(std::max(best, 0.0))});
      }
      changed |= moved;
    }
    if (!changed) break;
  }

  RowsFit fit;
  fit.rows = rows;
  fit.words = words_from_rows(rows, held, mask, kSr);
  fit.rms_db = std::sqrt(std::max(best, 0.0));
  fit.stopped = stopped;
  return fit;
}

}  // namespace trench::core::p2k
