#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "trench/core/p2k.hpp"

namespace p2k = trench::core::p2k;

namespace {

std::vector<double> jagged_target() {
  std::vector<double> t(p2k::kNpts, 0.0);
  for (std::size_t i = 0; i < p2k::kNpts; ++i) {
    t[i] = i % 2 == 0 ? 6.0 : -6.0;
  }
  return t;
}

}  // namespace

TEST(PerceptualSpace, TheDefaultSpaceIsTheParityGridBitForBit) {
  const auto made = p2k::make_grid(p2k::PerceptualSpace{});
  const auto& ref = p2k::grid();
  for (std::size_t i = 0; i < p2k::kNpts; ++i) {
    ASSERT_EQ(made.hz[i], ref.hz[i]);
    ASSERT_EQ(made.weight[i], ref.weight[i]);
    ASSERT_EQ(made.z1r[i], ref.z1r[i]);
    ASSERT_EQ(made.z2i[i], ref.z2i[i]);
  }
  ASSERT_EQ(made.weight_sum, ref.weight_sum);
  ASSERT_EQ(made.smooth_bins, 0U);
}

TEST(PerceptualSpace, BandLimitsMoveTheGridAndWeightsStillSumToN) {
  p2k::PerceptualSpace space;
  space.lo_hz = 100.0;
  space.hi_hz = 8000.0;
  const auto g = p2k::make_grid(space);
  ASSERT_NEAR(g.hz.front(), 100.0, 1e-9);
  ASSERT_NEAR(g.hz.back(), 8000.0, 1e-9);
  ASSERT_NEAR(g.weight_sum, static_cast<double>(p2k::kNpts), 1e-9);
}

TEST(PerceptualSpace, FlatWeightIsUniform) {
  p2k::PerceptualSpace space;
  space.weight = p2k::PerceptualSpace::Weight::kFlat;
  const auto g = p2k::make_grid(space);
  for (const double w : g.weight) {
    ASSERT_NEAR(w, 1.0, 1e-12);
  }
}

TEST(PerceptualSpace, AnEmphasisBandRaisesItsPointsRelativeToTheRest) {
  p2k::PerceptualSpace space;
  space.emphasis.push_back({1000.0, 2000.0, 4.0});
  const auto g = p2k::make_grid(space);
  const auto& ref = p2k::grid();
  double inside = 0.0;
  double outside = 0.0;
  for (std::size_t i = 0; i < p2k::kNpts; ++i) {
    const double ratio = g.weight[i] / ref.weight[i];
    if (g.hz[i] >= 1000.0 && g.hz[i] <= 2000.0) {
      inside = ratio;
    } else {
      outside = ratio;
    }
  }
  ASSERT_NEAR(inside / outside, 4.0, 1e-9);
}

TEST(PerceptualSpace, SmoothingLowersTheVarianceOfAJaggedResidual) {
  const auto t = jagged_target();
  std::vector<double> scratch(p2k::kNpts, 0.0);
  const double raw = p2k::grid().weighted_var(t, scratch);
  p2k::PerceptualSpace space;
  space.smooth_octaves = 0.5;
  const auto g = p2k::make_grid(space);
  ASSERT_GT(g.smooth_bins, 4U);
  const double smooth = g.weighted_var(t, scratch);
  ASSERT_LT(smooth, raw * 0.05);
}

TEST(PerceptualSpace, ACornerCarriesItsGridIntoTheFit) {
  p2k::PerceptualSpace space;
  space.lo_hz = 200.0;
  space.hi_hz = 6000.0;
  const auto g = p2k::make_grid(space);
  const auto corner = p2k::Corner::from_words(p2k::identity_words(), g);
  ASSERT_EQ(&corner.grid(), &g);
  for (std::size_t si = 0; si + 1 < p2k::kStageCount; ++si) {
    const auto n = corner.num(si);
    const auto d = corner.den(si);
    for (std::size_t i = 0; i < p2k::kNpts; ++i) {
      ASSERT_NEAR(n[i], d[i], 1e-12);
    }
  }
}
