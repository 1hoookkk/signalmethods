#include "trench/core/rows_fit.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "trench/core/formants.hpp"

namespace trench::core::p2k {

namespace {

double cost_of(const Rows& rows, std::span<const double> target, const Grid& g, Scratch& s,
               double sample_rate_hz) {
  const auto words = words_from_rows(rows, sample_rate_hz);
  const Corner c = Corner::from_words(words, g);
  return corner_cost(c, target, Cost::kWeightedVar, s);
}

bool row_live(const SectionParam& p) { return p.type != SectionType::kOff; }

double clamp_fc(double hz) { return std::clamp(hz, 20.0, kRootHiHz); }

}  // namespace

CornerWords words_from_rows(const Rows& rows, double sample_rate_hz) {
  CornerWords out = identity_words();
  for (std::size_t si = 0; si < 6; ++si) {
    out[si] = words_from_param(rows[si], out[si], si, sample_rate_hz);
  }
  return out;
}

double rows_rms_db(const Rows& rows, std::span<const double> target, const Grid& g) {
  Scratch s;
  return std::sqrt(std::max(cost_of(rows, target, g, s, kSr), 0.0));
}

Rows seed_rows_from_target(std::span<const double> target, const Grid& g) {
  Rows rows{};
  for (auto& r : rows) r = {SectionType::kOff, 18000.0, 1.0, 0.0};
  auto peaks = peaks_of_envelope(g.hz, target, 12);
  std::erase_if(peaks, [](const SpectralPeak& p) { return p.hz < 250.0 || p.hz > 8000.0; });
  std::sort(peaks.begin(), peaks.end(),
            [](const SpectralPeak& a, const SpectralPeak& b) { return a.db > b.db; });
  if (peaks.size() > 4) peaks.resize(4);
  std::sort(peaks.begin(), peaks.end(),
            [](const SpectralPeak& a, const SpectralPeak& b) { return a.hz < b.hz; });
  std::size_t row = 1;
  for (const auto& p : peaks) {
    const double bw_oct = std::clamp(2.0 * std::asinh(p.bw_hz / (2.0 * p.hz)) / std::log(2.0),
                                     0.05, 2.0);
    rows[row++] = {SectionType::kEq, clamp_fc(p.hz), bw_oct, 12.0};
  }
  rows[0] = {SectionType::kHighPass, 10500.0, 0.05, 0.0};
  rows[5] = {SectionType::kLowPass, 225.0, 0.8, 0.0};
  return rows;
}

std::optional<RowsFit> fit_rows_watched(std::span<const double> target, Rows seed,
                                        const RowsFitOptions& opts, const Grid& g,
                                        const std::function<bool()>& stop_requested,
                                        const std::function<void(const RowsStep&)>& on_step) {
  if (target.size() != kNpts) return std::nullopt;
  Scratch s;
  Rows rows = seed;
  double best = cost_of(rows, target, g, s, kSr);
  bool stopped = false;

  const auto try_values = [&](std::size_t si, auto&& set, const std::vector<double>& values) {
    bool moved = false;
    for (const double v : values) {
      Rows trial = rows;
      set(trial[si], v);
      const double c = cost_of(trial, target, g, s, kSr);
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
      if (!row_live(rows[si])) continue;
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
        on_step(RowsStep{si, rows, words_from_rows(rows, kSr), std::sqrt(std::max(best, 0.0))});
      }
      changed |= moved;
    }
    if (!changed) break;
  }

  RowsFit fit;
  fit.rows = rows;
  fit.words = words_from_rows(rows, kSr);
  fit.rms_db = std::sqrt(std::max(best, 0.0));
  fit.stopped = stopped;
  return fit;
}

}  // namespace trench::core::p2k
