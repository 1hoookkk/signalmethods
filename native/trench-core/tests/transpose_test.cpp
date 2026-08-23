#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

#include "trench/core/packed_body.hpp"
#include "trench/core/transpose.hpp"

namespace {

constexpr double kSr = trench::core::kP2kDatumHz;

trench::core::Biquad resonator(double hz, double radius, double zero_hz, double zero_radius) {
  const double w = 2.0 * std::numbers::pi * hz / kSr;
  const double wz = 2.0 * std::numbers::pi * zero_hz / kSr;
  return {1.0, -2.0 * zero_radius * std::cos(wz), zero_radius * zero_radius,
          -2.0 * radius * std::cos(w), radius * radius};
}

std::vector<std::uint8_t> hedz() {
  const std::filesystem::path path =
      std::string(TRENCH_SOURCE_ROOT) + "/ref/presets/P2k_013_talking_hedz.bin";
  std::ifstream in(path, std::ios::binary);
  return {(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(Transpose, PoleAndZeroMoveByTheRatioAndKeepTheirRadius) {
  const auto s = resonator(1000.0, 0.95, 3000.0, 0.6);
  const auto t = trench::core::transpose_section(s, 2.0, kSr);
  EXPECT_NEAR(*trench::core::conjugate_pair_hz(t[3], t[4], kSr), 2000.0, 1e-6);
  EXPECT_DOUBLE_EQ(t[4], s[4]);
  EXPECT_NEAR(*trench::core::conjugate_pair_hz(t[1] / t[0], t[2] / t[0], kSr), 6000.0, 1e-6);
  EXPECT_DOUBLE_EQ(t[2], s[2]);
}

TEST(Transpose, SubAnchorPoleAndZeroWallStayPut) {
  const auto anchor = resonator(64.0, 0.99, 3000.0, 0.5);
  const auto ta = trench::core::transpose_section(anchor, 2.0, kSr);
  EXPECT_DOUBLE_EQ(ta[3], anchor[3]);
  EXPECT_NE(ta[1], anchor[1]);
  const auto wall = resonator(1000.0, 0.9, 11506.0, 0.995);
  const auto tw = trench::core::transpose_section(wall, 2.0, kSr);
  EXPECT_NE(tw[3], wall[3]);
  EXPECT_DOUBLE_EQ(tw[1], wall[1]);
}

TEST(Transpose, UnityRatioAndSemitoneLawAreExact) {
  const auto s = resonator(700.0, 0.9, 700.0, 0.7);
  trench::core::Cascade c{};
  for (auto& section : c) section = s;
  EXPECT_EQ(trench::core::transpose_cascade(c, 1.0, kSr), c);
  EXPECT_DOUBLE_EQ(trench::core::ratio_of_semitones(12.0), 2.0);
  EXPECT_NEAR(trench::core::ratio_of_semitones(7.0), 1.4983070768766815, 1e-12);
}

TEST(Transpose, HedzKeepsItsSubAnchorWhenTunedUp) {
  const auto body = trench::core::PackedBody::from_body_bytes(hedz());
  const auto base = body.interpolate_biquads(0.0F, 0.0F, 0.0F);
  const auto up = trench::core::transpose_cascade(base, 2.0, kSr);
  for (std::size_t si = 0; si < base.size(); ++si) {
    const auto hz = trench::core::conjugate_pair_hz(base[si][3], base[si][4], kSr);
    if (!hz) continue;
    const auto moved = trench::core::conjugate_pair_hz(up[si][3], up[si][4], kSr);
    ASSERT_TRUE(moved.has_value());
    if (*hz < trench::core::kSubAnchorHz) {
      EXPECT_DOUBLE_EQ(*moved, *hz);
    } else {
      EXPECT_NEAR(*moved, std::min(*hz * 2.0, 0.49 * kSr), 1e-6);
    }
  }
}
