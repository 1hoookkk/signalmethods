#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "trench/core/formants.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/packed_body.hpp"

namespace p2k = trench::core::p2k;

namespace {

p2k::CornerWords skeleton_words(const char* name) {
  const auto* posture = p2k::posture(name);
  EXPECT_NE(posture, nullptr);
  p2k::CornerWords words{};
  for (auto& row : words) {
    row = {trench::core::kIdentitySection[0], trench::core::kIdentitySection[1],
           trench::core::kIdentitySection[2], trench::core::kIdentitySection[3]};
  }
  for (const auto& pole : p2k::pole_words_from_posture(*posture)) {
    words[pole.row][2] = pole.mag;
    words[pole.row][3] = pole.rsq;
  }
  return words;
}

double worst_over(const p2k::CornerWords& words, const std::vector<double>& ceiling) {
  auto corner = p2k::Corner::from_words(words);
  std::vector<double> model(p2k::kNpts);
  corner.total_into(model);
  double dc = 0.0;
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    const auto [n, d] = p2k::dc_terms(words[si]);
    dc += 20.0 * std::log10(std::max(std::abs(n), 1e-15) /
                            std::max(std::abs(d), 1e-15));
  }
  double worst = -1e9;
  for (std::size_t i = 0; i < p2k::kNpts; ++i) {
    worst = std::max(worst, model[i] - dc - ceiling[i]);
  }
  return worst;
}

}  // namespace

TEST(P2kZerosFit, ZerosTuckANakedSkeletonUnderAFlatCeiling) {
  const auto start = skeleton_words("REZ dead_ringer c1");
  const std::vector<double> ceiling(p2k::kNpts, 0.0);
  ASSERT_GT(worst_over(start, ceiling), 10.0);

  const auto fit = p2k::fit_zeros_under(ceiling, start, 0b111111U);
  ASSERT_TRUE(fit.has_value());
  EXPECT_LT(fit->worst_over_db, 1.5);
  for (std::size_t si = 0; si < p2k::kStageCount; ++si) {
    EXPECT_EQ(fit->words[si][2], start[si][2]) << si;
    EXPECT_EQ(fit->words[si][3], start[si][3]) << si;
  }
  EXPECT_LT(std::abs(p2k::dc_gain_db(fit->packed)), 0.2);
}

TEST(P2kZerosFit, AHeldSectionKeepsItsZeroWords)
{
  const auto start = skeleton_words("REZ dead_ringer c1");
  const std::vector<double> ceiling(p2k::kNpts, 0.0);
  const auto fit = p2k::fit_zeros_under(ceiling, start, 0b111110U);
  ASSERT_TRUE(fit.has_value());
  EXPECT_EQ(fit->words[0][0], start[0][0]);
  EXPECT_EQ(fit->words[0][1], start[0][1]);
}
