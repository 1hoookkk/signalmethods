#include "trench/core/p2k.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <tuple>

#include "trench/core/morph.hpp"
#include "trench/core/packed_body.hpp"

namespace trench::core::p2k {

std::pair<Corner, double> polish_from_words(const CornerWords& words,
                                            std::span<const double> target,
                                            std::size_t max_passes, const Grid& g) {
  Corner c = Corner::from_words(words, g);
  Scratch s;
  const double var = polish(c, target, max_passes, s);
  return {std::move(c), std::sqrt(var)};
}

std::pair<Corner, double> polish_from_words_fine(const CornerWords& words,
                                                 std::span<const double> target,
                                                 std::size_t max_passes, const Grid& g) {
  Corner c = Corner::from_words(words, g);
  Scratch s;
  polish(c, target, max_passes, s);
  const double var = polish_fine(c, target, max_passes, s);
  return {std::move(c), std::sqrt(var)};
}

std::optional<P2kFit> fit_corner(std::span<const double> target, std::span<const Seed> seeds,
                                 const FitOptions& opts) {
  if (target.size() != kNpts) {
    throw std::invalid_argument("a P2K target is 512 log-spaced dB points");
  }
  std::optional<std::tuple<Corner, double, std::string_view>> best;
  const Grid& g = opts.grid ? *opts.grid : grid();

  for (const auto& seed : seeds) {
    CornerWords words{};
    std::string_view label;
    if (std::holds_alternative<SeedContinuous>(seed)) {
      if (!opts.allow_continuous) {
        continue;
      }
      const auto cont = continuous_best(target, g);
      if (!cont) {
        continue;
      }
      words = words_from_continuous(std::get<1>(*cont));
      label = "continuous";
    } else if (std::holds_alternative<SeedPeel>(seed)) {
      words = peel_seed(target, g);
      label = "peel";
    } else {
      words = std::get<CornerWords>(seed);
      label = "rom";
    }
    auto [c, rms] = polish_from_words_fine(enter(words), target, opts.max_passes, g);
    if (!best || rms < std::get<1>(*best)) {
      best.emplace(std::move(c), rms, label);
    }
  }

  if (!best) {
    return std::nullopt;
  }
  auto& [c, rms, label] = *best;
  const StageScales scales = stage_gain_pass(c);
  P2kFit fit;
  fit.words = c.w;
  fit.scales = scales;
  fit.packed = pack_corner(c, scales);
  fit.shape_rms_db = rms;
  fit.seed_used = label;
  return fit;
}

double stage_db(const std::array<double, 5>& biquad, double hz) {
  const double w = 2.0 * std::numbers::pi * hz / kSr;
  const double cw = std::cos(w);
  const double sw = std::sin(w);
  const double c2w = std::cos(2.0 * w);
  const double s2w = std::sin(2.0 * w);
  const auto [b0, b1, b2, a1, a2] = biquad;
  const double nr = b0 + b1 * cw + b2 * c2w;
  const double ni = -(b1 * sw + b2 * s2w);
  const double dr = 1.0 + a1 * cw + a2 * c2w;
  const double di = -(a1 * sw + a2 * s2w);
  const double n = std::sqrt(nr * nr + ni * ni);
  const double d = std::max(std::sqrt(dr * dr + di * di), 1e-12);
  return 20.0 * std::log10(std::max(n / d, 1e-12));
}

std::vector<double> corner_response_db(const StoredCorner& words, const Grid& g) {
  SectionBiquads biquads{};
  for (std::size_t si = 0; si < kStageCount; ++si) {
    biquads[si] = section_words_to_biquad(words[si]);
  }
  return response_db(biquads, g);
}

StoredCorner interpolate_plane(const std::array<StoredCorner, 4>& corners, float morph, float q) {
  const float m = std::clamp(morph, 0.0F, 1.0F);
  const float qq = std::clamp(q, 0.0F, 1.0F);
  StoredCorner out{};
  for (std::size_t si = 0; si < kStageCount; ++si) {
    for (std::size_t wi = 0; wi < kWordCount; ++wi) {
      const std::uint16_t edge0 = interpolate_word(corners[0][si][wi], corners[1][si][wi], m);
      const std::uint16_t edge1 = interpolate_word(corners[2][si][wi], corners[3][si][wi], m);
      out[si][wi] = interpolate_word(edge0, edge1, qq);
    }
  }
  return out;
}

std::array<StoredCorner, 4> body_corners(std::span<const std::uint8_t> body) {
  std::array<StoredCorner, 4> out{};
  for (std::size_t ci = 0; ci < 4; ++ci) {
    out[ci] = rom_corner_words(body, ci);
  }
  return out;
}

StoredCorner interpolate_body(std::span<const std::uint8_t> body, float morph, float q) {
  return interpolate_plane(body_corners(body), morph, q);
}

StoredCorner packed_as_words(const PackedCorner& packed) {
  StoredCorner out{};
  for (std::size_t si = 0; si < kStageCount; ++si) {
    for (std::size_t wi = 0; wi < kWordCount; ++wi) {
      out[si][wi] = packed[si * kWordCount + wi];
    }
  }
  return out;
}

}
