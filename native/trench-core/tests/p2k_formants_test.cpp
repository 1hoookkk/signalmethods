#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "trench/core/formants.hpp"
#include "trench/core/p2k.hpp"
#include "trench/core/section_param.hpp"

namespace p2k = trench::core::p2k;

TEST(P2kFormants, KlattTableIIIsPresentAndAVowelRoundTripsThroughRows) {
  ASSERT_EQ(p2k::klatt_vowels().size(), 12U);
  const auto* iy = p2k::klatt_vowel("iy");
  ASSERT_NE(iy, nullptr);
  const auto recipe = p2k::rows_from_formants(iy->f);
  const auto words = p2k::words_from_recipe(recipe);
  for (std::size_t row = 0; row < 3; ++row) {
    const trench::core::PackedSection packed{words[row][0], words[row][1], words[row][2],
                                             words[row][3], 0};
    const auto param = p2k::param_of(packed, p2k::kSr);
    EXPECT_EQ(param.type, p2k::SectionType::kEq) << "row " << row;
    EXPECT_NEAR(param.fc_hz, iy->f[row].hz, iy->f[row].hz * 0.05) << "row " << row;
  }
  const trench::core::PackedSection lp{words[5][0], words[5][1], words[5][2], words[5][3], 0};
  EXPECT_EQ(p2k::param_of(lp, p2k::kSr).type, p2k::SectionType::kLowPass);
}

TEST(P2kFormants, TheVowelCascadePeaksAtItsFormants) {
  const auto* aa = p2k::klatt_vowel("aa");
  ASSERT_NE(aa, nullptr);
  const auto words = p2k::words_from_recipe(p2k::rows_from_formants(aa->f));
  p2k::StoredCorner corner{};
  for (std::size_t si = 0; si < 6; ++si) {
    for (std::size_t wi = 0; wi < 4; ++wi) corner[si][wi] = words[si][wi];
    corner[si][4] = p2k::nearest_gain_word(0.25);
  }
  const auto response = p2k::corner_response_db(corner);
  const auto& hz = p2k::grid().hz;
  const auto peaks = p2k::peaks_of_envelope(hz, response, 6);
  for (const auto& f : aa->f) {
    bool hit = false;
    for (const auto& p : peaks) {
      hit = hit || std::abs(std::log2(p.hz / f.hz)) < 0.12;
    }
    EXPECT_TRUE(hit) << "no peak near " << f.hz;
  }
}

TEST(P2kFormants, TheManualParavowelsPeakWhereTheManualSaysAndTheCombsNotchAtOctaves) {
  ASSERT_EQ(p2k::manual_recipes().size(), 8U);
  const auto response_of = [](const p2k::FormantRecipe& recipe) {
    const auto words = p2k::words_from_recipe(recipe);
    p2k::StoredCorner corner{};
    for (std::size_t si = 0; si < 6; ++si) {
      for (std::size_t wi = 0; wi < 4; ++wi) corner[si][wi] = words[si][wi];
      corner[si][4] = p2k::nearest_gain_word(0.25);
    }
    return p2k::corner_response_db(corner);
  };
  const auto& hz = p2k::grid().hz;
  const auto* a = p2k::manual_recipe("para A");
  ASSERT_NE(a, nullptr);
  const auto peaks = p2k::peaks_of_envelope(hz, response_of(a->recipe), 6);
  for (const double f : {800.0, 1150.0, 2800.0, 3500.0, 4950.0}) {
    bool hit = false;
    for (const auto& p : peaks) hit = hit || std::abs(std::log2(p.hz / f)) < 0.12;
    EXPECT_TRUE(hit) << "no peak near " << f;
  }
  const auto* comb = p2k::manual_recipe("comb 8ve");
  ASSERT_NE(comb, nullptr);
  const auto response = response_of(comb->recipe);
  const auto db_at = [&](double f) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < hz.size(); ++i) {
      if (std::abs(std::log(hz[i] / f)) < std::abs(std::log(hz[best] / f))) best = i;
    }
    return response[best];
  };
  for (const double f : {100.0, 200.0, 400.0, 800.0, 1600.0}) {
    EXPECT_LT(db_at(f), db_at(f * std::sqrt(2.0)) - 12.0) << "no notch at " << f;
  }
  EXPECT_EQ(p2k::manual_recipe("nope"), nullptr);
}

TEST(P2kFormants, PeaksOfAnEnvelopeAreSortedByFrequencyAndCapped) {
  std::vector<double> hz;
  std::vector<double> db;
  for (int i = 0; i < 400; ++i) {
    const double f = 50.0 * std::pow(400.0, i / 399.0);
    hz.push_back(f);
    double v = -0.002 * f;
    for (const double c : {300.0, 1200.0, 2500.0}) {
      v += 20.0 * std::exp(-std::pow(std::log(f / c) / 0.08, 2.0));
    }
    db.push_back(v);
  }
  const auto peaks = p2k::peaks_of_envelope(hz, db, 2);
  ASSERT_EQ(peaks.size(), 2U);
  EXPECT_LT(peaks[0].hz, peaks[1].hz);
}
