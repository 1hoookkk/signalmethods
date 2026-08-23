#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
};

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
  for (int mi = 0; mi < 9; ++mi) {
    for (int qi = 0; qi < 9; ++qi) {
      const float m = static_cast<float>(mi) / 8.0F;
      const float q = static_cast<float>(qi) / 8.0F;
      const double blended = (1.0 - m) * (1.0 - q) * corner_dc[0] + m * (1.0 - q) * corner_dc[1] +
                             (1.0 - m) * q * corner_dc[2] + m * q * corner_dc[3];
      const double landed = p2k::dc_gain_db(as_packed(p2k::interpolate_plane(corners, m, q)));
      out.dc_drift_db = std::max(out.dc_drift_db, std::abs(landed - blended));
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
  const auto render = [&](double q, auto morph_at) {
    trench::core::CascadeRunner runner;
    auto signal = noise;
    float peak = 0.0F;
    for (std::size_t off = 0; off < signal.size(); off += kBlock) {
      const std::size_t len = std::min(kBlock, signal.size() - off);
      const double m = morph_at(off / kSr);
      runner.set_target(
          nb::cascade(nb::blend(float_body, m, q, kSr), nb::blend_gain_db(float_body, m, q)));
      runner.process(std::span<float>(signal).subspan(off, len));
      for (std::size_t i = off; i < off + len; ++i) {
        if (!std::isfinite(signal[i])) ++out.non_finite;
        peak = std::max(peak, std::abs(signal[i]));
      }
    }
    return peak;
  };
  out.motion_db = -1.0e9;
  for (const double q : {0.0, 1.0}) {
    float frozen = 0.0F;
    for (int k = 0; k < 17; ++k) {
      const double m = k / 16.0;
      frozen = std::max(frozen, render(q, [m](double) { return m; }));
    }
    const float moving =
        render(q, [](double t) { return std::fmod(t * 20.0, 1.0) < 0.5 ? 0.0 : 1.0; });
    const double ratio =
        20.0 * std::log10(std::max(moving, 1.0e-15F) / std::max(frozen, 1.0e-15F));
    if (ratio > out.motion_db) {
      out.motion_db = ratio;
      out.frozen_peak = frozen;
      out.moving_peak = moving;
    }
  }
  return out;
}

}  // namespace trench::adversary
