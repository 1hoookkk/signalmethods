#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

#include "trench/core/audition.hpp"
#include "trench/core/morph.hpp"
#include "trench/core/native_body.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

namespace trench::adversary {

namespace p2k = trench::core::p2k;
namespace nb = trench::core::native;

struct RootState {
  double hz{};
  double r{};
  double bw_oct{};
};

struct ByteGrade {
  double max_step_db{};
  double excursion_up_db{};
  double loudness_beyond_corners_db{};
  std::size_t refused{};
  double dc_drift_db{};
  double motion_db{};
  double frozen_peak{};
  double moving_peak{};
  std::size_t non_finite{};
  std::size_t pole_over_ceiling{};
  std::size_t off_lattice{};
  std::size_t motion_culprit{};
  double motion_reduced_peak_db{};
  std::array<std::size_t, 2> motion_corners{};
  std::array<RootState, 2> motion_roots{};
  std::size_t snap_corner{};
  std::size_t snap_culprit{};
  std::array<double, nb::kSections> snap_loss_db{};
  std::size_t dc_culprit{};
  bool dc_crossed{};
  double dc_change{};
};

inline double octaves_of(double hz, double bw_hz) {
  const double x = bw_hz / std::max(hz, 1.0e-9);
  return 2.0 * std::log2((x + std::sqrt(x * x + 4.0)) / 2.0);
}

inline RootState root_state(const nb::Roots& roots, double sample_rate_hz) {
  if (const auto* res = std::get_if<nb::Resonant>(&roots)) {
    return {res->hz, std::exp(-std::numbers::pi * res->bw_hz / sample_rate_hz),
            octaves_of(res->hz, res->bw_hz)};
  }
  const auto [a1, a2] = nb::coefficients_of(roots, sample_rate_hz);
  return {a1 > 0.0 ? 0.5 * sample_rate_hz : 0.0, std::sqrt(std::abs(a2)), 0.0};
}

inline const std::vector<float>& motion_noise() {
  static const std::vector<float> noise = [] {
    constexpr double kSr = 48'000.0;
    std::vector<float> out(48'000);
    std::uint32_t seed = 0x1BADF00D;
    double y = 0.0;
    const double alpha = 1.0 - std::exp(-2.0 * std::numbers::pi * 500.0 / kSr);
    float peak = 0.0F;
    for (auto& v : out) {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      const double white = (seed / 4294967295.0) * 2.0 - 1.0;
      y += alpha * (white - y);
      v = static_cast<float>(y);
      peak = std::max(peak, std::abs(v));
    }
    for (auto& v : out) v = v / peak * 0.25F;
    return out;
  }();
  return noise;
}

inline p2k::PackedCorner as_packed(const p2k::StoredCorner& words) {
  p2k::PackedCorner out{};
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    for (std::size_t wi = 0; wi < p2k::kWordCount; ++wi) {
      out[si * p2k::kWordCount + wi] = words[si][wi];
    }
  }
  return out;
}

inline ByteGrade grade_bytes(std::span<const std::uint8_t> body) {
  ByteGrade out{};

  const auto audit = p2k::interior_audit(body);
  out.max_step_db = audit.max_step_db;
  out.excursion_up_db = audit.excursion_up_db;
  out.loudness_beyond_corners_db = audit.loudness_beyond_corners_db;
  out.refused = audit.refused;

  const auto corners = p2k::body_corners(body);
  std::array<double, 4> corner_dc{};
  for (std::size_t ci = 0; ci < 4; ++ci) {
    corner_dc[ci] = p2k::dc_gain_db(as_packed(corners[ci]));
  }
  float drift_q = 0.0F;
  for (int mi = 0; mi < 9; ++mi) {
    for (int qi = 0; qi < 9; ++qi) {
      const float m = static_cast<float>(mi) / 8.0F;
      const float q = static_cast<float>(qi) / 8.0F;
      const double blended = (1.0 - m) * (1.0 - q) * corner_dc[0] + m * (1.0 - q) * corner_dc[1] +
                             (1.0 - m) * q * corner_dc[2] + m * q * corner_dc[3];
      const double landed = p2k::dc_gain_db(as_packed(p2k::interpolate_plane(corners, m, q)));
      const double drift = std::abs(landed - blended);
      if (drift > out.dc_drift_db) {
        out.dc_drift_db = drift;
        drift_q = q;
      }
    }
  }

  constexpr std::size_t kLerpSteps = 64;
  std::array<std::array<double, kLerpSteps + 1>, p2k::kStageCount> lane_dc{};
  for (std::size_t k = 0; k <= kLerpSteps; ++k) {
    const auto words = p2k::interpolate_plane(
        corners, static_cast<float>(k) / static_cast<float>(kLerpSteps), drift_q);
    for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
      const auto [n, d] = p2k::dc_terms(
          p2k::StageWords{words[si][0], words[si][1], words[si][2], words[si][3]});
      lane_dc[si][k] = 4.0 * trench::core::decode_word(words[si][4]) * n / d;
    }
  }
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    bool crossed = std::abs(lane_dc[si][0]) <= 1.0e-6;
    for (std::size_t k = 1; k <= kLerpSteps; ++k) {
      crossed = crossed || std::abs(lane_dc[si][k]) <= 1.0e-6 ||
                (lane_dc[si][k] < 0.0) != (lane_dc[si][k - 1] < 0.0);
    }
    const double change = std::abs(lane_dc[si][kLerpSteps] - lane_dc[si][0]);
    if (crossed && !out.dc_crossed) {
      out.dc_crossed = true;
      out.dc_culprit = si;
      out.dc_change = change;
    } else if (!out.dc_crossed && change > out.dc_change) {
      out.dc_culprit = si;
      out.dc_change = change;
    }
  }

  const auto& lattice = p2k::lattice_words();
  for (const auto& corner : corners) {
    for (const auto& stage : corner) {
      for (std::size_t wi = 0; wi < 4; ++wi) {
        if (!std::binary_search(lattice.begin(), lattice.end(), stage[wi])) ++out.off_lattice;
      }
      const auto [p, q] = p2k::pq(stage[2], stage[3]);
      if (p2k::pair_radius(p, q) > p2k::pole_radius_ceiling()) ++out.pole_over_ceiling;
    }
  }

  constexpr double kSr = 48'000.0;
  constexpr std::size_t kBlock = 512;
  const auto& noise = motion_noise();
  const auto float_body = nb::import_p2k(body);
  const auto render = [&](double q, auto morph_at, std::size_t drop) {
    trench::core::CascadeRunner runner;
    auto signal = noise;
    float peak = 0.0F;
    for (std::size_t off = 0; off < signal.size(); off += kBlock) {
      const std::size_t len = std::min(kBlock, signal.size() - off);
      const double m = morph_at(off / kSr);
      auto plan = nb::blend(float_body, m, q, kSr);
      if (drop < nb::kSections) plan[drop] = nb::Coefficients{0.0, 0.0, 0.0, 0.0, true};
      runner.set_target(nb::cascade(plan, nb::blend_gain_db(float_body, m, q)));
      runner.process(std::span<float>(signal).subspan(off, len));
      for (std::size_t i = off; i < off + len; ++i) {
        if (drop >= nb::kSections && !std::isfinite(signal[i])) ++out.non_finite;
        peak = std::max(peak, std::abs(signal[i]));
      }
    }
    return peak;
  };
  const auto square = [](double t) { return std::fmod(t * 20.0, 1.0) < 0.5 ? 0.0 : 1.0; };
  out.motion_db = -1.0e9;
  double graded_q = 0.0;
  for (const double q : {0.0, 1.0}) {
    float frozen = 0.0F;
    for (int k = 0; k < 17; ++k) {
      const double m = k / 16.0;
      frozen = std::max(frozen, render(q, [m](double) { return m; }, nb::kSections));
    }
    const float moving = render(q, square, nb::kSections);
    const double ratio =
        20.0 * std::log10(std::max(moving, 1.0e-15F) / std::max(frozen, 1.0e-15F));
    if (ratio > out.motion_db) {
      out.motion_db = ratio;
      out.frozen_peak = frozen;
      out.moving_peak = moving;
      graded_q = q;
    }
  }

  float reduced = std::numeric_limits<float>::infinity();
  for (std::size_t si = 0; si < nb::kSections; ++si) {
    const float peak = render(graded_q, square, si);
    if (peak < reduced) {
      reduced = peak;
      out.motion_culprit = si;
    }
  }
  out.motion_reduced_peak_db = 20.0 * std::log10(std::max<double>(reduced, 1.0e-15));
  out.motion_corners = {graded_q > 0.5 ? 2U : 0U, graded_q > 0.5 ? 3U : 1U};
  for (std::size_t e = 0; e < 2; ++e) {
    out.motion_roots[e] =
        root_state(float_body.corners[out.motion_corners[e]].sections[out.motion_culprit].pole, kSr);
  }
  return out;
}

inline void attribute_snap(const nb::Corner& corner, const p2k::PackedCorner& packed,
                           std::size_t corner_index, ByteGrade& out) {
  const auto& g = p2k::grid();
  const auto plan = nb::design(corner, p2k::kSr);
  const auto words = p2k::packed_as_words(packed);
  out.snap_corner = corner_index;
  out.snap_culprit = 0;
  std::vector<double> diff(g.hz.size());
  for (std::size_t si = 0; si < nb::kSections; ++si) {
    const auto want = nb::biquad(plan[si]);
    const auto got = trench::core::section_words_to_biquad(words[si]);
    double acc = 0.0;
    for (std::size_t i = 0; i < diff.size(); ++i) {
      diff[i] = p2k::stage_db(want, g.hz[i]) - p2k::stage_db(got, g.hz[i]);
      acc += diff[i];
    }
    const double mean = acc / static_cast<double>(diff.size());
    double worst = 0.0;
    for (const double d : diff) worst = std::max(worst, std::abs(d - mean));
    out.snap_loss_db[si] = worst;
    if (worst > out.snap_loss_db[out.snap_culprit]) out.snap_culprit = si;
  }
}

}  // namespace trench::adversary
