#include "trench/core/p2k.hpp"

#include <algorithm>
#include <cmath>

#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

std::pair<double, double> dc_terms(const StageWords& w) {
  const auto [zp, zq] = pq(w[0], w[1]);
  const auto [pp, ppq] = pq(w[2], w[3]);
  return {1.0 + zp + zq, 1.0 + pp + ppq};
}

StageScales stage_gain_pass(const Corner& c) {
  double ratio = 1.0;
  for (std::size_t si = 0; si < kStageCount; ++si) {
    auto [n, d] = dc_terms(c.w[si]);
    if (std::abs(n) < 1e-15) {
      n = std::copysign(1e-15, n == 0.0 ? 1.0 : n);
    }
    ratio *= d / n;
  }
  StageScales out;
  out.fill(std::pow(std::abs(ratio), 1.0 / static_cast<double>(kStageCount)));
  return out;
}

void write_dc_unity_scales(StoredCorner& rows) {
  double ratio = 1.0;
  for (const auto& row : rows) {
    auto [n, d] = dc_terms({row[0], row[1], row[2], row[3]});
    if (std::abs(n) < 1e-15) {
      n = std::copysign(1e-15, n == 0.0 ? 1.0 : n);
    }
    ratio *= d / n;
  }
  const auto scale =
      std::pow(std::abs(ratio), 1.0 / static_cast<double>(kStageCount));
  for (auto& row : rows) {
    row[4] = nearest_gain_word(scale);
  }
}

StageScales stage_gain_pass_held(const Corner& c, std::uint32_t mask,
                                 const PackedCorner& baseline) {
  double ratio = 1.0;
  for (std::size_t si = 0; si < kStageCount; ++si) {
    auto [n, d] = dc_terms(c.w[si]);
    if (std::abs(n) < 1e-15) {
      n = std::copysign(1e-15, n == 0.0 ? 1.0 : n);
    }
    ratio *= d / n;
  }
  double held_product = 1.0;
  std::size_t free_count = 0;
  for (std::size_t si = 0; si < kStageCount; ++si) {
    if (scale_free(mask, si)) {
      ++free_count;
    } else {
      held_product *= 4.0 * decode_word(baseline[si * kWordCount + 4]);
    }
  }
  StageScales out{};
  const double free_scale =
      free_count == 0
          ? 1.0
          : std::pow(std::abs(ratio) / std::max(held_product, 1e-30),
                     1.0 / static_cast<double>(free_count));
  for (std::size_t si = 0; si < kStageCount; ++si) {
    out[si] = scale_free(mask, si) ? free_scale
                                   : 4.0 * decode_word(baseline[si * kWordCount + 4]);
  }
  return out;
}

PackedCorner pack_corner(const Corner& c, const StageScales& scales) {
  PackedCorner out{};
  for (std::size_t si = 0; si < kStageCount; ++si) {
    for (std::size_t wi = 0; wi < 4; ++wi) {
      out[si * kWordCount + wi] = c.w[si][wi];
    }
    out[si * kWordCount + 4] = nearest_gain_word(scales[si]);
  }
  return out;
}

std::array<std::uint8_t, 240> pack_body(const std::array<PackedCorner, 4>& corners) {
  std::array<std::uint8_t, 240> out{};
  std::size_t k = 0;
  for (const auto& corner : corners) {
    for (const auto w : corner) {
      out[k] = static_cast<std::uint8_t>(w & 0xFFU);
      out[k + 1] = static_cast<std::uint8_t>(w >> 8U);
      k += 2;
    }
  }
  return out;
}

double dc_gain_db(const PackedCorner& corner) {
  double h = 1.0;
  for (std::size_t si = 0; si < kStageCount; ++si) {
    const StageWords w{corner[si * kWordCount], corner[si * kWordCount + 1],
                       corner[si * kWordCount + 2], corner[si * kWordCount + 3]};
    const auto [n, d] = dc_terms(w);
    const double scale = 4.0 * decode_word(corner[si * kWordCount + 4]);
    h *= scale * n / d;
  }
  return 20.0 * std::log10(std::max(std::abs(h), 1e-30));
}

}  // namespace trench::core::p2k
